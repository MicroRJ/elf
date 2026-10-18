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
	Compiler *compiler;
	Scope    *globals;
	Scope    *source_scope;
	Scope    *active_scope;
	Ast       current_function;
	Com_Status status;
}
Checker;

static void check_statement(Checker *checker, Ast ast);
static Entity *resolve_identifier(Checker *checker, Ast identifier);
static StaticValue resolve_entity(Checker *checker, Entity *entity);
static StaticValue resolve_expression(Checker *checker, Ast expr);

static StaticValue static_value_error(void)
{
	StaticValue value = {STATIC_VALUE_ERROR};
	return value;
}

static void propagate_static_error(StaticValue *result, StaticValue value)
{
	if (value.type == STATIC_VALUE_ERROR) {
		result->type = STATIC_VALUE_ERROR;
	}
}

static b32 checker_report(Checker *checker, ErrorCode error, ErrorSeverity severity, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 reported = compiler_reportv(checker->compiler, error, severity, ERROR_PHASE_CHECKER, site, format, args);
	va_end(args);
	if (!reported) {
		checker->status = COM_STATUS_OOM;
	}
	return reported;
}

static void checker_out_of_memory(Checker *checker, SourceSite site)
{
	if (checker->status == COM_STATUS_OK) {
		checker->status = compiler_report(checker->compiler, ERROR_OUT_OF_MEMORY, ERROR_FATAL, ERROR_PHASE_CHECKER, site, "out of memory during semantic analysis") ? COM_STATUS_REPORTED : COM_STATUS_OOM;
	}
}

static Scope *push_scope(Checker *checker, ScopeKind kind, Ast owner)
{
	ASSERT(checker);
	ASSERT(checker->active_scope);
	ASSERT(owner);
	ASSERT(checker->source_scope);
	ASSERT(owner->source == checker->source_scope->source);
	ASSERT(checker->active_scope->source == checker->source_scope->source);
	Scope *scope = scope_create(checker->compiler->arena, kind, checker->active_scope, owner, checker->source_scope->source);
	if (scope) {
		checker->active_scope = scope;
	} else {
		checker_out_of_memory(checker, owner->site);
	}
	return scope;
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
		checker_report(checker, error, ERROR_ERROR, site, "'%s' is already declared in this scope", atom_data(name));
		return existing;
	}

	Entity *entity = scope_declare(scope, kind, name);
	if (!entity)
	{
		checker_out_of_memory(checker, site);
		return 0;
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

static void declare_locals(Checker *checker, EntityTags tags, Ast_Array names)
{
	for (u32 i = 0; checker->status == COM_STATUS_OK && i < names.count; ++ i)
	{
		Ast name = ast_ensure_kind(names.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			checker_report(checker, ERROR_INVALID_DECLARATION, ERROR_ERROR, names.items[i]->site, "declaration name must be an identifier");
			continue;
		}
		ASSERT(!name->entity);
		name->entity = declare_entity(checker, checker->active_scope, ENTITY_LOCAL, name, 0, tags, name->identifier, name->site);
		if (checker->status != COM_STATUS_OK) {
			break;
		}
	}
}

static void declare_statics(Checker *checker, Scope *scope, Ast_Array stats)
{
	for (u32 i = 0; checker->status == COM_STATUS_OK && i < stats.count; ++ i) {
		Ast ast_decl = stats.items[i];
		if (ast_decl->kind != AST_STATIC_DECL) {
			continue;
		}
		Ast ast_name = ast_decl->static_decl.name;
		Ast ast_expr = ast_decl->static_decl.expr;
		if (!ast_name || ast_name->kind != AST_IDENT || !ast_name->atom || !ast_expr)
		{
			checker_report(checker, ERROR_INVALID_STATIC_DECLARATION, ERROR_ERROR, ast_decl->site, "invalid static declaration");
			continue;
		}
		ASSERT(!ast_name->entity);
		ASSERT(!ast_decl->entity);

		EntityTags tags = ENTITY_TAG_CONSTANT;
		if (ast_decl->static_decl.exported) {
			tags |= ENTITY_TAG_EXPORTED;
		}
		Entity *entity = declare_entity(checker, scope, ENTITY_STATIC, ast_decl, ast_expr, tags, ast_name->atom, ast_decl->site);
		if (entity)
		{
			ast_name->entity = entity;
			ast_decl->entity = entity;
		}
		if (checker->status != COM_STATUS_OK) {
			break;
		}
	}
}

Com_Status check_source(Compiler *compiler, Com_Source *source)
{
	ASSERT(compiler);
	ASSERT(source);
	Ast ast_file = source->ast;
	ASSERT(ast_file);
	ASSERT(ast_file->kind == AST_FILE);
	ASSERT(ast_file->file.body);
	ASSERT(ast_file->file.body->kind == AST_BLOCK_STAT);

	Checker checker = {0};
	checker.compiler = compiler;
	checker.current_function = ast_file;
	checker.globals = scope_create(compiler->arena, SCOPE_GLOBAL, 0, 0, 0);
	u32 initial_error_count = compiler->error_count;
	if (!checker.globals) {
		checker_out_of_memory(&checker, ast_file->site);
	}

	Scope *previous_source_scope = checker.source_scope;
	Scope *previous_active_scope = checker.active_scope;

	if (checker.status == COM_STATUS_OK)
	{
		checker.source_scope = scope_create(checker.compiler->arena, SCOPE_SOURCE, checker.globals, ast_file, source);
		checker.active_scope = checker.source_scope;
		if (!checker.source_scope) {
			checker_out_of_memory(&checker, ast_file->site);
		}
	}

	if (checker.status == COM_STATUS_OK)
	{
		ASSERT(!source->scope);
		source->scope = checker.source_scope;

		declare_statics(&checker, checker.source_scope, ast_file->file.body->block);
		for (u32 i = 0; checker.status == COM_STATUS_OK && i < ast_file->file.body->block.count; ++i) {
			check_statement(&checker, ast_file->file.body->block.items[i]);
		}
	}

	checker.source_scope = previous_source_scope;
	checker.active_scope = previous_active_scope;
	if (checker.status != COM_STATUS_OK) {
		return checker.status;
	}
	return compiler->error_count > initial_error_count ? COM_STATUS_REPORTED : COM_STATUS_OK;
}

static StaticValue resolve_entity(Checker *checker, Entity *entity)
{
	if (checker->status != COM_STATUS_OK) {
		return static_value_error();
	}
	ASSERT(entity);
	ASSERT(entity->kind == ENTITY_STATIC);
	if (entity->state == ENTITY_RESOLVING)
	{
		checker_report(checker, ERROR_CYCLIC_STATIC_DEFINITION, ERROR_ERROR, entity->tree->site, "cyclic static definition involving '%s'", atom_data(entity->name));
		return static_value_error();
	}
	if (entity->state == ENTITY_RESOLVED) {
		return entity->value;
	}
	ASSERT(entity->expr || !"Internal Error");
	entity->state = ENTITY_RESOLVING;

	// NOTE(RJ): we can lazily resolve entities from different sources
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
		checker_report(checker, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, entity->expr->site, "static definition '%s' does not resolve to a static value", atom_data(entity->name));
		value = static_value_error();
	}
	entity->state = ENTITY_RESOLVED;
	entity->value = value;
	return value;
}

