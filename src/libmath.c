/*
** See Copyright Notice In elf.h
** libmath.c
*/



static int math_lib_floor(elf_Shell *R);
static int math_lib_ceil(elf_Shell *R);
static int math_lib_sqrt(elf_Shell *R);
static int math_lib_pow(elf_Shell *R);
static int math_lib_sin(elf_Shell *R);
static int math_lib_cos(elf_Shell *R);
static int math_lib_acos(elf_Shell *R);
static int math_lib_tan(elf_Shell *R);
static int math_lib_atan2(elf_Shell *R);



void math_lib_include(elf_Shell *R) {
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


int math_lib_floor(elf_Shell *R) {
	elf_put_number(R,floor(elf_get_number(R,0)));
	return 1;
}


int math_lib_ceil(elf_Shell *R) {
	elf_put_number(R,ceil(elf_get_number(R,0)));
	return 1;
}


int math_lib_sqrt(elf_Shell *R) {
	elf_put_number(R,sqrt(elf_get_number(R,0)));
	return 1;
}


int math_lib_pow(elf_Shell *R) {
	elf_put_number(R,pow(elf_get_number(R,0),elf_get_number(R,1)));
	return 1;
}


int math_lib_sin(elf_Shell *R) {
	elf_put_number(R,sin(elf_get_number(R,0)));
	return 1;
}


int math_lib_cos(elf_Shell *R) {
	elf_put_number(R,cos(elf_get_number(R,0)));
	return 1;
}


int math_lib_acos(elf_Shell *R) {
	elf_put_number(R,acos(elf_get_number(R,0)));
	return 1;
}


int math_lib_tan(elf_Shell *R) {
	elf_put_number(R,tan(elf_get_number(R,0)));
	return 1;
}


int math_lib_atan2(elf_Shell *R) {
	elf_put_number(R,atan2(elf_get_number(R,0),elf_get_number(R,1)));
	return 1;
}
