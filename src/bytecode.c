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

static void format_value_shallow(Arena *arena, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL:         arena_push_text(arena, "nil"); break;
		case ELF_VALUE_TYPE_INTEGER:     arena_pushf(arena, "%lli", value_as_integer(value)); break;
		case ELF_VALUE_TYPE_NUMBER:      arena_pushf(arena, "%f", value.x_num); break;
		case ELF_VALUE_TYPE_STRING:      arena_pushf(arena, "%s", elf_string_data(value_as_string(value))); break;
		case ELF_VALUE_TYPE_CLOSURE:     arena_push_text(arena, "closure"); break;
		case ELF_VALUE_TYPE_CFUNCTION:   arena_push_text(arena, "function"); break;
		case ELF_VALUE_TYPE_TABLE:       arena_push_text(arena, "table"); break;
		case ELF_VALUE_TYPE_ERROR:       arena_push_text(arena, "error"); break;
		case ELF_VALUE_TYPE_BUFFER:      arena_push_text(arena, "buffer"); break;
		case ELF_VALUE_TYPE_ARRAY:       arena_push_text(arena, "array"); break;
		case ELF_VALUE_TYPE_USER_OBJECT: arena_push_text(arena, "user object"); break;
		default:                         arena_push_text(arena, "?"); break;
	}
}

static void format_global_value(elf_State *state, Arena *arena, u32 index)
{
	if (index >= state->globals.count)
	{
		arena_push_text(arena, "<bad-global>");
		return;
	}

	format_value_shallow(arena, elf_globals_get_by_slot(&state->globals, index));
}

