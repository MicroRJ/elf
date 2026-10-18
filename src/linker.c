//
// See Copyright Notice In elf.h
//

#include <dy.h>
#include <string.h>

#include "elf.h"
#include "base.h"
#include "bytecode.h"
#include "compiler.h"
#include "elf_core.h"
#include "linker.h"

// TODO(RJ): better error reporting here!
elf_Program *elf_link_module(elf_State *state, const BcModule *compiled_module)
{
	ASSERT(state);
	if (!compiled_module || !compiled_module->function_count) {
		return 0;
	}

	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	elf_Program *module = arena_push_zero(&state->arena, sizeof(*module), 8);
	module->string_count = compiled_module->string_count;
	if (module->string_count)
	{
		module->strings = arena_push_zero(&state->arena, sizeof(*module->strings) * module->string_count, 8);
		for (u32 i = 0; i < module->string_count; ++i)
		{
			BcString string = compiled_module->strings[i];
			module->strings[i] = elf_string_from_data_size(state, string.data, string.size);
		}
	}

	module->integer_constant_count = compiled_module->integer_constant_count;
	if (module->integer_constant_count)
	{
		module->integer_constants = arena_push_copy(&state->arena, sizeof(*module->integer_constants) * module->integer_constant_count, 8, compiled_module->integer_constants);
	}
	module->number_constant_count = compiled_module->number_constant_count;
	if (module->number_constant_count)
	{
		module->number_constants = arena_push_copy(&state->arena, sizeof(*module->number_constants) * module->number_constant_count, 8, compiled_module->number_constants);
	}
	module->bytecode_count = compiled_module->bytecode_count;
	if (module->bytecode_count)
	{
		module->bytecode = arena_push_copy(&state->arena, sizeof(*module->bytecode) * module->bytecode_count, 8, compiled_module->bytecode);
	}
	module->source_map_count = compiled_module->source_map_count;
	if (module->source_map_count)
	{
		module->source_map = arena_push_copy(&state->arena, sizeof(*module->source_map) * module->source_map_count, 8, compiled_module->source_map);
	}
	module->prototype_count = compiled_module->function_count;
	if (module->prototype_count)
	{
		module->prototypes = arena_push_copy(&state->arena, sizeof(*module->prototypes) * module->prototype_count, 8, compiled_module->functions);
	}
	module->next = state->programs;
	state->programs = module;
	state->gc_mode = saved_gc_mode;
	return module;
}

static elf_PrototypeRef compile_source(elf_State *state, const char *name, elf_StrSlice source)
{
	Scratch scratch = scratch_begin();
	elf_StrSlice source_name = {(char *)name, strlen(name)};
	BcModule *compiled_module = compiler_compile(state, scratch.arena, source_name, source);
	elf_Program *module = elf_link_module(state, compiled_module);
	elf_PrototypeRef entry = {};
	if (module)
	{
		entry.program = module;
		entry.index = 0;
	}
	scratch_end(scratch);
	return entry;
}

elf_ErrorCode elf_push_code_source(elf_State *state, const char *name, elf_StrSlice source)
{
	if (!name || !source.data) {
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	elf_PrototypeRef function = compile_source(state, name, source);
	if (!prototype_ref_is_valid(function)) {
		return ELF_ERROR_COMPILATION_FAILED;
	}
	ASSERT(prototype_from_ref(function)->captures == 0);

	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, sizeof(*closure));
	closure->function = function;
	push_value(state, value_from_closure(closure));
	return ELF_ERROR_NONE;
}
