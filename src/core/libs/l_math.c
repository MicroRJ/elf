//
// See Copyright Notice In elf.h
//

static elf_Value math_load_numeric(elf_State *state, int index)
{
	elf_Value value = load_value(state, index);
	check_numeric(state, value);
	return value;
}

static void math_push_in_order(elf_State *state, b32 in_order,
	elf_Value left, elf_Value right)
{
	push_value(state, in_order ? left : right);
	push_value(state, in_order ? right : left);
}

ELF_FUNCTION(l_math_sgn)
{
	lib_check_arg_count(S, "math.sgn", nargs, 1, 1);
	elf_Value value = math_load_numeric(S, 1);

	if (value_is_number(value)) {
		f64 number = value_as_number(value);
		elf_push_int(S, number < 0 ? -1 : number > 0 ? 1 : 0);
	} else {
		i64 integer = value_as_integer(value);
		elf_push_int(S, integer < 0 ? -1 : integer > 0 ? 1 : 0);
	}
	return 1;
}

ELF_FUNCTION(l_math_abs)
{
	lib_check_arg_count(S, "math.abs", nargs, 1, 1);
	elf_Value value = math_load_numeric(S, 1);

	if (value_is_number(value)) {
		f64 number = value_as_number(value);
		elf_push_num(S, number < 0 ? -number : number);
	} else {
		i64 integer = value_as_integer(value);
		elf_push_int(S, integer < 0 ? -integer : integer);
	}
	return 1;
}

ELF_FUNCTION(l_math_max)
{
	lib_check_arg_count(S, "math.max", nargs, 2, 2);
	elf_Value left = math_load_numeric(S, 1);
	elf_Value right = math_load_numeric(S, 2);
	b32 left_is_greater = value_is_number(left) || value_is_number(right)
		? value_to_number(left) > value_to_number(right)
		: value_as_integer(left) > value_as_integer(right);
	math_push_in_order(S, left_is_greater, left, right);
	return 2;
}

ELF_FUNCTION(l_math_min)
{
	lib_check_arg_count(S, "math.min", nargs, 2, 2);
	elf_Value left = math_load_numeric(S, 1);
	elf_Value right = math_load_numeric(S, 2);
	b32 left_is_less = value_is_number(left) || value_is_number(right)
		? value_to_number(left) < value_to_number(right)
		: value_as_integer(left) < value_as_integer(right);
	math_push_in_order(S, left_is_less, left, right);
	return 2;
}

ELF_FUNCTION(l_math_trim)
{
	lib_check_arg_count(S, "math.trim", nargs, 3, 3);
	elf_Value value = math_load_numeric(S, 1);
	elf_Value minimum = math_load_numeric(S, 2);
	elf_Value maximum = math_load_numeric(S, 3);

	if (value_is_number(value) || value_is_number(minimum) || value_is_number(maximum)) {
		f64 x = value_to_number(value);
		f64 lower = value_to_number(minimum);
		f64 upper = value_to_number(maximum);
		elf_push_num(S, x < lower ? lower : x > upper ? upper : x);
	} else {
		i64 x = value_as_integer(value);
		i64 lower = value_as_integer(minimum);
		i64 upper = value_as_integer(maximum);
		elf_push_int(S, x < lower ? lower : x > upper ? upper : x);
	}
	return 1;
}

ELF_FUNCTION(l_math_mix)
{
	lib_check_arg_count(S, "math.mix", nargs, 3, 3);
	f64 left = value_to_number(math_load_numeric(S, 1));
	f64 right = value_to_number(math_load_numeric(S, 2));
	f64 weight = value_to_number(math_load_numeric(S, 3));
	elf_push_num(S, left + (right - left) * weight);
	return 1;
}

ELF_FUNCTION(l_math_log2)
{
	lib_check_arg_count(S, "math.log2", nargs, 1, 1);
	elf_push_num(S, log2(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_exp)
{
	lib_check_arg_count(S, "math.exp", nargs, 1, 1);
	elf_push_num(S, exp(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_floor)
{
	lib_check_arg_count(S, "math.floor", nargs, 1, 1);
	elf_push_num(S, floor(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_ceil)
{
	lib_check_arg_count(S, "math.ceil", nargs, 1, 1);
	elf_push_num(S, ceil(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_sqrt)
{
	lib_check_arg_count(S, "math.sqrt", nargs, 1, 1);
	elf_push_num(S, sqrt(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_pow)
{
	lib_check_arg_count(S, "math.pow", nargs, 2, 2);
	f64 base = value_to_number(math_load_numeric(S, 1));
	f64 exponent = value_to_number(math_load_numeric(S, 2));
	elf_push_num(S, pow(base, exponent));
	return 1;
}

ELF_FUNCTION(l_math_sin)
{
	lib_check_arg_count(S, "math.sin", nargs, 1, 1);
	elf_push_num(S, sin(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_cos)
{
	lib_check_arg_count(S, "math.cos", nargs, 1, 1);
	elf_push_num(S, cos(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_acos)
{
	lib_check_arg_count(S, "math.acos", nargs, 1, 1);
	elf_push_num(S, acos(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_tan)
{
	lib_check_arg_count(S, "math.tan", nargs, 1, 1);
	elf_push_num(S, tan(value_to_number(math_load_numeric(S, 1))));
	return 1;
}

ELF_FUNCTION(l_math_atan2)
{
	lib_check_arg_count(S, "math.atan2", nargs, 2, 2);
	f64 y = value_to_number(math_load_numeric(S, 1));
	f64 x = value_to_number(math_load_numeric(S, 2));
	elf_push_num(S, atan2(y, x));
	return 1;
}

static const elf_Binding lib_math[] = {
	{"abs",   l_math_abs},
	{"max",   l_math_max},
	{"min",   l_math_min},
	{"mix",   l_math_mix},
	{"trim",  l_math_trim},
	{"sgn",   l_math_sgn},
	{"exp",   l_math_exp},
	{"log2",  l_math_log2},
	{"floor", l_math_floor},
	{"ceil",  l_math_ceil},
	{"sqrt",  l_math_sqrt},
	{"pow",   l_math_pow},
	{"sin",   l_math_sin},
	{"cos",   l_math_cos},
	{"acos",  l_math_acos},
	{"tan",   l_math_tan},
	{"atan2", l_math_atan2},
};

static elf_Table *elf_lib_math(elf_State *state)
{
	return new_binding_table(state, lib_math, ARRAY_COUNT(lib_math));
}
