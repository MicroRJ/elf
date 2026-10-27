#include <stdio.h>
#include <string.h>

#include "elf.h"
#include <dy.h>
#include "base.h"
#include "elf_runtime.h"
#include "bytecode.h"

static void format_bytecode_instr(elf_State *state, elf_Prototype *function, Arena *arena, u32 index, Bytecode byte)
{
	arena_pushf(arena, "  %04u  %-24s x=%d y=%d z=%d  ", index, bytecode_type_name(byte.b_type), byte.b_x, byte.b_y, byte.b_z);

	switch (byte.b_type)
	{
		case BC_JUMP:
		{
			arena_pushf(arena, "jump %d -> %d", byte.b_x, (i32)index + byte.b_x);
		}
		break;

		case BC_JUMP_IF_ZERO:
		case BC_JUMP_IF_NONZERO:
		{
			arena_pushf(arena, "%s r%d -> %d", bytecode_type_name(byte.b_type), byte.b_y, (i32)index + byte.b_x);
		}
		break;

		case BC_RETURN:
		{
			arena_pushf(arena, "return r%d count=%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOAD_CAPTURE:
		{
			arena_pushf(arena, "r%d = capture[%d]", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOAD_NATIVE_FUNCTION:
		{
			arena_pushf(arena, "r%d = native_function[%d]", byte.b_x, byte.b_y);
			if (byte.b_y < 0 || (u32)byte.b_y >= state->native_function_count) {
				arena_push_text(arena, " // <bad-native-function>");
			}
		}
		break;

		case BC_LOAD_PROTOTYPE:
		{
			arena_pushf(arena, "r%d = prototype[%d]", byte.b_x, byte.b_y);
			if (byte.b_y < 0 || !prototype_index_is_valid(state, (u32)byte.b_y)) {
				arena_push_text(arena, " // <bad-prototype>");
			}
		}
		break;

		case BC_LOAD_CONSTANT_NUMBER:
		{
			arena_pushf(arena, "r%d = number[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < function->number_constant_count) {
				arena_pushf(arena, " // %f", function->num_data[byte.b_y]);
			}
		}
		break;

		case BC_LOAD_CONSTANT_STRING:
		{
			arena_pushf(arena, "r%d = string[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < function->string_count) {
				arena_pushf(arena, " // %s", elf_string_data(function->strings[byte.b_y]));
			}
		}
		break;

		case BC_LOAD_CONSTANT_INTEGER:
		{
			arena_pushf(arena, "r%d = integer[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < function->integer_constant_count) {
				arena_pushf(arena, " // %lli", function->int_data[byte.b_y]);
			}
		}
		break;

		case BC_LOAD_NIL:
		{
			arena_pushf(arena, "r%d = nil", byte.b_x);
		}
		break;

		case BC_MOVE:
		{
			arena_pushf(arena, "r%d = r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_GET_INDEX:
		{
			arena_pushf(arena, "r%d = r%d[r%d]", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SET_INDEX:
		{
			arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_GET_META_FIELD:
		{
			arena_pushf(arena, "r%d = metatable(r%d)[r%d]", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_GET_FIELD:
		{
			arena_pushf(arena, "r%d = r%d[r%d]", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SET_FIELD:
		{
			arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_MEASURE:
		{
			arena_pushf(arena, "r%d = measure(r%d)", byte.b_x, byte.b_y);
		}
		break;

		case BC_ARRAY_APPEND:
		{
			arena_pushf(arena, "array_append r%d, r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_CALL_DYNAMIC:
		{
			arena_pushf(arena, "call_dynamic base=r%d nargs=%d nout=%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_NEW_TABLE:
		{
			arena_pushf(arena, "r%d = table", byte.b_x);
		}
		break;

		case BC_NEW_ARRAY:
		{
			arena_pushf(arena, "r%d = array", byte.b_x);
		}
		break;

		case BC_NEW_CLOSURE:
		{
			arena_pushf(arena, "r%d = closure prototype[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && prototype_index_is_valid(state, (u32)byte.b_y))
			{
				elf_Prototype *prototype = proto_from_index(state, (u32)byte.b_y);
				arena_pushf(arena, " captures=r%d count=%u", byte.b_x, prototype->captures);
			}
		}
		break;

		case BC_CHECK_TYPE:
		{
			arena_pushf(arena, "check_type r%d mask=0x%04x", byte.b_x, (u32)(u16)byte.b_y);
		}
		break;

		case BC_LESS_THAN:
		case BC_LESS_THAN_EQ:
		case BC_EQ:
		case BC_NOT_EQ:
		case BC_MUL:
		case BC_DIV:
		case BC_ADD:
		case BC_SUB:
		case BC_MOD:
		case BC_POW:
		case BC_SHIFT_LEFT:
		case BC_SHIFT_RIGHT:
		case BC_BITWISE_XOR:
		case BC_BITWISE_OR:
		case BC_BITWISE_AND:
		{
			arena_pushf(arena, "r%d = r%d %s r%d", byte.b_x, byte.b_y, bytecode_type_name(byte.b_type), byte.b_z);
		}
		break;

		case BC_BITWISE_NOT:
		{
			arena_pushf(arena, "r%d = %s r%d", byte.b_x, bytecode_type_name(byte.b_type), byte.b_y);
		}
		break;

		default:
		{
			arena_push_text(arena, bytecode_type_name(byte.b_type));
		}
		break;
	}

	arena_push_text(arena, "\n");
}

static char *format_bytecode_function(elf_State *state, Arena *arena, u32 prototype_index)
{
	elf_Prototype *function = proto_from_index(state, prototype_index);
	char *start = arena_push(arena, 0, 1);

	arena_push_text(arena, "bytecode function\n");
	arena_pushf(arena, "  arity       = %u\n", function->arity);
	arena_pushf(arena, "  variadic    = %s\n", function->variadic ? "true" : "false");
	arena_pushf(arena, "  length      = %u\n", function->bytecode_count);
	arena_pushf(arena, "  captures    = %u\n", function->captures);
	arena_pushf(arena, "  stack_size  = %u\n", function->stack_size);
	arena_push_text(arena, "body\n");

	for (u32 i = 0; i < function->bytecode_count; ++i)
	{
		format_bytecode_instr(state, function, arena, i, function->bytecode[i]);
	}

	arena_push_text(arena, "end\n");
	arena_push_zero(arena, 1, 1);
	return start;
}

static void print_bytecode_function(elf_State *state, u32 prototype_index)
{
	Scratch scratch = scratch_begin();
	char *text = format_bytecode_function(state, scratch.arena, prototype_index);
	printf("%s", text);
	scratch_end(scratch);
}

static b32 read_source_file(elf_State *state, const char *path, elf_StrSlice *source)
{
	dy_File_Info info;
	if (dy_file_info(dy_string_from_cstring(path), &info).error || info.is_directory)
	{
		return 0;
	}
	dy_File file;
	if (dy_file_open(dy_string_from_cstring(path), DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		return 0;
	}
	u64 size = info.size;
	char *data = arena_push(&state->arena, size + 16, 1);
	zero_memory(data + size, 16);
	dy_u64 read = 0;
	dy_Result read_result = dy_file_read(file, data, size, &read);
	dy_file_close(file);
	if (read_result.error || read != size)
	{
		return 0;
	}

	source->data = data;
	source->size = size;
	return 1;
}

static elf_Prototype *compile_source(elf_State *state, elf_StrSlice name, elf_StrSlice source)
{
	if (elf_push_code_source(state, 0, name, source) != ELF_ERROR_NONE)
	{
		return 0;
	}
	return value_as_closure(state->stack_top[-1])->prototype;
}

int main(int argc, char **argv)
{
	const char *path = "main.elf";
	if (argc > 2)
	{
		fprintf(stderr, "usage: bytecode.exe [file]\n");
		return 2;
	}

	if (argc == 2) {
		path = argv[1];
	}

	elf_State *state = elf_create_state();
	elf_StrSlice source = {};
	if (!read_source_file(state, path, &source))
	{
		fprintf(stderr, "bytecode: could not load '%s'\n", path);
		elf_destroy_state(state);
		return 1;
	}

	u32 first_prototype = state->prototype_count;
	elf_StrSlice source_path = {(char *)path, strlen(path)};
	elf_Prototype *entry = compile_source(state, source_path, source);
	if (!entry)
	{
		elf_destroy_state(state);
		return 1;
	}
	for (u32 i = first_prototype; i < state->prototype_count; ++i)
	{
		printf("function[%u]\n", i);
		print_bytecode_function(state, i);
	}
	elf_destroy_state(state);

	return 0;
}