static void format_bytecode_instr(elf_State *state, elf_Program *module, Arena *arena, u32 index, Bytecode byte)
{
	arena_pushf(arena, "  %04u  %-16s x=%d y=%d z=%d w=%u  ", index, bytecode_type_name(byte.b_type), byte.b_x, byte.b_y, byte.b_z, byte.b_w);

	switch (byte.b_type)
	{
		case BC_HALT:
		case BC_NOP:
		{
			arena_push_text(arena, bytecode_type_name(byte.b_type));
		}
		break;

		case BC_JUMP:
		{
			arena_pushf(arena, "jump %d -> %d", byte.b_x, (i32)index + byte.b_x);
		}
		break;

		case BC_JZ:
		case BC_JNZ:
		case BC_JE:
		case BC_JNE:
		{
			arena_pushf(arena, "%s r%d r%d -> %d", bytecode_type_name(byte.b_type), byte.b_y, byte.b_z, (i32)index + byte.b_x);
		}
		break;

		case BC_RETURN:
		{
			arena_pushf(arena, "return r%d count=%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOADCVAL:
		{
			arena_pushf(arena, "r%d = capture[%d]", byte.b_x, byte.b_y);
		}
		break;

		case BC_GETGLOBAL:
		{
			arena_pushf(arena, "r%d = global[%d] // ", byte.b_x, byte.b_y);
			format_global_value(state, arena, byte.b_y);
		}
		break;

		case BC_SETGLOBAL:
		{
			arena_pushf(arena, "global[%d] = r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_LOADKNUM:
		{
			arena_pushf(arena, "r%d = number[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < module->number_constant_count) {
				arena_pushf(arena, " // %f", module->number_constants[byte.b_y]);
			}
		}
		break;

		case BC_LOADKSTRING:
		{
			arena_pushf(arena, "r%d = string[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < module->string_count) {
				arena_pushf(arena, " // %s", elf_string_data(module->strings[byte.b_y]));
			}
		}
		break;

		case BC_LOADKINT:
		{
			arena_pushf(arena, "r%d = integer[%d]", byte.b_x, byte.b_y);
			if (byte.b_y >= 0 && (u32)byte.b_y < module->integer_constant_count) {
				arena_pushf(arena, " // %lli", module->integer_constants[byte.b_y]);
			}
		}
		break;

		case BC_LOADNIL:
		{
			arena_pushf(arena, "r%d = nil", byte.b_x);
		}
		break;

		case BC_RELOAD:
		{
			arena_pushf(arena, "r%d = r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_CURRENT_CLOSURE:
		{
			arena_pushf(arena, "r%d = current_closure", byte.b_x);
		}
		break;

		case BC_GETINDEX:
		{
			arena_pushf(arena, "r%d = r%d[r%d]", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SETINDEX:
		{
			arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_GETMETAFIELD:
		case BC_GETFIELD:
		{
			arena_pushf(arena, "r%d = r%d.%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_SETFIELD:
		{
			arena_pushf(arena, "r%d[r%d] = r%d", byte.b_x, byte.b_y, byte.b_z);
		}
		break;

		case BC_MEASURE:
		{
			arena_pushf(arena, "r%d = measure(r%d)", byte.b_x, byte.b_y);
		}
		break;

		case BC_ARRAYADD:
		{
			arena_pushf(arena, "arrayadd r%d, r%d", byte.b_x, byte.b_y);
		}
		break;

		case BC_DYNAMIC_CALL:
		{
			arena_pushf(arena, "dynamic_call callee=r%d result=r%d nargs=%d nrets=%d", byte.b_y, byte.b_x, byte.b_z, byte.b_w);
		}
		break;

		case BC_CALL:
		{
			arena_pushf(arena, "call function[%d] result=r%d nargs=%d nrets=%u", byte.b_y, byte.b_x, byte.b_z, byte.b_w);
		}
		break;

		case BC_TABLE:
		{
			arena_pushf(arena, "r%d = table", byte.b_x);
		}
		break;

		case BC_ARRAY:
		{
			arena_pushf(arena, "r%d = array", byte.b_x);
		}
		break;

		case BC_CLOSURE:
		{
			arena_pushf(arena, "r%d = closure function[%d]", byte.b_x, byte.b_y);
		}
		break;

		case BC_ENFORCE:
		{
			arena_pushf(arena, "enforce r%d rule=%d", byte.b_x, byte.b_y);
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
			arena_pushf(arena, "r%d = r%d %s r%d", byte.b_x, byte.b_y, bytecode_type_name(byte.b_type), byte.b_z);
		}
		break;

		case BC_BIT_NOT:
		case BC_I2N:
		case BC_N2I:
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

	arena_push_char(arena, '\n');
}

char *format_bytecode_function(elf_State *state, Arena *arena, elf_PrototypeRef function_ref)
{
	elf_Prototype *function = prototype_from_ref(function_ref);
	elf_Program *module = function_ref.program;
	char *start = arena_push(arena, 0, 1);
	const char *source_name = module->source_name ? elf_string_data(module->source_name) : "<unknown>";

	arena_push_text(arena, "bytecode function\n");
	arena_pushf(arena, "  source      = %s\n", source_name);
	arena_pushf(arena, "  arity       = %u\n", function->arity);
	arena_pushf(arena, "  variadic    = %s\n", function->variadic ? "true" : "false");
	arena_pushf(arena, "  offset      = %u\n", function->offset);
	arena_pushf(arena, "  length      = %u\n", function->length);
	arena_pushf(arena, "  captures    = %u\n", function->captures);
	arena_pushf(arena, "  stack_size  = %u\n", function->stack_size);
	arena_push_text(arena, "body\n");

	Bytecode *bytecode = module->bytecode + function->offset;
	for (u32 i = 0; i < function->length; ++i)
	{
		format_bytecode_instr(state, module, arena, i, bytecode[i]);
	}

	arena_push_text(arena, "end\n");
	arena_push_zero(arena, 1, 1);
	return start;
}

void print_bytecode_function(elf_State *state, elf_PrototypeRef function)
{
	Scratch scratch = scratch_begin();
	char *text = format_bytecode_function(state, scratch.arena, function);
	printf("%s", text);
	scratch_end(scratch);
}
