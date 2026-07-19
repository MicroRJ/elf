//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "elf.h"
#include "base.h"
#include "core.h"
#include "bytecode_debug.h"

static void format_value_shallow(elf_Arena *arena, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:       elf_arena_push_text(arena, "nil"); break;
		case ELF_VALUE_TYPE_INTEGER:   elf_arena_pushf(arena, "%lli", value_as_integer(value)); break;
		case ELF_VALUE_TYPE_NUMBER:    elf_arena_pushf(arena, "%f", value.x_num); break;
		case ELF_VALUE_TYPE_ATOM:      elf_arena_pushf(arena, "%s", atom_data(value_as_atom(value))); break;
		case ELF_VALUE_TYPE_CLOSURE:   elf_arena_push_text(arena, "closure"); break;
		case ELF_VALUE_TYPE_CFUNCTION: elf_arena_push_text(arena, "function"); break;
		case ELF_VALUE_TYPE_TABLE:     elf_arena_push_text(arena, "table"); break;
		default:                       elf_arena_push_text(arena, "?"); break;
	}
}

static void format_global_value(elf_State *state, elf_Arena *arena, u32 index)
{
	if (index >= elf_array_length(state->globals))
	{
		elf_arena_push_text(arena, "<bad-global>");
		return;
	}

	format_value_shallow(arena, elf_array_get(state, state->globals, index));
}

static void format_bytecode_instr(elf_State *state, elf_Arena *arena, u32 index, Bytecode byte)
{
	elf_arena_pushf(arena, "  %04u  %-16s x=%d y=%d z=%d  "
		, index, bytecode_type_name(byte.b_type), byte.b_x, byte.b_y, byte.b_z);

	switch (byte.b_type)
	{
		case BC_HALT:
		case BC_NOP:
		{
			elf_arena_push_text(arena, bytecode_type_name(byte.b_type));
		}
		break;

		case BC_JUMP:
		{
			elf_arena_pushf(arena, "jump %d -> %d", byte.b_x, (i32)index + byte.b_x);
		}
		break;

		case BC_JZ:
		case BC_JNZ:
		case BC_JE:
		case BC_JNE:
		{
			elf_arena_pushf(arena, "%s r%d r%d -> %d"
				, bytecode_type_name(byte.b_type), byte.b_y, byte.b_z, (i32)index + byte.b_x);
		}
		break;

		case BC_RETURN:
		{
			elf_arena_pushf(arena, "return r%d count=%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOADCVAL:
		{
			elf_arena_pushf(arena, "r%d = capture[%d]", byte.b_x, byte.b_y);
		}
		break;

		case BC_GETGLOBAL:
		{
			elf_arena_pushf(arena, "r%d = global[%d] // ", byte.b_x, byte.b_y);
			format_global_value(state, arena, byte.b_y);
		}
		break;

		case BC_SETGLOBAL:
		{
			elf_arena_pushf(arena, "global[%d] = r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOADKNUM:
		{
			elf_arena_pushf(arena, "r%d = number[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < state->number_constant_count) {
				elf_arena_pushf(arena, " // %f", state->number_constants[byte.b_y]);
			}
		}
		break;

		case BC_LOADKINT:
		{
			elf_arena_pushf(arena, "r%d = integer[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < state->integer_constant_count) {
				elf_arena_pushf(arena, " // %lli", state->integer_constants[byte.b_y]);
			}
		}
		break;

		case BC_LOADNIL:
		{
			elf_arena_pushf(arena, "r%d = nil", byte.b_x);
		}
		break;

		case BC_RELOAD:
		{
			elf_arena_pushf(arena, "r%d = r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_CURRENT_CLOSURE:
		{
			elf_arena_pushf(arena, "r%d = current_closure", byte.b_x);
		}
		break;

		case BC_GETINDEX:
		{
			elf_arena_pushf(arena, "r%d = r%d[r%d]", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SETINDEX:
		{
			elf_arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_GETMETAFIELD:
		case BC_GETFIELD:
		{
			elf_arena_pushf(arena, "r%d = r%d.%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SETFIELD:
		{
			elf_arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_GETLENGTH:
		{
			elf_arena_pushf(arena, "r%d = len(r%d)", byte.b_x, byte.b_y);
		}
		break;

		case BC_ARRAYADD:
		{
			elf_arena_pushf(arena, "arrayadd r%d, r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_CALL:
		{
			elf_arena_pushf(arena, "call r%d nargs=%d nrets=%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_TABLE:
		{
			elf_arena_pushf(arena, "r%d = table", byte.b_x);
		}
		break;

		case BC_CLOSURE:
		{
			elf_arena_pushf(arena, "r%d = closure function[%d]", byte.b_x, byte.b_y);
		}
		break;

		case BC_ENFORCE:
		{
			elf_arena_pushf(arena, "enforce r%d rule=%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LT:
		case BC_LTEQ:
		case BC_EQ:
		case BC_NEQ:
		case BC_MUL:
		case BC_DIV:
		case BC_ADD:
		case BC_SUB:
		case BC_MOD:
		case BC_POW:
		case BC_BIT_SHL:
		case BC_BIT_SHR:
		case BC_BIT_XOR:
		case BC_BIT_OR:
		case BC_BIT_AND:
		{
			elf_arena_pushf(arena, "r%d = r%d %s r%d"
				, byte.b_x, byte.b_y, bytecode_type_name(byte.b_type), byte.b_z);
		}
		break;

		case BC_BIT_NOT:
		case BC_I2N:
		case BC_N2I:
		{
			elf_arena_pushf(arena, "r%d = %s r%d"
				, byte.b_x, bytecode_type_name(byte.b_type), byte.b_y);
		}
		break;

		default:
		{
			elf_arena_push_text(arena, bytecode_type_name(byte.b_type));
		}
		break;
	}

	elf_arena_push_char(arena, '\n');
}

char *format_bytecode_function(elf_State *state, elf_Arena *arena, BcFunction function)
{
	char *start = elf_arena_push(arena, 0);
	const char *source_name = function.source_name ? atom_data(function.source_name) : "<unknown>";

	elf_arena_push_text(arena, "bytecode function\n");
	elf_arena_pushf(arena, "  source      = %s\n", source_name);
	elf_arena_pushf(arena, "  arity       = %u\n", function.arity);
	elf_arena_pushf(arena, "  variadic    = %s\n", function.variadic ? "true" : "false");
	elf_arena_pushf(arena, "  offset      = %u\n", function.offset);
	elf_arena_pushf(arena, "  length      = %u\n", function.length);
	elf_arena_pushf(arena, "  captures    = %u\n", function.captures);
	elf_arena_pushf(arena, "  stack_size  = %u\n", function.stack_size);
	elf_arena_push_text(arena, "body\n");

	Bytecode *bytecode = state->bytecode + function.offset;
	for (u32 i = 0; i < function.length; ++ i)
	{
		format_bytecode_instr(state, arena, i, bytecode[i]);
	}

	elf_arena_push_text(arena, "end\n");
	elf_arena_push_zero(arena, 1);
	return start;
}

void print_bytecode_function(elf_State *state, BcFunction function)
{
	elf_Scratch scratch = elf_begin_scratch();
	char *text = format_bytecode_function(state, scratch.arena, function);
	printf("%s", text);
	elf_end_scratch(scratch);
}
