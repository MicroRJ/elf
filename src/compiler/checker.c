//
// See Copyright Notice In elf.h
//

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


static void check_statement(Checker *checker, Ast ast);
static Entity *resolve_identifier(Checker *checker, Atom *name);
static void resolve_entity(Checker *checker, Entity *entity);
static StaticValue resolve_expression(Checker *checker, Ast expr);

static Scope *push_scope(Checker *checker, ScopeKind kind, Ast owner)
{
	ASSERT(checker);
	ASSERT(checker->active_scope);
	ASSERT(owner);
	ASSERT(checker->source_scope);
	ASSERT(owner->source == checker->source_scope->source);
	ASSERT(checker->active_scope->source == checker->source_scope->source);
	Scope *scope = scope_create(checker->compiler->arena, kind, checker->active_scope, owner, checker->source_scope->source);
	ASSERT(scope || !"Codex: proper error propagation");
	checker->active_scope = scope;
	return scope;
}

static void pop_scope(Checker *checker, Scope *scope)
{
	ASSERT(scope);
	ASSERT(checker->active_scope == scope);
	checker->active_scope = scope->parent;
}

static Entity *declare_entity(Checker *checker, Scope *scope, EntityKind kind, Ast tree, Ast expr, EntityTags tags, Atom *name)
{
	Entity *existing = scope_find_current(scope, name);
	if (existing)
	{
		ASSERT(tree);
		compiler_report(checker->compiler, COM_ERROR_DUPLICATE_DECLARATION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, tree->site, "'%s' is already declared in this scope", atom_data(name));
		return existing;
	}

	Entity *entity = scope_declare(scope, kind, name);
	ASSERT(entity || !"Codex: proper erroring!");

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

// Codex: just inline this
static void declare_locals(Checker *checker, EntityTags tags, Ast_Array names) {
	for (u32 i = 0; i < names.count; ++ i)
	{
		Ast name = ast_ensure_kind(names.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			ASSERT(!"Codex: proper error reporting!");
		}
		ASSERT(!name->entity);
		name->entity = declare_entity(checker, checker->active_scope, ENTITY_LOCAL, name, 0, tags, name->identifier);
	}
}

static void declare_statics(Checker *checker, Ast_Array stats)
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

		Entity *entity = declare_entity(checker, checker->active_scope, ENTITY_STATIC, ast_decl, ast_expr, ENTITY_TAG_CONSTANT, ast_name->atom);
		ASSERT(entity);
		ast_name->entity = entity;
		ast_decl->entity = entity;
	}
}

Checker *check_source(Compiler *compiler, Com_Source *source)
{
	ASSERT(compiler);
	ASSERT(source);
	Ast ast_file = source->ast;
	ASSERT(ast_file);
	ASSERT(ast_file->kind == AST_FILE);
	ASSERT(ast_file->file.body);
	ASSERT(ast_file->file.body->kind == AST_BLOCK_STAT);

	Checker *checker = elf_arena_push_zero(compiler->arena, sizeof(* checker));
	checker->compiler = compiler;
	checker->current_function = ast_file;
	checker->globals = scope_create(compiler->arena, SCOPE_GLOBAL, 0, 0, 0);

	Scope *previous_source_scope = checker->source_scope;
	Scope *previous_active_scope = checker->active_scope;

	checker->source_scope = scope_create(checker->compiler->arena, SCOPE_SOURCE, checker->globals, ast_file, source);
	checker->active_scope = checker->source_scope;

	ASSERT(!source->scope);
	source->scope = checker->source_scope;

	declare_statics(checker, ast_file->file.body->block);
	for (u32 i = 0; i < ast_file->file.body->block.count; ++i) {
		check_statement(checker, ast_file->file.body->block.items[i]);
	}

	checker->source_scope = previous_source_scope;
	checker->active_scope = previous_active_scope;
	return checker;
}