static Entity *resolve_identifier(Checker *checker, Ast identifier)
{
	ASSERT(identifier);
	ASSERT(identifier->kind == AST_IDENT);
	Atom *name = identifier->identifier;
	Entity *entity = scope_find(checker->active_scope, name);
	if (entity)
	{
		ASSERT(entity->kind == ENTITY_STATIC || entity->state == ENTITY_RESOLVED);
		return entity;
	}
	entity = declare_entity(checker, checker->globals, ENTITY_GLOBAL, 0, 0, ENTITY_TAG_NONE, name, identifier->site);
	return entity;
}

static Scope *resolve_source_scope_lazily(Checker *checker, Com_Source *source)
{
	Compiler *compiler = checker->compiler;
	ASSERT(source);
	ASSERT(source->name);
	ASSERT(source->path);
	ASSERT(source->ast);
	ASSERT(source->ast->kind == AST_FILE);
	ASSERT(source->ast->file.body->kind == AST_BLOCK_STAT);
	if (source->scope) {
		return source->scope;
	}
	source->scope = scope_create(compiler->arena, SCOPE_SOURCE, checker->globals, source->ast, source);
	if (!source->scope)
	{
		checker_out_of_memory(checker, source->ast->site);
		return 0;
	}
	declare_statics(checker, source->scope, source->ast->file.body->block);
	if (checker->status != COM_STATUS_OK) {
		return 0;
	}

	return source->scope;
}

