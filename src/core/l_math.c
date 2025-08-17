//
// See Copyright Notice In elf.h
//


// todo: all of these should be instrinsics...


ELF_FUNCTION(math_lib_sgn) {
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



ELF_FUNCTION(math_lib_abs) {
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



ELF_FUNCTION(math_lib_max) {
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


ELF_FUNCTION(math_lib_min) {
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


ELF_FUNCTION(math_lib_trim) {
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



ELF_FUNCTION(math_lib_exp) {
	pushnum(S,exp(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_floor) {
	pushnum(S,floor(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_ceil) {
	pushnum(S,ceil(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_sqrt) {
	pushnum(S,sqrt(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_pow) {
	pushnum(S,pow(loadnum(S,1),loadnum(S,2)));
	return 1;
}


ELF_FUNCTION(math_lib_sin) {
	pushnum(S,sin(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_cos) {
	pushnum(S,cos(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_acos) {
	pushnum(S,acos(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_tan) {
	pushnum(S,tan(loadnum(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_atan2) {
	pushnum(S,atan2(loadnum(S,1),loadnum(S,2)));
	return 1;
}

static elf_Binding lib_math[] = {
	{"abs",math_lib_abs},
	{"max",math_lib_max},
	{"min",math_lib_min},
	{"trim",math_lib_trim},
	{"sgn",math_lib_sgn},
	{"exp",math_lib_exp},
	{"floor",math_lib_floor},
	{"ceil",math_lib_ceil},
	{"sqrt",math_lib_sqrt},
	{"pow",math_lib_pow},
	{"sin",math_lib_sin},
	{"cos",math_lib_cos},
	{"acos",math_lib_acos},
	{"tan",math_lib_tan},
	{"atan2",math_lib_atan2},
};