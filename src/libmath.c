/*
** See Copyright Notice In elf.h
** libmath.c
*/



static int math_lib_floor(elf_State *R);
static int math_lib_ceil(elf_State *R);
static int math_lib_sqrt(elf_State *R);
static int math_lib_pow(elf_State *R);
static int math_lib_sin(elf_State *R);
static int math_lib_cos(elf_State *R);
static int math_lib_acos(elf_State *R);
static int math_lib_tan(elf_State *R);
static int math_lib_atan2(elf_State *R);



void math_lib_include(elf_State *R) {
	elf_gsetx_cfn(R,"floor",math_lib_floor);
	elf_gsetx_cfn(R,"ceil",math_lib_ceil);
	elf_gsetx_cfn(R,"sqrt",math_lib_sqrt);
	elf_gsetx_cfn(R,"pow",math_lib_pow);
	elf_gsetx_cfn(R,"sin",math_lib_sin);
	elf_gsetx_cfn(R,"cos",math_lib_cos);
	elf_gsetx_cfn(R,"acos",math_lib_acos);
	elf_gsetx_cfn(R,"tan",math_lib_tan);
	elf_gsetx_cfn(R,"atan2",math_lib_atan2);
}


int math_lib_floor(elf_State *R) {
	elf_add_num(R,floor(elf_get_num(R,0)));
	return 1;
}


int math_lib_ceil(elf_State *R) {
	elf_add_num(R,ceil(elf_get_num(R,0)));
	return 1;
}


int math_lib_sqrt(elf_State *R) {
	elf_add_num(R,sqrt(elf_get_num(R,0)));
	return 1;
}


int math_lib_pow(elf_State *R) {
	elf_add_num(R,pow(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}


int math_lib_sin(elf_State *R) {
	elf_add_num(R,sin(elf_get_num(R,0)));
	return 1;
}


int math_lib_cos(elf_State *R) {
	elf_add_num(R,cos(elf_get_num(R,0)));
	return 1;
}


int math_lib_acos(elf_State *R) {
	elf_add_num(R,acos(elf_get_num(R,0)));
	return 1;
}


int math_lib_tan(elf_State *R) {
	elf_add_num(R,tan(elf_get_num(R,0)));
	return 1;
}


int math_lib_atan2(elf_State *R) {
	elf_add_num(R,atan2(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}
