//
// See Copyright Notice In elf.h
//

#include <string.h>

#include "elf.h"
#include "base.h"
#include "compiler.h"
#include "elf_core.h"

elf_ErrorCode elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_ErrorCode error = ELF_ERROR_INVALID_ARGUMENT;
	if (state && name && source.data)
	{
		Scratch scratch = scratch_begin();
		elf_StrSlice source_name = {(char *)name, strlen(name)};
		elf_Prototype *prototype = compiler_compile(state, scratch.arena, source_name, source);
		if (prototype)
		{
			ASSERT(prototype->captures == 0);
			elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
			closure->prototype = prototype;
			push_value(state, value_from_closure(closure));
			error = ELF_ERROR_NONE;
		}
		else
		{
			error = ELF_ERROR_COMPILATION_FAILED;
		}
		scratch_end(scratch);
	}
	return error;
}
