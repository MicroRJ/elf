/*
** See Copyright Notice In elf.h
** lib_math.c
*/


// todo: all of these should be instrinsics...

static int math_lib_exp(elf_State *R) {
	elf_add_num(R,exp(elf_get_num(R,0)));
	return 1;
}


static int math_lib_floor(elf_State *R) {
	elf_add_num(R,floor(elf_get_num(R,0)));
	return 1;
}


static int math_lib_ceil(elf_State *R) {
	elf_add_num(R,ceil(elf_get_num(R,0)));
	return 1;
}


static int math_lib_sqrt(elf_State *R) {
	elf_add_num(R,sqrt(elf_get_num(R,0)));
	return 1;
}


static int math_lib_pow(elf_State *R) {
	elf_add_num(R,pow(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}


static int math_lib_sin(elf_State *R) {
	elf_add_num(R,sin(elf_get_num(R,0)));
	return 1;
}


static int math_lib_cos(elf_State *R) {
	elf_add_num(R,cos(elf_get_num(R,0)));
	return 1;
}


static int math_lib_acos(elf_State *R) {
	elf_add_num(R,acos(elf_get_num(R,0)));
	return 1;
}


static int math_lib_tan(elf_State *R) {
	elf_add_num(R,tan(elf_get_num(R,0)));
	return 1;
}


static int math_lib_atan2(elf_State *R) {
	elf_add_num(R,atan2(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}

static NameFunctionPair lib_math[] = {
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