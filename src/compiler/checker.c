//
// See Copyright Notice In elf.h
//

#include <stdarg.h>

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "compiler.h"
#include "entity.h"
#include "ast.h"
#include "ir.h"
#include "ir_emitter.h"

#include "checker.h"

typedef struct
{
	Scope    *globals;
	Scope    *source_scope;
	Scope    *active_scope;
	Ast       current_function;
}
Checker;

static Com_Status check_statement(Checker *checker, Ast ast);
static StaticValue resolve_entity(Checker *checker, Entity *entity);
static StaticValue resolve_expression(Checker *checker, Ast expr);

static StaticValue static_value_error(Com_Status status)
{
	ASSERT(status != COM_STATUS_OK);
	StaticValue value = {.type = STATIC_VALUE_ERROR, .status = status};
	return value;
}

static Com_Status static_value_status(StaticValue value)
{
	return value.type == STATIC_VALUE_ERROR ? value.status : COM_STATUS_OK;
}

static Com_Status checker_report(Com_Source *source, ErrorCode error, ErrorSeverity severity, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 reported = compiler_reportv(source, error, severity, ERROR_PHASE_CHECKER, site, format, args);
	va_end(args);
	return reported ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Com_Status checker_out_of_memory(Com_Source *source, SourceSite site)
{
	return compiler_report(source, ERROR_OUT_OF_MEMORY, ERROR_FATAL, ERROR_PHASE_CHECKER, site, "out of memory during semantic analysis") ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Com_Status push_scope(Checker *checker, ScopeKind kind, Ast owner, Scope **result)
{
	ASSERT(checker);
	ASSERT(checker->active_scope);
	ASSERT(owner);
	ASSERT(result);
	ASSERT(checker->source_scope);
	ASSERT(checker->active_scope->source == checker->source_scope->source);
	Scope *scope = scope_create(checker->active_scope->arena, kind, checker->active_scope, owner, checker->source_scope->source);
	if (scope) {
		checker->active_scope = scope;
		*result = scope;
		return COM_STATUS_OK;
	}
	*result = 0;
	return checker_out_of_memory(checker->source_scope->source, owner->site);
}

static void pop_scope(Checker *checker, Scope *scope)
{
	ASSERT(scope);
	ASSERT(checker->active_scope == scope);
	checker->active_scope = scope->parent;
}

static Entity *declare_entity(Checker *checker, Scope *scope, EntityKind kind, Ast tree, Ast expr, EntityTags tags, Atom *name, SourceSite site)
{
	Entity *existing = scope_find_current(scope, name);
	if (existing)
	{
		ErrorCode error = kind == ENTITY_STATIC ? ERROR_DUPLICATE_STATIC_DECLARATION : ERROR_DUPLICATE_DECLARATION;
		Com_Status status = checker_report(scope->source, error, ERROR_ERROR, site, "'%s' is already declared in this scope", atom_data(name));
		return entity_from_status(status);
	}

	Entity *entity = scope_declare(scope, kind, name);
	if (!entity)
	{
		return entity_from_status(checker_out_of_memory(scope->source, site));
	}

	entity->state = kind == ENTITY_STATIC ? ENTITY_UNRESOLVED : ENTITY_RESOLVED;
	entity->tree = tree;
	entity->expr = expr;

	if (kind == ENTITY_GLOBAL) {
		entity->parent = 0;
	}
	else if (scope->kind == SCOPE_SOURCE) {
		entity->parent = scope->owner;
	}
	else {
		entity->parent = checker->current_function;
	}

	entity->tags = tags;
	return entity;
}

static Com_Status declare_statics(Checker *checker, Scope *scope, Ast_Array stats)
{
	for (u32 i = 0; i < stats.count; ++ i) {
		Ast ast_decl = stats.items[i];
		if (ast_decl->kind != AST_STATIC_DECL) {
			continue;
		}
		Ast ast_name = ast_decl->static_decl.name;
		Ast ast_expr = ast_decl->static_decl.expr;
		ASSERT(ast_name);
		ASSERT(ast_name->kind == AST_IDENT);
		ASSERT(ast_name->atom);
		ASSERT(ast_expr);
		ASSERT(!ast_name->entity);
		ASSERT(!ast_decl->entity);

		EntityTags tags = ENTITY_TAG_CONSTANT;
		if (ast_decl->static_decl.exported) {
			tags |= ENTITY_TAG_EXPORTED;
		}
		Entity *entity = declare_entity(checker, scope, ENTITY_STATIC, ast_decl, ast_expr, tags, ast_name->atom, ast_decl->site);
		if (entity_is_error(entity)) {
			return entity->status;
		}
		ast_name->entity = entity;
		ast_decl->entity = entity;
	}
	return COM_STATUS_OK;
}

Com_Status check_source(Com_Source *source)
{
	ASSERT(source);
	Compiler *compiler = source->compiler;
	ASSERT(compiler);
	Ast ast_file = source->ast;
	ASSERT(ast_file);
	ASSERT(ast_file->kind == AST_FILE);
	ASSERT(ast_file->file.body);
	ASSERT(ast_file->file.body->kind == AST_BLOCK_STAT);

	Checker checker = {0};
	checker.current_function = ast_file;
	checker.globals = scope_create(compiler->arena, SCOPE_GLOBAL, 0, 0, 0);
	if (!checker.globals) {
		return checker_out_of_memory(source, ast_file->site);
	}

	checker.source_scope = scope_create(compiler->arena, SCOPE_SOURCE, checker.globals, ast_file, source);
	checker.active_scope = checker.source_scope;
	if (!checker.source_scope) {
		return checker_out_of_memory(source, ast_file->site);
	}

	ASSERT(!source->scope);
	source->scope = checker.source_scope;

	Com_Status status = declare_statics(&checker, checker.source_scope, ast_file->file.body->block);
	for (u32 i = 0; status == COM_STATUS_OK && i < ast_file->file.body->block.count; ++i) {
		status = check_statement(&checker, ast_file->file.body->block.items[i]);
	}
	return status;
}

static StaticValue resolve_entity(Checker *checker, Entity *entity)
{
	ASSERT(entity);
	ASSERT(entity->kind == ENTITY_STATIC);
	if (entity->state == ENTITY_RESOLVING)
	{
		Com_Status status = checker_report(entity->scope->source, ERROR_CYCLIC_STATIC_DEFINITION, ERROR_ERROR, entity->tree->site, "cyclic static definition involving '%s'", atom_data(entity->name));
		return static_value_error(status);
	}
	if (entity->state == ENTITY_RESOLVED)
	{
		return entity->value;
	}
	ASSERT(entity->expr || !"Error: internal error, unresolved entity missing expression to resolve");
	entity->state = ENTITY_RESOLVING;

	// Note, we can lazily resolve entities from different sources
	Scope *previous_source_scope = checker->source_scope;
	Scope *previous_active_scope = checker->active_scope;

	Com_Source *source = entity->scope->source;
	ASSERT(source);
	ASSERT(source->scope);
	checker->source_scope = source->scope;
	checker->active_scope = entity->scope;

	StaticValue value = resolve_expression(checker, entity->expr);

	checker->source_scope = previous_source_scope;
	checker->active_scope = previous_active_scope;

	if (value.type == STATIC_VALUE_NONE)
	{
		Com_Status status = checker_report(source, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, entity->expr->site, "static definition '%s' does not resolve to a static value", atom_data(entity->name));
		value = static_value_error(status);
	}
	entity->state = ENTITY_RESOLVED;
	entity->value = value;
	return value;
}

static Com_Status resolve_source_scope_lazily(Checker *checker, Com_Source *source, Scope **result)
{
	Compiler *compiler = source->compiler;
	ASSERT(result);
	*result = 0;
	ASSERT(source);
	ASSERT(source->name);
	ASSERT(source->path);
	ASSERT(source->ast);
	ASSERT(source->ast->kind == AST_FILE);
	ASSERT(source->ast->file.body->kind == AST_BLOCK_STAT);
	if (source->scope)
	{
		*result = source->scope;
		return COM_STATUS_OK;
	}
	source->scope = scope_create(compiler->arena, SCOPE_SOURCE, checker->globals, source->ast, source);
	if (!source->scope)
	{
		return checker_out_of_memory(source, source->ast->site);
	}
	Com_Status status = declare_statics(checker, source->scope, source->ast->file.body->block);
	if (status == COM_STATUS_OK) {
		*result = source->scope;
	}
	return status;
}

static i32 find_parameter_by_name(Ast_Array params, Atom *name)
{
	for (u32 i = 0; i < params.count; ++i)
	{
		Ast param = params.items[i];
		ASSERT(param);
		ASSERT(param->kind == AST_FUNCTION_PARAM);
		ASSERT(param->param.name);
		ASSERT(param->param.name->kind == AST_IDENT);
		ASSERT(param->param.name->identifier);
		if (param->param.name->identifier == name) {
			return i;
		}
	}
	return -1;
}

// TOOD(RJ): imagine writing an interpreter for an interpreter ...
static StaticValue resolve_expression(Checker *checker, Ast expr)
{
	StaticValue value = {STATIC_VALUE_NONE};
	ASSERT(expr);

	switch (expr->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		case AST_RECURSE:
		case AST_ELLIPSIS:
		{
		}
		break;

		case AST_NIL_LITERAL:
		{
			value.type = STATIC_VALUE_NIL;
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			value.type = STATIC_VALUE_INTEGER;
			value.integer = expr->integer_value;
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			value.type = STATIC_VALUE_NUMBER;
			value.number = expr->number_value;
		}
		break;

		case AST_STRING_LITERAL:
		{
			value.type = STATIC_VALUE_STRING;
			value.string = expr->string_value;
		}
		break;

		case AST_FUNCTION:
		{
			Scope *scope = 0;
			Com_Status status = push_scope(checker, SCOPE_FUNCTION, expr, &scope);
			if (status != COM_STATUS_OK) {
				value = static_value_error(status);
				break;
			}
			Ast outer_function = checker->current_function;
			checker->current_function = expr;
			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < expr->function.params.count; ++i)
			{
				Ast param = expr->function.params.items[i];
				ASSERT(param);
				ASSERT(param->kind == AST_FUNCTION_PARAM);
				if (param->param.type)
				{
					StaticValue param_value = resolve_expression(checker, param->param.type);
					if (param_value.type == STATIC_VALUE_ERROR) {
						value = param_value;
					}
				}
				if (value.type != STATIC_VALUE_ERROR && param->param.expr)
				{
					StaticValue param_value = resolve_expression(checker, param->param.expr);
					if (param_value.type == STATIC_VALUE_ERROR) {
						value = param_value;
					}
				}
				if (value.type == STATIC_VALUE_ERROR) {
					break;
				}
				Ast name = param->param.name;
				ASSERT(name);
				ASSERT(name->kind == AST_IDENT);
				ASSERT(name->atom);
				ASSERT(!name->entity);
				Entity *entity = declare_entity(checker, checker->active_scope, ENTITY_PARAMETER, param, 0, ENTITY_TAG_PARAMETER, name->identifier, name->site);
				if (entity_is_error(entity)) {
					value = static_value_error(entity->status);
				} else {
					name->entity = entity;
				}
			}
			if (value.type != STATIC_VALUE_ERROR) {
				status = check_statement(checker, expr->function.body);
				if (status != COM_STATUS_OK) {
					value = static_value_error(status);
				}
			}
			checker->current_function = outer_function;
			pop_scope(checker, scope);

			if (value.type != STATIC_VALUE_ERROR) {
				value.type = STATIC_VALUE_FUNCTION;
				value.function = expr;
			}
		}
		break;

		case AST_IMPORT:
		{
			if (!expr->import.target)
			{
				Com_Status status = checker_report(checker->source_scope->source, ERROR_IMPORT, ERROR_ERROR, expr->site, "import '%s' was not resolved to a source", atom_data(expr->import.path));
				value = static_value_error(status);
			}
			else
			{
				value.type = STATIC_VALUE_EXPORTS;
				Com_Status status = resolve_source_scope_lazily(checker, expr->import.target, &value.exports);
				if (status != COM_STATUS_OK) {
					value = static_value_error(status);
				}
			}
		}
		break;

		case AST_IDENT:
		{
			Atom *name = expr->identifier;
			Entity *entity = scope_find(checker->active_scope, name);
			if (entity) {
				ASSERT(entity->kind == ENTITY_STATIC || entity->state == ENTITY_RESOLVED);
			} else {
				entity = declare_entity(checker, checker->globals, ENTITY_GLOBAL, 0, 0, ENTITY_TAG_NONE, name, expr->site);
				if (entity_is_error(entity)) {
					value = static_value_error(entity->status);
				}
			}
			if (value.type != STATIC_VALUE_ERROR)
			{
				expr->entity = entity;
				if (entity->kind == ENTITY_STATIC) {
					value = resolve_entity(checker, entity);
				} else {
					value = entity->value;
				}
			}
		}
		break;

		case AST_FIELD:
		{
			ASSERT(expr->binary.x);
			ASSERT(expr->binary.y);
			StaticValue left_value = resolve_expression(checker, expr->binary.x);
			if (left_value.type == STATIC_VALUE_ERROR)
			{
				value = left_value;
			}
			else if (left_value.type == STATIC_VALUE_EXPORTS)
			{
				ASSERT(expr->binary.y);
				ASSERT(expr->binary.y->kind == AST_IDENT || expr->binary.y->kind == AST_STRING_LITERAL);
				ASSERT(expr->binary.y->atom);
				Entity *entity = scope_find_current(left_value.exports, expr->binary.y->atom);
				if (!entity)
				{
					Com_Status status = checker_report(checker->source_scope->source, ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "source '%s' has no static declaration named '%s'", atom_data(left_value.exports->source->name), atom_data(expr->binary.y->atom));
					value = static_value_error(status);
				}
				else if (!(entity->tags & ENTITY_TAG_EXPORTED))
				{
					Com_Status status = checker_report(checker->source_scope->source, ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "static declaration '%s' is private to source '%s'", atom_data(entity->name), atom_data(left_value.exports->source->name));
					value = static_value_error(status);
				}
				else
				{
					value = resolve_entity(checker, entity);
					if (value.type != STATIC_VALUE_ERROR) {
						entity->users += 1;
						entity->tags |= ENTITY_TAG_REFERENCED;
						expr->entity = entity;
					}
				}
			}
		}
		break;

		case AST_BITWISE_NOT:
		case AST_MEASURE:
		case AST_GET_MEM:
		{
			ASSERT(expr->unary);
			StaticValue operand = resolve_expression(checker, expr->unary);
			if (operand.type == STATIC_VALUE_ERROR) {
				value = operand;
			}
		}
		break;

		case AST_AND:
		case AST_OR:
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_POW:
		case AST_MOD:
		case AST_EQ:
		case AST_NOT_EQ:
		case AST_LESS_THAN:
		case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN:
		case AST_GREATER_THAN_EQ:
		case AST_NIL_AND:
		case AST_NIL_OR:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		case AST_BITWISE_XOR:
		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_MOD_ASSIGN:
		case AST_XOR_ASSIGN:
		case AST_SHL_ASSIGN:
		case AST_SHR_ASSIGN:
		case AST_NIL_ASSIGN:
		case AST_ASSIGN:
		case AST_INDEX:
		case AST_COMMA_EXPR:
		case AST_RANGE_INDEX:
		{
			ASSERT(expr->binary.x);
			ASSERT(expr->binary.y);
			StaticValue operand = resolve_expression(checker, expr->binary.x);
			if (operand.type != STATIC_VALUE_ERROR) {
				operand = resolve_expression(checker, expr->binary.y);
			}
			if (operand.type == STATIC_VALUE_ERROR) {
				value = operand;
			}
		}
		break;

		case AST_RANGE:
		{
			StaticValue operand = {STATIC_VALUE_NONE};
			if (expr->binary.x)
			{
				operand = resolve_expression(checker, expr->binary.x);
			}
			if (operand.type != STATIC_VALUE_ERROR && expr->binary.y)
			{
				operand = resolve_expression(checker, expr->binary.y);
			}
			if (operand.type == STATIC_VALUE_ERROR) {
				value = operand;
			}
		}
		break;

		case AST_META_FIELD:
		{
			ASSERT(expr->binary.x);
			ASSERT(expr->binary.y);
			StaticValue operand = resolve_expression(checker, expr->binary.x);
			if (operand.type == STATIC_VALUE_ERROR) {
				value = operand;
			}
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < expr->interpolated_string.count; ++i)
			{
				StaticValue item = resolve_expression(checker, expr->interpolated_string.items[i]);
				if (item.type == STATIC_VALUE_ERROR) {
					value = item;
				}
			}
		}
		break;

		case AST_CALL:
		{
			StaticValue target = resolve_expression(checker, expr->call.expr);
			if (target.type == STATIC_VALUE_ERROR) {
				value = target;
				break;
			}

			Ast_Array args = expr->call.args;

			u32 bind_slot = 0;
			b32 require_named_arguments = 0;

			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < args.count; ++i)
			{
				ASSERT(args.items[i]);
				Ast argument = args.items[i];
				Ast argument_expr = argument;
				u32 argument_slot = bind_slot;

				if (argument->kind == AST_NAMED_ARGUMENT)
				{
					require_named_arguments = true;

					Ast argument_name = argument->named_argument.name;
					ASSERT(argument_name);
					ASSERT(argument_name->kind == AST_IDENT);
					ASSERT(argument_name->identifier != 0);
					ASSERT(argument->named_argument.expr);

					if (target.type != STATIC_VALUE_FUNCTION)
					{
						Com_Status status = checker_report(checker->source_scope->source, ERROR_NAMED_ARGUMENT_TARGET, ERROR_ERROR, argument->site, "named arguments require a statically known function");
						value = static_value_error(status);
						break;
					}
					ASSERT(target.function);

					i32 named_slot = find_parameter_by_name(target.function->function.params, argument_name->identifier);
					if (named_slot < 0)
					{
						Com_Status status = checker_report(checker->source_scope->source, ERROR_UNKNOWN_NAMED_ARGUMENT, ERROR_ERROR, argument_name->site, "function has no parameter named '%s'", atom_data(argument_name->identifier));
						value = static_value_error(status);
						break;
					}
					argument_slot = (u32)named_slot;

					b32 duplicate = false;
					for (u32 prior_index = 0; !duplicate && prior_index < i; ++ prior_index) {
						duplicate = args.items[prior_index]->param_slot == argument_slot;
					}
					if (duplicate)
					{
						Com_Status status = checker_report(checker->source_scope->source, ERROR_DUPLICATE_ARGUMENT, ERROR_ERROR, argument->site, "parameter '%s' was specified more than once", atom_data(argument_name->identifier));
						value = static_value_error(status);
						break;
					}
					if (argument_slot < bind_slot)
					{
						Com_Status status = checker_report(checker->source_scope->source, ERROR_ARGUMENT_ORDER, ERROR_ERROR, argument->site, "named argument '%s' does not follow parameter order", atom_data(argument_name->identifier));
						value = static_value_error(status);
						break;
					}
					argument_expr = argument->named_argument.expr;
				}
				else
				{
					if (require_named_arguments)
					{
						Com_Status status = checker_report(checker->source_scope->source, ERROR_ARGUMENT_ORDER, ERROR_ERROR, argument->site, "positional arguments must come before named arguments");
						value = static_value_error(status);
						break;
					}
				}

				StaticValue item = resolve_expression(checker, argument_expr);
				if (item.type == STATIC_VALUE_ERROR) {
					value = item;
					break;
				}

				argument->param_slot = argument_slot;
				bind_slot = argument_slot + 1;
			}
		}
		break;

		case AST_TUPLE:
		{
			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < expr->tuple.count; ++i)
			{
				StaticValue item = resolve_expression(checker, expr->tuple.items[i]);
				if (item.type == STATIC_VALUE_ERROR) {
					value = item;
				}
			}
		}
		break;

		case AST_TABLE:
		{
			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < expr->table.count; ++i)
			{
				StaticValue item = resolve_expression(checker, expr->table.items[i]);
				if (item.type == STATIC_VALUE_ERROR) {
					value = item;
				}
			}
		}
		break;

		case AST_ARRAY:
		{
			for (u32 i = 0; value.type != STATIC_VALUE_ERROR && i < expr->array.count; ++i)
			{
				StaticValue item = resolve_expression(checker, expr->array.items[i]);
				if (item.type == STATIC_VALUE_ERROR) {
					value = item;
				}
			}
		}
		break;

		case AST_TABLE_ENTRY:
		{
			ASSERT(expr->table_entry.key);
			ASSERT(expr->table_entry.value);
			StaticValue item = resolve_expression(checker, expr->table_entry.key);
			if (item.type != STATIC_VALUE_ERROR) {
				item = resolve_expression(checker, expr->table_entry.value);
			}
			if (item.type == STATIC_VALUE_ERROR) {
				value = item;
			}
		}
		break;

		default:
		{
			Com_Status status = checker_report(checker->source_scope->source, ERROR_INTERNAL, ERROR_ERROR, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			value = static_value_error(status);
		}
		break;
	}
	return value;
}