// TOOD(RJ): imagine writing an interpreter for an interpreter ...
static StaticValue resolve_expression(Checker *checker, Ast expr)
{
	StaticValue value = {STATIC_VALUE_NONE};
	if (checker->status != COM_STATUS_OK) {
		return static_value_error();
	}
	if (!expr) {
		return value;
	}

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
			Scope *scope = push_scope(checker, SCOPE_FUNCTION, expr);
			if (!scope) {
				value = static_value_error();
				break;
			}
			Ast outer_function = checker->current_function;
			checker->current_function = expr;
			for (u32 i = 0; checker->status == COM_STATUS_OK && i < expr->function.params.count; ++i)
			{
				Ast param = expr->function.params.items[i];
				if (!param || param->kind != AST_FUNCTION_PARAM)
				{
					checker_report(checker, ERROR_INVALID_FUNCTION_PARAMETER, ERROR_ERROR, expr->site, "invalid function parameter");
					value = static_value_error();
					continue;
				}
				propagate_static_error(&value, resolve_expression(checker, param->param.type));
				propagate_static_error(&value, resolve_expression(checker, param->param.expr));
				Ast name = ast_ensure_kind(param->param.name, AST_IDENT);
				if (ast_is_error(name))
				{
					checker_report(checker, ERROR_INVALID_FUNCTION_PARAMETER, ERROR_ERROR, param->site, "function parameter name must be an identifier");
					value = static_value_error();
					continue;
				}
				ASSERT(!name->entity);
				name->entity = declare_entity(checker, checker->active_scope, ENTITY_PARAMETER, param, 0, ENTITY_TAG_PARAMETER, name->identifier, name->site);
				if (checker->status != COM_STATUS_OK || !name->entity) {
					value = static_value_error();
					break;
				}
			}
			check_statement(checker, expr->function.body);
			checker->current_function = outer_function;
			pop_scope(checker, scope);
			if (value.type != STATIC_VALUE_ERROR)
			{
				value.type = STATIC_VALUE_FUNCTION;
				value.function = expr;
			}
		}
		break;

		case AST_IMPORT:
		{
			if (!expr->import.target)
			{
				checker_report(checker, ERROR_IMPORT, ERROR_ERROR, expr->site, "import '%s' was not resolved to a source", atom_data(expr->import.path));
				value = static_value_error();
			}
			else
			{
				value.type = STATIC_VALUE_EXPORTS;
				value.exports = resolve_source_scope_lazily(checker, expr->import.target);
				if (!value.exports) {
					value = static_value_error();
				}
			}
		}
		break;

		case AST_IDENT:
		{
			Entity *entity = resolve_identifier(checker, expr);
			if (entity)
			{
				expr->entity = entity;
				value = entity->kind == ENTITY_STATIC ? resolve_entity(checker, entity) : entity->value;
			}
			else {
				value = static_value_error();
			}
		}
		break;

		case AST_FIELD:
		{
			StaticValue left_value = resolve_expression(checker, expr->binary.x);
			if (left_value.type == STATIC_VALUE_ERROR)
			{
				value = static_value_error();
			}
			else if (left_value.type == STATIC_VALUE_EXPORTS)
			{
				ASSERT(expr->binary.y);
				ASSERT(expr->binary.y->kind == AST_IDENT || expr->binary.y->kind == AST_STRING_LITERAL);
				ASSERT(expr->binary.y->atom);
				Entity *entity = scope_find_current(left_value.exports, expr->binary.y->atom);
				if (!entity)
				{
					checker_report(checker, ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "source '%s' has no static declaration named '%s'", atom_data(left_value.exports->source->name), atom_data(expr->binary.y->atom));
					value = static_value_error();
				}
				else if (!(entity->tags & ENTITY_TAG_EXPORTED))
				{
					checker_report(checker, ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "static declaration '%s' is private to source '%s'", atom_data(entity->name), atom_data(left_value.exports->source->name));
					value = static_value_error();
				}
				else
				{
					value = resolve_entity(checker, entity);
					entity->users += 1;
					entity->tags |= ENTITY_TAG_REFERENCED;
					expr->entity = entity;
				}
			}
		}
		break;

		case AST_BITWISE_NOT:
		case AST_MEASURE:
		case AST_GET_MEM:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->unary));
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
		case AST_RANGE:
		case AST_RANGE_INDEX:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->binary.x));
			propagate_static_error(&value, resolve_expression(checker, expr->binary.y));
		}
		break;

		case AST_META_FIELD:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->binary.x));
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			for (u32 i = 0; i < expr->interpolated_string.count; ++i) {
				propagate_static_error(&value, resolve_expression(checker, expr->interpolated_string.items[i]));
			}
		}
		break;

		case AST_CALL:
		case AST_META_CALL:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->call.expr));
			for (u32 i = 0; i < expr->call.args.count; ++i) {
				propagate_static_error(&value, resolve_expression(checker, expr->call.args.items[i]));
			}
		}
		break;

		case AST_NAMED_ARGUMENT:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->named_argument.expr));
		}
		break;

		case AST_TUPLE:
		{
			for (u32 i = 0; i < expr->tuple.count; ++i) {
				propagate_static_error(&value, resolve_expression(checker, expr->tuple.items[i]));
			}
		}
		break;

		case AST_TABLE:
		{
			for (u32 i = 0; i < expr->table.count; ++i) {
				propagate_static_error(&value, resolve_expression(checker, expr->table.items[i]));
			}
		}
		break;

		case AST_ARRAY:
		{
			for (u32 i = 0; i < expr->array.count; ++i) {
				propagate_static_error(&value, resolve_expression(checker, expr->array.items[i]));
			}
		}
		break;

		case AST_TABLE_ENTRY:
		{
			propagate_static_error(&value, resolve_expression(checker, expr->table_entry.key));
			propagate_static_error(&value, resolve_expression(checker, expr->table_entry.value));
		}
		break;

		default:
		{
			checker_report(checker, ERROR_INTERNAL, ERROR_ERROR, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			value = static_value_error();
		}
		break;
	}
	return value;
}

