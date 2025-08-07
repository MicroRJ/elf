//
// See Copyright Notice In elf.h
//

// todo: all of these should be instrinsics...

ELF_FUNCTION(math_lib_exp) {
	elf_pushnum(S,exp(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_floor) {
	elf_pushnum(S,floor(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_ceil) {
	elf_pushnum(S,ceil(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_sqrt) {
	elf_pushnum(S,sqrt(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_pow) {
	elf_pushnum(S,pow(elf_tonum(S,args+1),elf_tonum(S,args+2)));
	return 1;
}


ELF_FUNCTION(math_lib_sin) {
	elf_pushnum(S,sin(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_cos) {
	elf_pushnum(S,cos(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_acos) {
	elf_pushnum(S,acos(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_tan) {
	elf_pushnum(S,tan(elf_tonum(S,args+1)));
	return 1;
}


ELF_FUNCTION(math_lib_atan2) {
	elf_pushnum(S,atan2(elf_tonum(S,args+1),elf_tonum(S,args+2)));
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