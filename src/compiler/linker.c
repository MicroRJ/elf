//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "bytecode.h"
#include "elf_core.h"

// TODO(RJ): better error reporting here!
elf_Module *elf_link_module(elf_State *state, const BcModule *compiled_module)
{
	ASSERT(state);
	if (!compiled_module || !compiled_module->source_count || compiled_module->root_source >= compiled_module->source_count || !compiled_module->function_count) {
		return 0;
	}
	for (u32 i = 0; i < compiled_module->global_relocation_count; ++i)
	{
		BcGlobalRelocation relocation = compiled_module->global_relocations[i];
		if (relocation.bytecode_index >= compiled_module->bytecode_count || relocation.name_index >= compiled_module->string_count) {
			return 0;
		}
		BcType type = BC_TYPE(compiled_module->bytecode[relocation.bytecode_index]);
		if (type != BC_GETGLOBAL && type != BC_SETGLOBAL) {
			return 0;
		}
	}

	elf_Scratch scratch = elf_begin_scratch();
	u32 *global_slots = elf_arena_push(scratch.arena, sizeof(*global_slots) * compiled_module->string_count);
	for (u32 i = 0; i < compiled_module->string_count; ++i) {
		global_slots[i] = UINT32_MAX;
	}
	u32 next_new_slot = state->globals.count;
	for (u32 i = 0; i < compiled_module->global_relocation_count; ++i)
	{
		u32 name_index = compiled_module->global_relocations[i].name_index;
		if (global_slots[name_index] == UINT32_MAX)
		{
			BcString name = compiled_module->strings[name_index];
			elf_StrSlice slice = {(char *)name.data, name.size};
			u32 slot = 0;
			if (!elf_globals_find(&state->globals, slice, &slot)) {
				slot = next_new_slot++;
			}
			if (slot > 32767u)
			{
				elf_end_scratch(scratch);
				return 0;
			}
			global_slots[name_index] = slot;
		}
	}

	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;
	for (u32 i = 0; i < compiled_module->string_count; ++i)
	{
		if (global_slots[i] != UINT32_MAX)
		{
			BcString name = compiled_module->strings[i];
			global_slots[i] = elf_globals_ensure(&state->globals, (elf_StrSlice) {(char *)name.data, name.size});
		}
	}

	elf_arena_align(&state->arena, 8);
	elf_Module *module = elf_arena_push_zero(&state->arena, sizeof(*module));
	BcSource root_source = compiled_module->sources[compiled_module->root_source];
	module->source_name = elf_string_from_data_size(state, root_source.name.data, root_source.name.size);
	u64 source_storage_size = ((u64)root_source.source.size + 1 + 7) & ~(u64)7;
	module->source_data = elf_arena_push_zero(&state->arena, source_storage_size);
	copy_memory(module->source_data, root_source.source.data, root_source.source.size);
	module->source_size = root_source.source.size;

	module->string_count = compiled_module->string_count;
	if (module->string_count)
	{
		elf_arena_align(&state->arena, 8);
		module->strings = elf_arena_push_zero(&state->arena, sizeof(*module->strings) * module->string_count);
		for (u32 i = 0; i < module->string_count; ++i)
		{
			BcString string = compiled_module->strings[i];
			module->strings[i] = elf_string_from_data_size(state, string.data, string.size);
		}
	}

	module->integer_constant_count = compiled_module->integer_constant_count;
	if (module->integer_constant_count)
	{
		elf_arena_align(&state->arena, 8);
		module->integer_constants = elf_arena_push_copy(&state->arena, sizeof(*module->integer_constants) * module->integer_constant_count, compiled_module->integer_constants);
	}
	module->number_constant_count = compiled_module->number_constant_count;
	if (module->number_constant_count)
	{
		elf_arena_align(&state->arena, 8);
		module->number_constants = elf_arena_push_copy(&state->arena, sizeof(*module->number_constants) * module->number_constant_count, compiled_module->number_constants);
	}
	module->bytecode_count = compiled_module->bytecode_count;
	if (module->bytecode_count)
	{
		elf_arena_align(&state->arena, 8);
		module->bytecode = elf_arena_push_copy(&state->arena, sizeof(*module->bytecode) * module->bytecode_count, compiled_module->bytecode);
	}
	module->source_map_count = compiled_module->source_map_count;
	if (module->source_map_count)
	{
		module->source_map = dy_arena_push_copy(&state->arena, sizeof(*module->source_map) * module->source_map_count, 8, compiled_module->source_map);
	}
	module->bytecode_function_count = compiled_module->function_count;
	if (module->bytecode_function_count)
	{
		elf_arena_align(&state->arena, 8);
		module->bytecode_functions = elf_arena_push_copy(&state->arena, sizeof(*module->bytecode_functions) * module->bytecode_function_count, compiled_module->functions);
	}
	for (u32 i = 0; i < compiled_module->global_relocation_count; ++i)
	{
		BcGlobalRelocation relocation = compiled_module->global_relocations[i];
		Bytecode *bytecode = module->bytecode + relocation.bytecode_index;
		i16 slot = (i16)global_slots[relocation.name_index];
		if (BC_TYPE(*bytecode) == BC_GETGLOBAL) {
			bytecode->b_y = slot;
		}
		else {
			bytecode->b_x = slot;
		}
	}

	module->next = state->modules;
	state->modules = module;
	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return module;
}
