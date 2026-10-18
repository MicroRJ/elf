//
// Runtime materialization for compiler syntax objects.
//

#include "atom_view.h"
#include "ast.h"
#include "l_compile.h"

#if 0

static b32 lib_eval_constant_ast(elf_State *state, Compiler *compiler, Ast ast, elf_Value *out)
{
	if (!ast)
	{
		*out = value_nil();
		return true;
	}

	switch (ast->kind)
	{
		case AST_TUPLE:
		{
			if (ast->tuple.count != 1)
			{
				compiler_report(compiler, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, ERROR_PHASE_CHECKER, ast->site, "constant expression must produce one value");
				return false;
			}
			return lib_eval_constant_ast(state, compiler, ast->tuple.items[0], out);
		}

		case AST_NIL_LITERAL:
		{
			*out = value_nil();
			return true;
		}

		case AST_INTEGER_LITERAL:
		{
			*out = value_from_integer(ast->integer_value);
			return true;
		}

		case AST_NUMBER_LITERAL:
		{
			*out = value_from_number(ast->number_value);
			return true;
		}

		case AST_STRING_LITERAL:
		{
			*out = value_from_string(elf_string_from_data_size(state, atom_data(ast->atom), atom_size(ast->atom)));
			return true;
		}

		case AST_TABLE:
		{
			elf_Table *table = elf_table_new(state);
			for (u32 i = 0; i < ast->table.count; ++i)
			{
				Ast entry_ast = ast->table.items[i];
				if (!entry_ast || entry_ast->kind != AST_TABLE_ENTRY)
				{
					compiler_report(compiler, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, ERROR_PHASE_CHECKER, ast->site, "invalid table constant");
					return false;
				}

				elf_Value value = {};
				if (!lib_eval_constant_ast(state, compiler, entry_ast->table_entry.value, &value)) {
					return false;
				}
				if (!entry_ast->table_entry.key)
				{
					compiler_report(compiler, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, ERROR_PHASE_CHECKER, entry_ast->site, "invalid unkeyed table constant");
					return false;
				}

				elf_Value key = {};
				if (!lib_eval_constant_ast(state, compiler, entry_ast->table_entry.key, &key)) {
					return false;
				}
				elf_table_set(state, table, key, value);
			}
			*out = value_from_table(table);
			return true;
		}

		case AST_ARRAY:
		{
			elf_Array *array = elf_array_new(state);
			for (u32 i = 0; i < ast->array.count; ++i)
			{
				elf_Value value = {};
				if (!lib_eval_constant_ast(state, compiler, ast->array.items[i], &value)) {
					return false;
				}
				elf_array_add(state, array, value);
			}
			*out = value_from_array(array);
			return true;
		}

		default:
		{
			compiler_report(compiler, ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, ERROR_PHASE_CHECKER, ast->site, "expression is not a constant expression");
			return false;
		}
	}
}

static void lib_restore_compile_stack(elf_State *state, elf_Value *checkpoint)
{
	ASSERT(checkpoint >= state->frame->framebase && checkpoint <= state->stack_ptr);
	value_zero_many(checkpoint, state->stack_ptr - checkpoint);
	state->stack_ptr = checkpoint;
}

elf_Bool lib_push_compiler_ast(elf_State *state, Compiler *compiler, Ast ast)
{
	elf_Value *stack_checkpoint = state->stack_ptr;
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	elf_Value value = {};
	b32 result = !ast_is_error(ast) && lib_eval_constant_ast(state, compiler, ast, &value);
	if (result) {
		push_value(state, value);
	}
	else {
		lib_restore_compile_stack(state, stack_checkpoint);
	}
	ASSERT(!result || state->stack_ptr == stack_checkpoint + 1);
	state->gc_mode = saved_gc_mode;
	return result;
}

typedef enum
{
	LIB_PARSE_CONSTANT,
	LIB_PARSE_JSON,
	LIB_PARSE_JSONL,
}
LibParseType;

static b32 lib_push_parsed_source(elf_State *state, const char *name, elf_StrSlice source, LibParseType type)
{
	Scratch scratch = scratch_begin();
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, name, source);
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	Ast ast = ERROR_AST;
	if (type == LIB_PARSE_JSON)
	{
		ast = compiler_parse_json(scratch.arena, source_name, source);
	}
	else if (type == LIB_PARSE_JSONL)
	{
		ast = compiler_parse_jsonl(scratch.arena, source_name, source);
	}
	else
	{
		ast = compiler_parse_constant_expr(scratch.arena, source_name, source);
	}
	b32 result = lib_push_compiler_ast(state, compiler, ast);
	scratch_end(scratch);
	return result;
}

elf_Bool lib_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	return lib_push_parsed_source(state, name, source, LIB_PARSE_CONSTANT);
}

elf_Bool lib_push_json(elf_State *state, const char *name, elf_StrSlice source)
{
	return lib_push_parsed_source(state, name, source, LIB_PARSE_JSON);
}

elf_Bool lib_push_jsonl(elf_State *state, const char *name, elf_StrSlice source)
{
	return lib_push_parsed_source(state, name, source, LIB_PARSE_JSONL);
}
#endif