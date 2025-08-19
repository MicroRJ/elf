//
// See Copyright Notice In elf.h
//


#define checknumargs(S, n, x) do \
{ if ((n) != (x)) elf_errorf(S, -1, "wrong number of arguments: %i, expected %i instead", n - 1, x - 1); \
} while(0)


ELF_FUNCTION(l_math_sgn) {
	checknumargs(S, nargs, 2);

	V vx = loadvalue(S, 1);
	ASSERT(visnumeric(vx));

	if (isnum(vx)) {
		Num x=vitonum(vx);
		pushint(S, x < 0 ? -1 : x > 0 ? +1 : 0);
	}
	else {
		Int x=vgetint(vx);
		pushint(S, x < 0 ? -1 : x > 0 ? +1 : 0);
	}
	return 1;
}



ELF_FUNCTION(l_math_abs) {
	checknumargs(S, nargs, 2);

	V vx = loadvalue(S, 1);
	ASSERT(visnumeric(vx));

	if (isnum(vx)) {
		Num x=vitonum(vx);
		pushnum(S, x < 0 ? -x : x);
	}
	else {
		Int x=vgetint(vx);
		pushint(S, x < 0 ? -x : x);
	}
	return 1;
}



ELF_FUNCTION(l_math_max) {
	checknumargs(S, nargs, 3);

	V vx = loadvalue(S, 1);
	V vy = loadvalue(S, 2);
	ASSERT(visnumeric(vx) && visnumeric(vy));


	if (isnum(vx) || isnum(vy)) {
		Num x, y;
		x = vitonum(vx);
		y = vitonum(vy);

		pushnum(S, x > y ? x : y);
	}
	else {
		Int x, y;
		x = vgetint(vx);
		y = vgetint(vy);

		pushint(S, x > y ? x : y);
	}
	return 1;
}


ELF_FUNCTION(l_math_min) {
	checknumargs(S, nargs, 3);

	V vx = loadvalue(S, 1);
	V vy = loadvalue(S, 2);
	ASSERT(visnumeric(vx) && visnumeric(vy));


	if (isnum(vx) || isnum(vy)) {
		Num x, y;
		x = vitonum(vx);
		y = vitonum(vy);

		pushnum(S, x < y ? x : y);
	}
	else {
		Int x, y;
		x = vgetint(vx);
		y = vgetint(vy);

		pushint(S, x < y ? x : y);
	}
	return 1;
}


ELF_FUNCTION(l_math_trim) {
	checknumargs(S, nargs, 4);

	V vx = loadvalue(S, 1);
	V vy = loadvalue(S, 2);
	V vz = loadvalue(S, 3);
	ASSERT(visnumeric(vx) && visnumeric(vy) && visnumeric(vz));


	if (isnum(vx) || isnum(vy) || isnum(vz)) {
		Num x, y, z;
		x = vitonum(vx);
		y = vitonum(vy);
		z = vitonum(vz);
		pushnum(S, x < y ? y : x > z ? z : x);
	}
	else {
		Int x, y, z;
		x = vgetint(vx);
		y = vgetint(vy);
		z = vgetint(vz);
		pushint(S, x < y ? y : x > z ? z : x);
	}
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
	{"trim",l_math_trim},
	{"sgn",l_math_sgn},
	{"exp",l_math_exp},
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