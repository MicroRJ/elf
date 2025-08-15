//
// See Copyright Notice In elf.h
//



// todo: all of these should be instrinsics...

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