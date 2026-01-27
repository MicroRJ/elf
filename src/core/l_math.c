//
// See Copyright Notice In elf.h
//


// todo: most of these are to be made intrinsic!


#define checknumargs(S, n, x) do \
{ if ((n) != (x)) reporterrorf(S, -1, "wrong number of arguments: %i, expected %i instead", n - 1, x - 1); \
} while(0)


ELF_FUNCTION(l_math_sgn)
{
	checknumargs(S, nargs, 2);

	V vx = loadnumeric(S, 1);

	if (is_num(vx)) {
		Num x=int_to_num(vx);
		pushint(S, x < 0 ? -1 : x > 0 ? +1 : 0);
	}
	else {
		Int x=as_int(vx);
		pushint(S, x < 0 ? -1 : x > 0 ? +1 : 0);
	}
	return 1;
}

ELF_FUNCTION(l_math_abs)
{
	checknumargs(S, nargs, 2);

	V vx = loadnumeric(S, 1);

	if (is_num(vx)) {
		Num x=as_num(vx);
		pushnum(S, x < 0 ? -x : x);
	}
	else {
		Int x=as_int(vx);
		pushint(S, x < 0 ? -x : x);
	}
	return 1;
}

static inline void pushinorder(elf_State *S, int inorder, V x, V y)
{
	if (inorder) {
		pushvalueunsafe(S, x);
		pushvalueunsafe(S, y);
	}
	else {
		pushvalueunsafe(S, y);
		pushvalueunsafe(S, x);
	}
}

ELF_FUNCTION(l_math_max)
{
	checknumargs(S, nargs, 3);

	V vx = loadnumeric(S, 1);
	V vy = loadnumeric(S, 2);


	if (is_num(vx) || is_num(vy)) {
		Num x, y;
		x = int_to_num(vx);
		y = int_to_num(vy);

		pushinorder(S, x > y, vx, vy);
	}
	else {
		Int x, y;
		x = as_int(vx);
		y = as_int(vy);

		pushinorder(S, x > y, vx, vy);
	}

	return 2;
}

ELF_FUNCTION(l_math_min)
{
	checknumargs(S, nargs, 3);

	V vx = loadnumeric(S, 1);
	V vy = loadnumeric(S, 2);


	if (is_num(vx) || is_num(vy))
	{
		Num x, y;
		x = int_to_num(vx);
		y = int_to_num(vy);

		pushinorder(S, x < y, vx, vy);
	}
	else {
		Int x, y;
		x = as_int(vx);
		y = as_int(vy);

		pushinorder(S, x < y, vx, vy);
	}

	return 2;
}

ELF_FUNCTION(l_math_trim)
{
	checknumargs(S, nargs, 4);

	V vx = loadnumeric(S, 1);
	V vy = loadnumeric(S, 2);
	V vz = loadnumeric(S, 3);


	if (is_num(vx) || is_num(vy) || is_num(vz)) {
		Num x, y, z;
		x = int_to_num(vx);
		y = int_to_num(vy);
		z = int_to_num(vz);
		pushnum(S, x < y ? y : x > z ? z : x);
	}
	else {
		Int x, y, z;
		x = as_int(vx);
		y = as_int(vy);
		z = as_int(vz);
		pushint(S, x < y ? y : x > z ? z : x);
	}
	return 1;
}

ELF_FUNCTION(l_math_mix)
{
	checknumargs(S, nargs, 4);

	V vx = loadnumeric(S, 1);
	V vy = loadnumeric(S, 2);
	V vz = loadnumeric(S, 3);

	Num x, y, z;
	x = int_to_num(vx);
	y = int_to_num(vy);
	z = int_to_num(vz);

	Num r = x + (y - x) * z;
	pushnum(S, r);
	return 1;
}

ELF_FUNCTION(l_math_log2) {
	pushnum(S,log2(loadnum(S,1)));
	return 1;
}



ELF_FUNCTION(l_math_exp) {
	pushnum(S,exp(loadnum(S,1)));
	return 1;
}



ELF_FUNCTION(l_math_floor) {
	pushnum(S,floor(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_ceil) {
	pushnum(S,ceil(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_sqrt) {
	pushnum(S,sqrt(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_pow) {
	pushnum(S,pow(loadnum(S,1),loadnum(S,2)));
	return 1;
}


ELF_FUNCTION(l_math_sin) {
	pushnum(S,sin(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_cos) {
	pushnum(S,cos(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_acos) {
	pushnum(S,acos(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_tan) {
	pushnum(S,tan(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(l_math_atan2) {
	pushnum(S,atan2(loadnum(S,1),loadnum(S,2)));
	return 1;
}

static elf_Binding lib_math[] = {
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