//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "compiler.h"
#include "ast.h"


typedef struct Entity      Entity;
typedef struct EntityScope EntityScope;


struct Entity {
	Atom      *name;
	SourceSite site;
	u32        users;
	union {
		EntityScope *scope;
	};
};

struct EntityScope {
	Entity *entities;
	u32     entity_count;
};

typedef struct {
	b32 is_constant;
	u32 entity;
}
StaticData;

typedef struct {
	StaticData *static_data;
	Entity     *entities;
}
StaticModule;

StaticModule *run_static_analysis(Compiler *compiler, Ast ast)
{
	ASSERT(compiler);
	ASSERT(ast->kind == AST_FILE);
	ASSERT(ast->file.body);
	ASSERT(ast->file.body->kind == AST_BLOCK_STAT);

	// iterate all top level statements and find imports
	Ast_Array stats = ast->file.body->block;
	for (u32 i = 0; i < stats.count; ++ i) {
		Ast item = stats.items[i];
		if (item->kind == AST_STATIC_DECL) {
			ASSERT(item->static_decl.expr);
			switch (item->static_decl.expr->kind) {
				case AST_IMPORT: {

				}
				break;
				case AST_FUNCTION: {
				}
				break;
				case AST_NUMBER_LITERAL:
				case AST_INTEGER_LITERAL:
				case AST_STRING_LITERAL: {
				}
				break;
				default: {
					ASSERT(!"Codex: proper error reporting here");
				}
				break;
			}
		}
	}
}






#if 0
typedef u32 BindingId;

enum
{
	BINDING_INVALID = 0,
};

typedef enum
{
	BINDING_LOCAL,
	BINDING_PARAMETER,
	BINDING_GLOBAL,
}
BindingKind;

typedef struct
{
	BindingKind kind;
	Atom       *name;
	Ast         declaration;
	Ast         owner_function;
}
Binding;

typedef struct
{
	Ast       ast;
	BindingId binding;
}
ResolvedName;

typedef struct
{
	Ast_Array      functions;
	Binding      *bindings;
	u32           binding_count;
	ResolvedName *names;
	u32           name_count;
}
ResolvedModule;

typedef struct
{
	Compiler       *compiler;
	ResolvedModule *module;
	BindingId      *visible;
	u32             visible_count;
	u32             visible_capacity;
	u32             binding_capacity;
	u32             name_capacity;
	Ast             function;
}
NameResolver;

static void *grow_array(elf_Arena *arena, void *items, u32 count, u32 *capacity, u64 item_size, u64 alignment)
{
	u32 new_capacity = *capacity ? *capacity * 2 : 16;
	void *result = dy_arena_push_zero(arena, item_size * new_capacity, alignment);
	if (items) {
		copy_memory(result, items, item_size * count);
	}
	*capacity = new_capacity;
	return result;
}

static void resolve_ast(NameResolver *resolver, Ast ast);

static void record_name(NameResolver *resolver, Ast ast, BindingId binding)
{
	ResolvedModule *module = resolver->module;
	if (module->name_count == resolver->name_capacity) {
		module->names = grow_array(resolver->compiler->arena, module->names, module->name_count, &resolver->name_capacity, sizeof(*module->names), _Alignof(ResolvedName));
	}
	module->names[module->name_count++] = (ResolvedName) {ast, binding};
}

static BindingId find_visible_binding(NameResolver *resolver, Atom *name)
{
	for (u32 i = resolver->visible_count; i > 0; --i)
	{
		BindingId id = resolver->visible[i - 1];
		if (resolver->module->bindings[id - 1].name == name) {
			return id;
		}
	}
	return BINDING_INVALID;
}

static BindingId declare_binding(NameResolver *resolver, Ast declaration, BindingKind kind)
{
	ASSERT(declaration && declaration->kind == AST_IDENT);
	ResolvedModule *module = resolver->module;
	if (module->binding_count == resolver->binding_capacity) {
		module->bindings = grow_array(resolver->compiler->arena, module->bindings, module->binding_count, &resolver->binding_capacity, sizeof(*module->bindings), _Alignof(Binding));
	}
	if (resolver->visible_count == resolver->visible_capacity) {
		resolver->visible = grow_array(resolver->compiler->arena, resolver->visible, resolver->visible_count, &resolver->visible_capacity, sizeof(*resolver->visible), _Alignof(BindingId));
	}
	BindingId id = ++module->binding_count;
	module->bindings[id - 1] = (Binding) {kind, declaration->atom, declaration, resolver->function};
	resolver->visible[resolver->visible_count++] = id;
	record_name(resolver, declaration, id);
	return id;
}

