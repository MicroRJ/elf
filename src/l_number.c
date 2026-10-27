//
// See Copyright Notice In elf.h
//

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#undef STB_SPRINTF_STATIC

enum
{
	NUMBER_FORMAT_MAX_PRECISION = 99,
};

static f64 number_receiver(elf_State *state)
{
	elf_Value value = load_value(state, 0);
	check_value_type(state, value, ELF_VALUE_TYPE_NUMBER);
	return value_as_number(value);
}

static b32 parse_number_precision(elf_String *format, i32 *precision)
{
	const char *data = elf_string_data(format);
	u32 size = elf_string_size(format);
	b32 valid = size >= 2 && data[0] == '.';
	u32 value = 0;

	for (u32 i = 1; valid && i < size; ++i)
	{
		char digit = data[i];
		if (digit < '0' || digit > '9')
		{
			valid = false;
		}
		else
		{
			value = value * 10 + (u32)(digit - '0');
			if (value > NUMBER_FORMAT_MAX_PRECISION)
			{
				valid = false;
			}
		}
	}

	if (valid)
	{
		*precision = (i32)value;
	}
	return valid;
}

static b32 push_fixed_number_text(elf_State *state, f64 number, i32 precision)
{
	int size = stbsp_snprintf(0, 0, "%.*f", precision, number);
	if (size < 0)
	{
		return false;
	}

	Scratch scratch = scratch_begin();
	char *data = arena_push(scratch.arena, (u64)size + 1, 1);
	b32 formatted = data != 0 && stbsp_snprintf(data, size + 1, "%.*f", precision, number) == size;
	if (formatted)
	{
		push_value(state, str_value(elf_string_from_data_size(state, data, (u32)size)));
	}
	scratch_end(scratch);
	return formatted;
}

ELF_FUNCTION(l_number_fmt)
{
	if (!lib_check_arg_count(S, "number.fmt", nargs - 1, 1, 1)) {
		return 1;
	}
	f64 number = number_receiver(S);
	elf_String *format = lib_load_string(S, 1);
	i32 precision = 0;

	if (!parse_number_precision(format, &precision))
	{
		elf_push_error(S, "number format must be '.' followed by a precision from 0 to 99");
	}
	else if (!push_fixed_number_text(S, number, precision))
	{
		elf_push_error(S, "could not format number");
	}
	return 1;
}

static const elf_Binding l_number[] = {
	{"fmt", l_number_fmt},
};

static elf_Table *elf_lib_number(elf_State *state)
{
	return new_binding_table(state, l_number, ARRAY_COUNT(l_number));
}