static void check_statement(Checker *checker, Ast stat)
{
	if (checker->status != COM_STATUS_OK || !stat) {
		return;
	}

	ASSERT(stat->kind != AST_FILE);
	ASSERT(checker->active_scope);
	ASSERT(checker->source_scope);
	ASSERT(stat->source == checker->source_scope->source);
	ASSERT(stat->source == checker->active_scope->source);

	switch (stat->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		{
		}
		break;

		case AST_BLOCK_STAT:
		{
			Scope *scope = push_scope(checker, SCOPE_BLOCK, stat);
			if (!scope) {
				break;
			}
			declare_statics(checker, scope, stat->block);
			for (u32 i = 0; i < stat->block.count; ++i) {
				check_statement(checker, stat->block.items[i]);
				if (checker->status != COM_STATUS_OK) {
					break;
				}
			}
			pop_scope(checker, scope);
		}
		break;

		case AST_DECL_STAT:
		{
			Ast names = ast_ensure_kind(stat->decl.name, AST_TUPLE);
			EntityTags tags = stat->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
			resolve_expression(checker, stat->decl.type);
			resolve_expression(checker, stat->decl.expr);
			declare_locals(checker, tags, names->tuple);
		}
		break;

		case AST_STATIC_DECL:
		{
			// NOTE(RJ): lazily resolved !
		}
		break;

		case AST_IF:
		{
			resolve_expression(checker, stat->if_stat.pred);
			check_statement(checker, stat->if_stat.true_clause);
			check_statement(checker, stat->if_stat.else_clause);
			check_statement(checker, stat->if_stat.then_clause);
		}
		break;

		case AST_DEFER_STAT:
		{
			check_statement(checker, stat->defer_stat.body);
		}
		break;

		case AST_WHILE:
		{
			resolve_expression(checker, stat->while_stat.pred);
			check_statement(checker, stat->while_stat.body);
		}
		break;

		case AST_FOR:
		{
			Scope *scope = push_scope(checker, SCOPE_BLOCK, stat);
			if (scope)
			{
				check_statement(checker, stat->for_stat.init);
				resolve_expression(checker, stat->for_stat.pred);
				check_statement(checker, stat->for_stat.step);
				check_statement(checker, stat->for_stat.body);
				pop_scope(checker, scope);
			}
		}
		break;

		case AST_FOR_RANGE:
		{
			Scope *scope = push_scope(checker, SCOPE_BLOCK, stat);
			if (scope)
			{
				check_statement(checker, stat->for_range_stat.decl);
				check_statement(checker, stat->for_range_stat.body);
				pop_scope(checker, scope);
			}
		}
		break;

		case AST_RETURN:
		{
			resolve_expression(checker, stat->return_stat.expr);
		}
		break;

		case AST_BREAK:
		{
			resolve_expression(checker, stat->break_stat.expr);
		}
		break;

		case AST_CONTINUE:
		{
			resolve_expression(checker, stat->continue_stat.expr);
		}
		break;

		default:
		{
			resolve_expression(checker, stat);
		}
		break;
	}
}
