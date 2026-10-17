//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "compiler.h"
#include "ast.h"
#include "ir.h"
#include "ir_emitter.h"

#include "checker.h"


static void check_statement(Checker *checker, Ast ast);
static Entity *resolve_identifier(Checker *checker, Atom *name);
static void resolve_entity(Checker *checker, Entity *entity);
static StaticValue resolve_expression(Checker *checker, Ast expr);

typedef struct {
	u32 outer_scope_start;
	u32 entity_start;
	u32 local;
}
StaticScope;

static StaticScope push_scope(Checker *checker)
{
	StaticScope scope;
	scope.outer_scope_start = checker->scope_start;
	scope.entity_start = checker->entity_index;
	checker->scope_start = checker->entity_index;
	return scope;
}

static void pop_scope(Checker *checker, StaticScope scope)
{
	ASSERT(checker->entity_index >= scope.entity_start);
	checker->entity_index = scope.entity_start;
	checker->scope_start = scope.outer_scope_start;
}

static Entity *lookup_entity_in_current_scope(Checker *checker, Atom *name)
{
	for (u32 i = checker->entity_index; i > checker->scope_start; -- i)
	{
		Entity *entity = checker->entity_stack[i - 1];
		if (entity->name == name) {
			return entity;
		}
	}
	return 0;
}

static Entity *alloc_entity(Checker *checker, Atom *name)
{
	return entity_alloc(checker->compiler->arena, ENTITY_NONE, name);
}

static Entity *declare_entity(Checker *checker, EntityKind kind, Ast tree, EntityTags tags, Atom *name) {
	Entity *entity = lookup_entity_in_current_scope(checker, name);
	if (entity) {
		compiler_report(checker->compiler, COM_ERROR_DUPLICATE_DECLARATION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, tree->site, "'%s' is already declared in this scope", atom_data(name));
	}
	printf("DECLARE ENTITY: %s\n", atom_data(name));
	ASSERT(checker->entity_index < MAX_ENTITIES);
	entity = alloc_entity(checker, name);
	checker->entity_stack[checker->entity_index ++] = entity;
	entity->kind = kind;
	entity->name = name;
	entity->state = ENTITY_UNRESOLVED;
	entity->tree = tree;
	entity->parent = checker->current_function;
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
		name->entity = declare_entity(checker, ENTITY_LOCAL, name, tags, name->identifier);
		name->entity->state = ENTITY_RESOLVED;
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

		Entity *entity = declare_entity(checker, ENTITY_STATIC, ast_decl, ENTITY_TAG_CONSTANT, ast_name->atom);
		ASSERT(entity);
		ast_name->entity = entity;
		ast_decl->entity = entity;
		entity->expr = ast_expr;
	}
}

Checker *checker_run(Compiler *compiler, Ast ast)
{
	ASSERT(compiler);
	ASSERT(ast->kind == AST_FILE);
	ASSERT(ast->file.body);
	ASSERT(ast->file.body->kind == AST_BLOCK_STAT);

	Checker *checker = elf_arena_push_zero(compiler->arena, sizeof(* checker));
	checker->compiler = compiler;
	checker->current_function = ast;
	checker->globals = entity_table_create(compiler->arena);
	check_statement(checker, ast->file.body);
	return checker;
}

static void resolve_entity(Checker *checker, Entity *entity) {
	ASSERT(entity->state != ENTITY_RESOLVING || !"Codex: error reporting!");
	if (entity->state == ENTITY_RESOLVED) {
		return;
	}
	ASSERT(entity->expr || !"Internal Error");
	entity->state = ENTITY_RESOLVING;
	StaticValue value = resolve_expression(checker, entity->expr);
	ASSERT(value.type != STATIC_VALUE_NONE || !"Codex: error reporting!");
	entity->state = ENTITY_RESOLVED;
	entity->value = value;
}

static Entity *resolve_identifier(Checker *checker, Atom *name)
{
	for (u32 i = checker->entity_index; i > 0; -- i)
	{
		Entity *entity = checker->entity_stack[i - 1];
		if (entity->name == name)
		{
			// NOTE(RJ): if we're not a static entity then we must be resolved, because only static entities
			// are allowed to be unresolved
			ASSERT(entity->kind == ENTITY_STATIC || entity->state == ENTITY_RESOLVED);
			if (entity->kind == ENTITY_STATIC && entity->state != ENTITY_RESOLVED) {
				resolve_entity(checker, entity);
			}
			return entity;
		}
	}
	Entity *entity = entity_table_find(checker->globals, name);
	if (!entity)
	{
		entity = alloc_entity(checker, name);
		entity->kind = ENTITY_GLOBAL;
		entity->state = ENTITY_RESOLVED;
		entity = entity_table_insert(checker->globals, entity);
	}
	return entity;
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
			StaticScope scope = push_scope(checker);
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
				name->entity = declare_entity(checker, ENTITY_PARAMETER, param, ENTITY_TAG_PARAMETER, name->identifier);
				name->entity->state = ENTITY_RESOLVED;
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
			ASSERT(expr->import.source != COM_INVALID_SOURCE_ID);
			ASSERT(expr->import.source < checker->compiler->source_count);
			Com_Source *source = checker->compiler->sources + expr->import.source;
			ASSERT(source->exports);
			value.type = STATIC_VALUE_EXPORTS;
			value.exports = source->exports;
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
				Entity *entity = entity_table_find(left_value.exports, expr->binary.y->atom);
				if (!entity) {
					ASSERT(!"Codex: proper error handling");
				}
				ASSERT(entity->value.type != STATIC_VALUE_NONE);
				ASSERT(entity->kind == ENTITY_STATIC);
				entity->users ++;
				entity->tags |= ENTITY_TAG_REFERENCED;
				value = entity->value;
				ASSERT(!expr->entity);
				expr->entity = entity;
				entity->tree = expr;
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

	switch (stat->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		{
		}
		break;

		case AST_FILE:
		{
			check_statement(checker, stat->file.body);
		}
		break;

		case AST_BLOCK_STAT:
		{
			StaticScope scope = push_scope(checker);
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
			StaticScope scope = push_scope(checker);
			check_statement(checker, stat->for_stat.init);
			resolve_expression(checker, stat->for_stat.pred);
			check_statement(checker, stat->for_stat.step);
			check_statement(checker, stat->for_stat.body);
			pop_scope(checker, scope);
		}
		break;

		case AST_FOR_RANGE:
		{
			StaticScope scope = push_scope(checker);
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
