//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "bytecode.h"

const char *bytecode_type_name(BytecodeType type)
{
	static const char *names[] =
	{
#define XPAND(ENUM, NAME) NAME,
		BC_XDEF(XPAND)
#undef XPAND
	};

	if ((u32)type >= BC_COUNT_) {
		return "unknown";
	}
	return names[type];
}
