//
// See Copyright Notice In elf.h
//


// todo: most of these are to be made intrinsic!


#define checknumargs(S, n, x) do \
{ if ((n) != (x)) elf_report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "wrong number of arguments: %i, expected %i instead", n - 1, x - 1); \
} while(0)


ELF_FUNCTION(l_math_sgn)
{
	checknumargs(S, nargs, 2);

	elf_Value vx = load_value(S, 1);
	check_value_type_rule(S, vx, TRULE_NUMERIC);

	if (value_is_number(vx))
	{
		f64 x = value_to_number(vx);
		push_value(S, value_from_integer(x < 0 ? -1 : x > 0 ? +1 : 0));
	}
	else
	{
		i64 x = value_as_integer(vx);
		push_value(S, value_from_integer(x < 0 ? -1 : x > 0 ? +1 : 0));
	}
	return 1;
}

ELF_FUNCTION(l_math_abs)
{
	checknumargs(S, nargs, 2);

	elf_Value vx = load_value(S, 1);
	check_value_type_rule(S, vx, TRULE_NUMERIC);

	if (value_is_number(vx)) {
		f64 x=value_as_number(vx);
		push_value(S, value_from_number(x < 0 ? -x : x));
	}
	else {
		i64 x=value_as_integer(vx);
		push_value(S, value_from_integer(x < 0 ? -x : x));
	}
	return 1;
}

static inline void pushinorder(elf_State *S, i64 inorder, elf_Value x, elf_Value y)
{
	if (inorder) {
		push_value(S, x);
		push_value(S, y);
	}
	else {
		push_value(S, y);
		push_value(S, x);
	}
}

ELF_FUNCTION(l_math_max)
{
	checknumargs(S, nargs, 3);

	elf_Value vx = load_value(S, 1);
	elf_Value vy = load_value(S, 2);
	check_value_type_rule(S, vx, TRULE_NUMERIC);
	check_value_type_rule(S, vy, TRULE_NUMERIC);


	if (value_is_number(vx) || value_is_number(vy)) {
		f64 x, y;
		x = value_to_number(vx);
		y = value_to_number(vy);

		pushinorder(S, x > y, vx, vy);
	}
	else {
		i64 x, y;
		x = value_as_integer(vx);
		y = value_as_integer(vy);

		pushinorder(S, x > y, vx, vy);
	}

	return 2;
}

ELF_FUNCTION(l_math_min)
{
	checknumargs(S, nargs, 3);

	elf_Value vx = load_value(S, 1);
	elf_Value vy = load_value(S, 2);
	check_value_type_rule(S, vx, TRULE_NUMERIC);
	check_value_type_rule(S, vy, TRULE_NUMERIC);


	if (value_is_number(vx) || value_is_number(vy))
	{
		f64 x, y;
		x = value_to_number(vx);
		y = value_to_number(vy);

		pushinorder(S, x < y, vx, vy);
	}
	else {
		i64 x, y;
		x = value_as_integer(vx);
		y = value_as_integer(vy);

		pushinorder(S, x < y, vx, vy);
	}

	return 2;
}

ELF_FUNCTION(l_math_trim)
{
	checknumargs(S, nargs, 4);

	elf_Value vx = load_value(S, 1);
	elf_Value vy = load_value(S, 2);
	elf_Value vz = load_value(S, 3);
	check_value_type_rule(S, vx, TRULE_NUMERIC);
	check_value_type_rule(S, vy, TRULE_NUMERIC);
	check_value_type_rule(S, vz, TRULE_NUMERIC);


	if (value_is_number(vx) || value_is_number(vy) || value_is_number(vz)) {
		f64 x, y, z;
		x = value_to_number(vx);
		y = value_to_number(vy);
		z = value_to_number(vz);
		push_value(S, value_from_number(x < y ? y : x > z ? z : x));
	}
	else {
		i64 x, y, z;
		x = value_as_integer(vx);
		y = value_as_integer(vy);
		z = value_as_integer(vz);
		push_value(S, value_from_integer(x < y ? y : x > z ? z : x));
	}
	return 1;
}

ELF_FUNCTION(l_math_mix)
{
	checknumargs(S, nargs, 4);

	elf_Value vx = load_value(S, 1);
	elf_Value vy = load_value(S, 2);
	elf_Value vz = load_value(S, 3);
	check_value_type_rule(S, vx, TRULE_NUMERIC);
	check_value_type_rule(S, vy, TRULE_NUMERIC);
	check_value_type_rule(S, vz, TRULE_NUMERIC);

	f64 x, y, z;
	x = value_to_number(vx);
	y = value_to_number(vy);
	z = value_to_number(vz);

	f64 r = x + (y - x) * z;
	push_value(S, value_from_number(r));
	return 1;
}

ELF_FUNCTION(l_math_log2) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(log2(value_to_number(value))));
	return 1;
}



ELF_FUNCTION(l_math_exp) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(exp(value_to_number(value))));
	return 1;
}



ELF_FUNCTION(l_math_floor) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(floor(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_ceil) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(ceil(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_sqrt) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(sqrt(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_pow) {
	elf_Value x = load_value(S, 1);
	elf_Value y = load_value(S, 2);
	check_value_type_rule(S, x, TRULE_NUMERIC);
	check_value_type_rule(S, y, TRULE_NUMERIC);
	push_value(S, value_from_number(pow(value_to_number(x), value_to_number(y))));
	return 1;
}


ELF_FUNCTION(l_math_sin) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(sin(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_cos) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(cos(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_acos) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(acos(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_tan) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_NUMERIC);
	push_value(S, value_from_number(tan(value_to_number(value))));
	return 1;
}


ELF_FUNCTION(l_math_atan2) {
	elf_Value y = load_value(S, 1);
	elf_Value x = load_value(S, 2);
	check_value_type_rule(S, y, TRULE_NUMERIC);
	check_value_type_rule(S, x, TRULE_NUMERIC);
	push_value(S, value_from_number(atan2(value_to_number(y), value_to_number(x))));
	return 1;
}

static const elf_Binding lib_math[] = {
	{"abs",l_math_abs},
	{"max",l_math_max},
	{"min",l_math_min},
	{"mix",l_math_mix},
	{"trim",l_math_trim},
	{"sgn",l_math_sgn},
	{"exp",l_math_exp},
	{"log2",l_math_log2},
	{"floor",l_math_floor},
	{"ceil",l_math_ceil},
	{"sqrt",l_math_sqrt},
	{"pow",l_math_pow},
	{"sin",l_math_sin},
	{"cos",l_math_cos},
	{"acos",l_math_acos},
	{"tan",l_math_tan},
	{"atan2",l_math_atan2},
};

static elf_Table *elf_lib_math(elf_State *state)
{
	return new_binding_table(state, lib_math, ARRAY_COUNT(lib_math));
}
