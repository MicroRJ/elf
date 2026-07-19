//
// See Copyright Notice In elf.h
//

#include "elf.h"
#include "base.h"
#include "system.h"
#include "core.h"

const char *value_type_name(elf_ValueType type)
{
	static const char *names[] =
	{
		[ELF_VALUE_TYPE_NIL]         = "nil",
		[ELF_VALUE_TYPE_NUMBER]      = "number",
		[ELF_VALUE_TYPE_INTEGER]     = "integer",
		[ELF_VALUE_TYPE_USER_OBJECT] = "resource",
		[ELF_VALUE_TYPE_CFUNCTION]   = "function",
		[ELF_VALUE_TYPE_CLOSURE]     = "closure",
		[ELF_VALUE_TYPE_ATOM]        = "string",
		[ELF_VALUE_TYPE_TABLE]       = "table",
	};

	if ((u32)type >= ELF_VALUE_TYPE_COUNT_ || !names[type]) {
		return "unknown";
	}
	return names[type];
}