static BindingId global_binding(NameResolver *resolver, Atom *name)
{
	ResolvedModule *module = resolver->module;
	for (u32 i = 0; i < module->binding_count; ++i)
	{
		Binding *binding = module->bindings + i;
		if (binding->kind == BINDING_GLOBAL && binding->name == name) {
			return i + 1;
		}
	}
	if (module->binding_count == resolver->binding_capacity) {
		module->bindings = grow_array(resolver->compiler->arena, module->bindings, module->binding_count, &resolver->binding_capacity, sizeof(*module->bindings), _Alignof(Binding));
	}
	BindingId id = ++module->binding_count;
	module->bindings[id - 1] = (Binding) {BINDING_GLOBAL, name, 0, 0};
	return id;
}

static void declare_names(NameResolver *resolver, Ast ast, BindingKind kind)
{
	if (ast->kind == AST_TUPLE)
	{
		for (u32 i = 0; i < ast->tuple.count; ++i) {
			declare_names(resolver, ast->tuple.items[i], kind);
		}
	}
	else
	{
		declare_binding(resolver, ast, kind);
	}
}

static void resolve_array(NameResolver *resolver, Ast_Array array)
{
	for (u32 i = 0; i < array.count; ++i) {
		resolve_ast(resolver, array.items[i]);
	}
}

static void resolve_ast(NameResolver *resolver, Ast ast)
{
	if (!ast || ast_is_error(ast)) {
		return;
	}

	switch (ast->kind)
	{
		case AST_IDENT:
		{
			BindingId binding = find_visible_binding(resolver, ast->atom);
			if (binding == BINDING_INVALID) {
				binding = global_binding(resolver, ast->atom);
			}
			record_name(resolver, ast, binding);
		}
		break;

		case AST_FUNCTION:
		{
			u32 scope_start = resolver->visible_count;
			Ast outer_function = resolver->function;
			resolver->function = ast;
			for (u32 i = 0; i < ast->function.params.count; ++i)
			{
				Ast param = ast->function.params.items[i];
				resolve_ast(resolver, param->param.expr);
				declare_binding(resolver, param->param.name, BINDING_PARAMETER);
			}
			resolve_ast(resolver, ast->function.body);
			resolver->visible_count = scope_start;
			resolver->function = outer_function;
		}
		break;

		case AST_BLOCK_STAT:
		{
			u32 scope_start = resolver->visible_count;
			resolve_array(resolver, ast->block);
			resolver->visible_count = scope_start;
		}
		break;

		case AST_DECL_STAT:
		{
			resolve_ast(resolver, ast->decl.expr);
			declare_names(resolver, ast->decl.name, BINDING_LOCAL);
		}
		break;

		case AST_TUPLE:
		{
			resolve_array(resolver, ast->tuple);
		}
		break;

		case AST_CALL:
		{
			resolve_ast(resolver, ast->call.expr);
			resolve_array(resolver, ast->call.args);
			resolve_ast(resolver, ast->call.carg);
		}
		break;

		case AST_FIELD:
		{
			resolve_ast(resolver, ast->binary.x);
		}
		break;

		case AST_RETURN:
		{
			resolve_ast(resolver, ast->return_stat.expr);
		}
		break;

		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_ASSIGN:
		{
			resolve_ast(resolver, ast->binary.x);
			resolve_ast(resolver, ast->binary.y);
		}
		break;

		default: break;
	}
}



ResolvedModule *static_analyze_names(Compiler *compiler, Ast_Array functions)
{
	ASSERT(compiler);
	ResolvedModule *module = dy_arena_push_zero(compiler->arena, sizeof(*module), _Alignof(ResolvedModule));
	module->functions = functions;

	NameResolver resolver = {};
	resolver.compiler = compiler;
	resolver.module = module;
	resolve_array(&resolver, functions);
	return module;
}

#endif