static Com_Status check_statement(Checker *checker, Ast stat)
{
	ASSERT(stat);
	ASSERT(stat->kind != AST_FILE);
	ASSERT(checker->active_scope);
	ASSERT(checker->source_scope);
	Com_Status status = COM_STATUS_OK;
	StaticValue value = {0};

	switch (stat->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		{
		}
		break;

		case AST_BLOCK_STAT:
		{
			Scope *scope = 0;
			status = push_scope(checker, SCOPE_BLOCK, stat, &scope);
			if (status == COM_STATUS_OK)
			{
				status = declare_statics(checker, scope, stat->block);
				for (u32 i = 0; status == COM_STATUS_OK && i < stat->block.count; ++i) {
					status = check_statement(checker, stat->block.items[i]);
				}
				pop_scope(checker, scope);
			}
		}
		break;

		case AST_DECL_STAT:
		{
			Ast names = ast_ensure_kind(stat->decl.name, AST_TUPLE);
			EntityTags tags = stat->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
			if (stat->decl.type)
			{
				value = resolve_expression(checker, stat->decl.type);
				status = static_value_status(value);
			}
			ASSERT(stat->decl.expr);
			if (status == COM_STATUS_OK)
			{
				value = resolve_expression(checker, stat->decl.expr);
				status = static_value_status(value);
			}
			for (u32 i = 0; status == COM_STATUS_OK && i < names->tuple.count; ++ i)
			{
				Ast name = ast_ensure_kind(names->tuple.items[i], AST_IDENT);
				if (ast_is_error(name))
				{
					status = checker_report(checker->source_scope->source, ERROR_INVALID_DECLARATION, ERROR_ERROR, names->tuple.items[i]->site, "declaration name must be an identifier");
					break;
				}
				ASSERT(!name->entity);
				Entity *entity = declare_entity(checker, checker->active_scope, ENTITY_LOCAL, name, 0, tags, name->identifier, name->site);
				status = entity_is_error(entity) ? entity->status : COM_STATUS_OK;
				if (status == COM_STATUS_OK) {
					name->entity = entity;
				}
			}
		}
		break;

		case AST_STATIC_DECL:
		{
			// NOTE(RJ): lazily resolved !
		}
		break;

		case AST_IF:
		{
			value = resolve_expression(checker, stat->if_stat.pred);
			status = static_value_status(value);
			if (status == COM_STATUS_OK && stat->if_stat.true_clause) {
				status = check_statement(checker, stat->if_stat.true_clause);
			}
			if (status == COM_STATUS_OK && stat->if_stat.else_clause) {
				status = check_statement(checker, stat->if_stat.else_clause);
			}
			if (status == COM_STATUS_OK && stat->if_stat.then_clause) {
				status = check_statement(checker, stat->if_stat.then_clause);
			}
		}
		break;

		case AST_DEFER_STAT:
		{
			status = check_statement(checker, stat->defer_stat.body);
		}
		break;

		case AST_WHILE:
		{
			value = resolve_expression(checker, stat->while_stat.pred);
			status = static_value_status(value);
			if (status == COM_STATUS_OK) {
				status = check_statement(checker, stat->while_stat.body);
			}
		}
		break;

		case AST_FOR:
		{
			Scope *scope = 0;
			status = push_scope(checker, SCOPE_BLOCK, stat, &scope);
			if (status == COM_STATUS_OK)
			{
				status = check_statement(checker, stat->for_stat.init);
				if (status == COM_STATUS_OK)
				{
					value = resolve_expression(checker, stat->for_stat.pred);
					status = static_value_status(value);
				}
				if (status == COM_STATUS_OK) {
					status = check_statement(checker, stat->for_stat.step);
				}
				if (status == COM_STATUS_OK) {
					status = check_statement(checker, stat->for_stat.body);
				}
				pop_scope(checker, scope);
			}
		}
		break;

		case AST_FOR_RANGE:
		{
			Scope *scope = 0;
			status = push_scope(checker, SCOPE_BLOCK, stat, &scope);
			if (status == COM_STATUS_OK)
			{
				status = check_statement(checker, stat->for_range_stat.decl);
				if (status == COM_STATUS_OK) {
					status = check_statement(checker, stat->for_range_stat.body);
				}
				pop_scope(checker, scope);
			}
		}
		break;

		case AST_RETURN:
		{
			if (stat->return_stat.expr)
			{
				value = resolve_expression(checker, stat->return_stat.expr);
				status = static_value_status(value);
			}
		}
		break;

		case AST_BREAK:
		{
			if (stat->break_stat.expr)
			{
				value = resolve_expression(checker, stat->break_stat.expr);
				status = static_value_status(value);
			}
		}
		break;

		case AST_CONTINUE:
		{
			if (stat->continue_stat.expr)
			{
				value = resolve_expression(checker, stat->continue_stat.expr);
				status = static_value_status(value);
			}
		}
		break;

		default:
		{
			value = resolve_expression(checker, stat);
			status = static_value_status(value);
		}
		break;
	}
	return status;
}
