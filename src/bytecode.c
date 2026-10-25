//
// See Copyright Notice In elf.h
//

#include <stdio.h>

#include "elf.h"
#include "bytecode.h"
#include "elf_core.h"

const char *bytecode_type_name(BcType type)
{
	static const char *names[] =
	{
#define XPAND(ENUM, NAME) NAME,
		BC_XDEF(XPAND)
#undef XPAND
	};

	if ((u32)type >= BC_COUNT_) {
		return "unknown";
	}
	return names[type];
}

static void format_bytecode_instr(elf_State *state, elf_Prototype *function, Arena *arena, u32 index, Bytecode byte)
{
	arena_pushf(arena, "  %04u  %-24s x=%d y=%d z=%d w=%u  ", index, bytecode_type_name(byte.b_type), byte.b_x, byte.b_y, byte.b_z, byte.b_w);

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

		case BC_LOAD_CONSTANT_NUMBER:
		{
			arena_pushf(arena, "r%d = number[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < function->number_constant_count) {
				arena_pushf(arena, " // %f", function->number_constants[byte.b_y]);
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
				arena_pushf(arena, " // %lli", function->integer_constants[byte.b_y]);
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

		case BC_LOAD_CURRENT_CLOSURE:
		{
			arena_pushf(arena, "r%d = current_closure", byte.b_x);
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
			arena_pushf(arena, "call_dynamic callee=r%d result=r%d nargs=%d nrets=%u", byte.b_y, byte.b_x, byte.b_z, byte.b_w);
		}
		break;

		case BC_CALL_DIRECT:
		{
			arena_pushf(arena, "call_direct prototype[%d] result=r%d nargs=%d nrets=%u", byte.b_y, byte.b_x, byte.b_z, byte.b_w);
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
				elf_Prototype *prototype = prototype_from_index(state, (u32)byte.b_y);
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

char *format_bytecode_function(elf_State *state, Arena *arena, u32 prototype_index)
{
	elf_Prototype *function = prototype_from_index(state, prototype_index);
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

void print_bytecode_function(elf_State *state, u32 prototype_index)
{
	Scratch scratch = scratch_begin();
	char *text = format_bytecode_function(state, scratch.arena, prototype_index);
	printf("%s", text);
	scratch_end(scratch);
}
