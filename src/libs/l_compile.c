//
// Runtime materialization for compiler syntax objects.
//

#include "atom_view.h"
#include "ast.h"
#include "l_compile.h"

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
			if (ast->tuple.nargs != 1)
			{
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "constant expression must produce one value");
				return false;
			}
			return lib_eval_constant_ast(state, compiler, ast->tuple.args[0], out);
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
			for (u32 i = 0; i < ast->table.nargs; ++i)
			{
				Ast entry_ast = ast->table.args[i];
				if (!entry_ast || entry_ast->kind != AST_TABLE_ENTRY)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "invalid table constant");
					return false;
				}

				elf_Value value = {};
				if (!lib_eval_constant_ast(state, compiler, entry_ast->table_entry.value, &value)) {
					return false;
				}
				if (!entry_ast->table_entry.key)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, entry_ast->site, "invalid unkeyed table constant");
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
			for (u32 i = 0; i < ast->array.nargs; ++i)
			{
				elf_Value value = {};
				if (!lib_eval_constant_ast(state, compiler, ast->array.args[i], &value)) {
					return false;
				}
				elf_array_add(state, array, value);
			}
			*out = value_from_array(array);
			return true;
		}

		default:
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "expression is not a constant expression");
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

static b32 lib_push_parsed_source(elf_State *state, const char *name, elf_StrSlice source, b32 json)
{
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, name, source);
	Ast ast = json ? compiler_parse_json(compiler) : compiler_parse_constant_expr(compiler);
	b32 result = lib_push_compiler_ast(state, compiler, ast);
	elf_end_scratch(scratch);
	return result;
}

elf_Bool lib_push_constant_expr(elf_State *state, const char *name, elf_StrSlice source)
{
	return lib_push_parsed_source(state, name, source, false);
}

elf_Bool lib_push_json(elf_State *state, const char *name, elf_StrSlice source)
{
	return lib_push_parsed_source(state, name, source, true);
}
