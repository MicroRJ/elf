//
// See Copyright Notice In elf.h
//


const char *Static_StrFromBytecode[] =
{
#define XPAND(ENUM, NAME) NAME,
	BYTECODE_XDEF(XPAND)
#undef XPAND
};

const char *tag2s[] =
{
	[ELF_VALUE_TYPE_NIL]      = "nil",
	[ELF_VALUE_TYPE_TOMB]     = "tomb",
	[ELF_VALUE_TYPE_NUMBER]   = "num",
	[ELF_VALUE_TYPE_INTEGER]  = "int",
	[ELF_VALUE_TYPE_HANDLE]   = "sysobj",
	[ELF_VALUE_TYPE_USER_OBJECT]     = "userobj",
	[ELF_VALUE_TYPE_CFUNCTION] = "Function",
	[ELF_VALUE_TYPE_CLOSURE]  = "Closure",
	[ELF_VALUE_TYPE_STRING]   = "GCStr",
	[ELF_VALUE_TYPE_BUFFER]   = "Buffer",
	[ELF_VALUE_TYPE_TABLE]    = "Tab",
};