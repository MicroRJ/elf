/*
** See Copyright Notice In elf.h
** lib_math.c
*/


// todo: all of these should be instrinsics...

int math_lib_floor(elf_State *R) {
	elf_push_number(R,floor(elf_get_num(R,0)));
	return 1;
}


int math_lib_ceil(elf_State *R) {
	elf_push_number(R,ceil(elf_get_num(R,0)));
	return 1;
}


int math_lib_sqrt(elf_State *R) {
	elf_push_number(R,sqrt(elf_get_num(R,0)));
	return 1;
}


int math_lib_pow(elf_State *R) {
	elf_push_number(R,pow(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}


int math_lib_sin(elf_State *R) {
	elf_push_number(R,sin(elf_get_num(R,0)));
	return 1;
}


int math_lib_cos(elf_State *R) {
	elf_push_number(R,cos(elf_get_num(R,0)));
	return 1;
}


int math_lib_acos(elf_State *R) {
	elf_push_number(R,acos(elf_get_num(R,0)));
	return 1;
}


int math_lib_tan(elf_State *R) {
	elf_push_number(R,tan(elf_get_num(R,0)));
	return 1;
}


int math_lib_atan2(elf_State *R) {
	elf_push_number(R,atan2(elf_get_num(R,0),elf_get_num(R,1)));
	return 1;
}
