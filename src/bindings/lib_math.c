//
// See Copyright Notice In elf.h
//

// todo: all of these should be instrinsics...

ELF_FUNCTION(math_lib_exp) {
	elf_push_num(S,exp(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_floor) {
	elf_push_num(S,floor(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_ceil) {
	elf_push_num(S,ceil(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_sqrt) {
	elf_push_num(S,sqrt(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_pow) {
	elf_push_num(S,pow(elf_get_numarg(S,0),elf_get_numarg(S,1)));
	return 1;
}


ELF_FUNCTION(math_lib_sin) {
	elf_push_num(S,sin(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_cos) {
	elf_push_num(S,cos(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_acos) {
	elf_push_num(S,acos(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_tan) {
	elf_push_num(S,tan(elf_get_numarg(S,0)));
	return 1;
}


ELF_FUNCTION(math_lib_atan2) {
	elf_push_num(S,atan2(elf_get_numarg(S,0),elf_get_numarg(S,1)));
	return 1;
}

static elf_Binding lib_math[] = {
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