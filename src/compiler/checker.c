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

#define DEBUG_LOG(contents) printf(contents "\n")


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


static Entity *lookup_entity(Checker *checker, Atom *name)
{
	for (u32 i = checker->entity_index; i > 0; -- i)
	{
		Entity *entity = checker->entity_stack[i - 1];
		if (entity->name == name) {
			return entity;
		}
	}
	return 0;
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

static Entity *alloc_entity(Checker *checker, Atom *name) {
	Entity *entity = dy_arena_push_zero(checker->compiler->arena, sizeof(*entity), _Alignof(Entity));
	entity->name = name;
	return entity;
}

// TODO(RJ): hash table
static Entity *global_entity(Checker *checker, Atom *name) {
	EntityTable *table = & checker->globals;

	for (u32 index = 0; index < table->counter; ++ index) {
		if (table->references[index]->name == name) {
			return table->references[index];
		}
	}
	ASSERT(table->counter < table->capacity);
	Entity *entity = alloc_entity(checker, name);
	entity->kind = ENTITY_GLOBAL;
	table->references[table->counter ++] = entity;
	return entity;
}

static Entity *declare_entity(Checker *checker, EntityKind kind, Ast tree, EntityTags tags, Atom *name) {
	Entity *entity = lookup_entity_in_current_scope(checker, name);
	if (entity) {
		ASSERT(!"Codex: generate proper error handling here!");
	}
	ASSERT(checker->entity_index < MAX_ENTITIES);
	entity = alloc_entity(checker, name);
	checker->entity_stack[checker->entity_index ++] = entity;
	entity->kind = kind;
	entity->tree = tree;
	entity->owner_function = checker->current_function;
	entity->tags = tags;
	entity->name = name;
	return entity;
}

static void declare_names(Checker *checker, EntityTags tags, Ast_Array names) {
	for (u32 i = 0; i < names.count; ++ i)
	{
		Ast name = ast_ensure_kind(names.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			ASSERT(!"Codex: error");
		}
		ASSERT(!name->entity);
		name->entity = declare_entity(checker, ENTITY_LOCAL, name, tags, name->identifier);
	}
}

static StaticValue process_static_declaration_value(Checker *checker, Ast expr)
{
	StaticValue value = {STATIC_VALUE_NONE};
	switch (expr->kind) {
		case AST_IDENT: {
			ASSERT(expr->atom);
			value.type = STATIC_VALUE_UNIDENT;
			value.unident = expr->atom;
		}
		break;
		case AST_IMPORT: {
			ASSERT(!"Error: not supported yet!");
		}
		break;
		case AST_FUNCTION: {
		}
		break;
		case AST_INTEGER_LITERAL: {
			value.type = STATIC_VALUE_INTEGER;
			value.integer = expr->integer_value;
		}
		break;
		case AST_NUMBER_LITERAL:
		case AST_STRING_LITERAL: {
		}
		break;
		default: {
			ASSERT(!"Codex: proper error reporting here");
		}
		break;
	}
	return value;
}

// NOTE(RJ): iterate over each statement, collect static declarations only, and add them to the scope and resolve
static void predeclare_static_declarations(Checker *checker, Ast_Array stats)
{
	for (u32 i = 0; i < stats.count; ++ i) {
		Ast item = stats.items[i];
		if (item->kind != AST_STATIC_DECL) {
			continue;
		}
		ASSERT(item->static_decl.name);
		ASSERT(item->static_decl.name->kind == AST_IDENT);
		ASSERT(item->static_decl.name->atom);
		ASSERT(item->static_decl.expr);

		StaticValue value = process_static_declaration_value(checker, item->static_decl.expr);
		Entity *entity = declare_entity(checker, ENTITY_STATIC, item, ENTITY_TAG_CONSTANT, item->static_decl.name->atom);
		ASSERT(!item->static_decl.name->entity);
		item->static_decl.name->entity = entity;
		entity->value = value;
	}
}

static StaticValue check_tree(Checker *checker, Ast ast);

Checker *checker_run(Compiler *compiler, Ast ast)
{
	ASSERT(compiler);
	ASSERT(ast->kind == AST_FILE);
	ASSERT(ast->file.body);
	ASSERT(ast->file.body->kind == AST_BLOCK_STAT);

	Checker *checker = elf_arena_push_zero(compiler->arena, sizeof(* checker));
	checker->compiler = compiler;
	checker->current_function = ast;
	checker->globals.capacity = 4096;
	checker->globals.references = dy_arena_push_zero(compiler->arena, sizeof(*checker->globals.references) * checker->globals.capacity, _Alignof(Entity *));
	check_tree(checker, ast->file.body);
	return checker;
}

static StaticValue check_tree(Checker *checker, Ast ast)
{

	StaticValue value = { STATIC_VALUE_NONE };
	if (!ast) {
		return value;
	}

	switch (ast->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		case AST_NUMBER_LITERAL:
		case AST_INTEGER_LITERAL:
		case AST_STRING_LITERAL:
		case AST_IMPORT:
		case AST_RECURSE:
		case AST_NIL_LITERAL:
		case AST_ELLIPSIS:
		{
		}
		break;

		case AST_IDENT:
		{
			Entity *entity = lookup_entity(checker, ast->identifier);
			if (!entity) entity = global_entity(checker, ast->identifier);
			ast->entity = entity;
		}
		break;

		case AST_FILE:
		{
			check_tree(checker, ast->file.body);
		}
		break;

		case AST_BLOCK_STAT:
		{
			StaticScope scope = push_scope(checker);
			predeclare_static_declarations(checker, ast->block);
			for (u32 i = 0; i < ast->block.count; ++i) {
				check_tree(checker, ast->block.items[i]);
			}
			pop_scope(checker, scope);
		}
		break;

		case AST_FUNCTION:
		{
			StaticScope scope = push_scope(checker);
			Ast outer_function = checker->current_function;
			checker->current_function = ast;
			for (u32 i = 0; i < ast->function.params.count; ++i) {
				check_tree(checker, ast->function.params.items[i]);
			}
			check_tree(checker, ast->function.body);
			checker->current_function = outer_function;
			pop_scope(checker, scope);
		}
		break;

		case AST_FUNCTION_PARAM:
		{
			check_tree(checker, ast->param.type);
			check_tree(checker, ast->param.expr);
			Ast name = ast_ensure_kind(ast->param.name, AST_IDENT);
			if (ast_is_error(name)) {
				ASSERT(!"Codex: error");
			}
			ASSERT(!name->entity);
			name->entity = declare_entity(checker, ENTITY_PARAMETER, ast, ENTITY_TAG_PARAMETER, name->identifier);
		}
		break;

		case AST_DECL_STAT:
		{
			Ast names = ast_ensure_kind(ast->decl.name, AST_TUPLE);
			EntityTags tags = ast->decl.tags & AST_DECL_TAG_CONSTANT ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
			check_tree(checker, ast->decl.type);
			check_tree(checker, ast->decl.expr);
			declare_names(checker, tags, names->tuple);
		}
		break;

		case AST_STATIC_DECL:
		{
			check_tree(checker, ast->static_decl.expr);
		}
		break;

		case AST_BITWISE_NOT:
		case AST_MEASURE:
		case AST_GET_MEM:
		{
			check_tree(checker, ast->unary);
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
			check_tree(checker, ast->binary.x);
			check_tree(checker, ast->binary.y);
		}
		break;

		case AST_FIELD:
		case AST_META_FIELD:
		{
			check_tree(checker, ast->binary.x);
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			for (u32 i = 0; i < ast->interpolated_string.count; ++i) {
				check_tree(checker, ast->interpolated_string.items[i]);
			}
		}
		break;

		case AST_CALL:
		case AST_META_CALL:
		{
			check_tree(checker, ast->call.expr);
			for (u32 i = 0; i < ast->call.args.count; ++i) {
				check_tree(checker, ast->call.args.items[i]);
			}
			check_tree(checker, ast->call.carg);
		}
		break;

		case AST_NAMED_ARGUMENT:
		{
			check_tree(checker, ast->named_argument.expr);
		}
		break;

		case AST_TUPLE:
		{
			for (u32 i = 0; i < ast->tuple.count; ++i) {
				check_tree(checker, ast->tuple.items[i]);
			}
		}
		break;

		case AST_TABLE:
		{
			for (u32 i = 0; i < ast->table.count; ++i) {
				check_tree(checker, ast->table.items[i]);
			}
		}
		break;

		case AST_ARRAY:
		{
			for (u32 i = 0; i < ast->array.count; ++i) {
				check_tree(checker, ast->array.items[i]);
			}
		}
		break;

		case AST_TABLE_ENTRY:
		{
			check_tree(checker, ast->table_entry.key);
			check_tree(checker, ast->table_entry.value);
		}
		break;

		case AST_IF:
		{
			check_tree(checker, ast->if_stat.pred);
			check_tree(checker, ast->if_stat.true_clause);
			check_tree(checker, ast->if_stat.else_clause);
			check_tree(checker, ast->if_stat.then_clause);
		}
		break;

		case AST_DEFER_STAT:
		{
			check_tree(checker, ast->defer_stat.body);
		}
		break;

		case AST_WHILE:
		{
			check_tree(checker, ast->while_stat.pred);
			check_tree(checker, ast->while_stat.body);
		}
		break;

		case AST_FOR:
		{
			check_tree(checker, ast->for_stat.init);
			check_tree(checker, ast->for_stat.pred);
			check_tree(checker, ast->for_stat.step);
			check_tree(checker, ast->for_stat.body);
		}
		break;

		case AST_FOR_RANGE:
		{
			check_tree(checker, ast->for_range_stat.decl);
			check_tree(checker, ast->for_range_stat.body);
		}
		break;

		case AST_RETURN:
		{
			check_tree(checker, ast->return_stat.expr);
		}
		break;

		case AST_BREAK:
		{
			check_tree(checker, ast->break_stat.expr);
		}
		break;

		case AST_CONTINUE:
		{
			check_tree(checker, ast->continue_stat.expr);
		}
		break;

		default:
		{
			ASSERT(!"unhandled AST kind");
		}
		break;
	}

	return value;
}