static void resolve_entity(Checker *checker, Entity *entity) {
	ASSERT(entity->state != ENTITY_RESOLVING || !"Codex: error reporting!");
	if (entity->state == ENTITY_RESOLVED) {
		return;
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


	ASSERT(value.type != STATIC_VALUE_NONE || !"Codex: error reporting!");
	entity->state = ENTITY_RESOLVED;
	entity->value = value;
}

static Entity *resolve_identifier(Checker *checker, Atom *name)
{
	Entity *entity = scope_find(checker->active_scope, name);
	if (entity)
	{
		// NOTE(RJ): if we're not a static entity then we must be resolved, because only static entities
		// are allowed to be unresolved
		ASSERT(entity->kind == ENTITY_STATIC || entity->state == ENTITY_RESOLVED);
		if (entity->kind == ENTITY_STATIC && entity->state != ENTITY_RESOLVED) {
			resolve_entity(checker, entity);
		}
		return entity;
	}
	entity = declare_entity(checker, checker->globals, ENTITY_GLOBAL, 0, 0, ENTITY_TAG_NONE, name);
	return entity;
}

static Scope *resolve_source_scope_lazily(Checker *checker, Com_SourceId source_id)
{
	Compiler *compiler = checker->compiler;
	ASSERT(source_id != COM_INVALID_SOURCE_ID);
	Com_Source *source = compiler_source_from_id(compiler, source_id);
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
	ASSERT(source->scope || !"Codex: proper error propagation");

	Ast_Array stats = source->ast->file.body->block;
	for (u32 i = 0; i < stats.count; ++ i)
	{
		Ast stat = stats.items[i];
		if (stat->kind != AST_STATIC_DECL) {
			continue;
		}

		Ast name = stat->static_decl.name;
		ASSERT(name);
		ASSERT(name->kind == AST_IDENT);
		ASSERT(name->atom);
		ASSERT(!name->entity);
		ASSERT(!stat->entity);


		EntityTags tags = ENTITY_TAG_CONSTANT;
		if (stat->static_decl.exported) {
			tags |= ENTITY_TAG_EXPORTED;
			printf("  FOUND EXPORT: %s\n", atom_data(name->atom));
		}

		Entity *entity = declare_entity(checker, source->scope, ENTITY_STATIC, stat, stat->static_decl.expr, tags, name->identifier);
		name->entity = entity;
		stat->entity = entity;

	}

	return source->scope;
}

// TOOD(RJ): imagine writing an interpreter for an interpreter ...
static StaticValue resolve_expression(Checker *checker, Ast expr)
{
	StaticValue value = {STATIC_VALUE_NONE};
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
			Ast outer_function = checker->current_function;
			checker->current_function = expr;
			for (u32 i = 0; i < expr->function.params.count; ++i)
			{
				Ast param = expr->function.params.items[i];
				ASSERT(param->kind == AST_FUNCTION_PARAM);
				resolve_expression(checker, param->param.type);
				resolve_expression(checker, param->param.expr);
				Ast name = ast_ensure_kind(param->param.name, AST_IDENT);
				if (ast_is_error(name)) {
					ASSERT(!"Codex: error");
				}
				ASSERT(!name->entity);
				name->entity = declare_entity(checker, checker->active_scope, ENTITY_PARAMETER, param, 0, ENTITY_TAG_PARAMETER, name->identifier);
			}
			check_statement(checker, expr->function.body);
			checker->current_function = outer_function;
			pop_scope(checker, scope);
			value.type = STATIC_VALUE_FUNCTION;
			value.function = expr;
		}
		break;

		case AST_IMPORT:
		{
			printf("RESOLVING IMPORT EXPRESSION!\n");
			value.type = STATIC_VALUE_EXPORTS;
			value.exports = resolve_source_scope_lazily(checker, expr->import.source);
		}
		break;

		case AST_IDENT:
		{
			Entity *entity = resolve_identifier(checker, expr->identifier);
			ASSERT(entity || !"Codex: proper error!");
			expr->entity = entity;
			value = entity->value;
		}
		break;

		case AST_FIELD:
		{
			StaticValue left_value = resolve_expression(checker, expr->binary.x);
			if (left_value.type == STATIC_VALUE_EXPORTS)
			{
				ASSERT(expr->binary.y);
				ASSERT(expr->binary.y->kind == AST_IDENT || expr->binary.y->kind == AST_STRING_LITERAL);
				ASSERT(expr->binary.y->atom);
				Entity *entity = scope_find_current(left_value.exports, expr->binary.y->atom);
				if (!entity) {
					ASSERT(!"Codex: proper error handling");
				}
				if (~entity->tags & ENTITY_TAG_EXPORTED) {
					ASSERT(!"You can't see this entity ... use 'export'");
					ASSERT(!"Codex: proper error handling");
				}
				resolve_entity(checker, entity);
				ASSERT(entity->value.type != STATIC_VALUE_NONE);
				ASSERT(entity->kind == ENTITY_STATIC);
				entity->users ++;
				entity->tags |= ENTITY_TAG_REFERENCED;
				value = entity->value;
				ASSERT(!expr->entity);
				expr->entity = entity;
			}
		}
		break;

		case AST_BITWISE_NOT:
		case AST_MEASURE:
		case AST_GET_MEM:
		{
			resolve_expression(checker, expr->unary);
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
			resolve_expression(checker, expr->binary.x);
			resolve_expression(checker, expr->binary.y);
		}
		break;

		case AST_META_FIELD:
		{
			resolve_expression(checker, expr->binary.x);
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			for (u32 i = 0; i < expr->interpolated_string.count; ++i) {
				resolve_expression(checker, expr->interpolated_string.items[i]);
			}
		}
		break;

		case AST_CALL:
		case AST_META_CALL:
		{
			resolve_expression(checker, expr->call.expr);
			for (u32 i = 0; i < expr->call.args.count; ++i) {
				resolve_expression(checker, expr->call.args.items[i]);
			}
			resolve_expression(checker, expr->call.carg);
		}
		break;

		case AST_NAMED_ARGUMENT:
		{
			resolve_expression(checker, expr->named_argument.expr);
		}
		break;

		case AST_TUPLE:
		{
			for (u32 i = 0; i < expr->tuple.count; ++i) {
				resolve_expression(checker, expr->tuple.items[i]);
			}
		}
		break;

		case AST_TABLE:
		{
			for (u32 i = 0; i < expr->table.count; ++i) {
				resolve_expression(checker, expr->table.items[i]);
			}
		}
		break;

		case AST_ARRAY:
		{
			for (u32 i = 0; i < expr->array.count; ++i) {
				resolve_expression(checker, expr->array.items[i]);
			}
		}
		break;

		case AST_TABLE_ENTRY:
		{
			resolve_expression(checker, expr->table_entry.key);
			resolve_expression(checker, expr->table_entry.value);
		}
		break;

		default:
		{
			ASSERT(!"statement passed to resolve_expression");
		}
		break;
	}
	return value;
}

static void check_statement(Checker *checker, Ast stat)
{
	if (!stat) {
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
			declare_statics(checker, stat->block);
			for (u32 i = 0; i < stat->block.count; ++i) {
				check_statement(checker, stat->block.items[i]);
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
			ASSERT(stat->entity);
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
			check_statement(checker, stat->for_stat.init);
			resolve_expression(checker, stat->for_stat.pred);
			check_statement(checker, stat->for_stat.step);
			check_statement(checker, stat->for_stat.body);
			pop_scope(checker, scope);
		}
		break;

		case AST_FOR_RANGE:
		{
			Scope *scope = push_scope(checker, SCOPE_BLOCK, stat);
			check_statement(checker, stat->for_range_stat.decl);
			check_statement(checker, stat->for_range_stat.body);
			pop_scope(checker, scope);
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
