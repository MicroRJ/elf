//
// See Copyright Notice In elf.h
//


#define BCITEM(_,__,NAME) #NAME,
const char *byte2s[] = { BCDEF(BCITEM) };
#undef BCITEM


const char *tag2s[] = {
	[ELF_TNIL] = "nil",
	[ELF_TTOMB] = "tomb",
	[ELF_TNUMBER] = "num",
	[ELF_TINTEGER] = "int",
	[ELF_THANDLE] = "sysobj",
	[ELF_TUSER] = "userobj",
	[ELF_TFUNCTION] = "Function",
	[ELF_TCLOSURE] = "Closure",
	[ELF_TSTRING] = "Str",
	[ELF_TBUFFER] = "Buffer",
	[ELF_TTABLE] = "Tab",
};