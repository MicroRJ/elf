//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#if defined(__EMSCRIPTEN__)
   #include <emscripten.h>
   #include <unistd.h>
#endif

#include "elf.h"
#include "base.h"
#include "libs.h"

#include "logging.c"

#include "elf_core.h"
#include "source_diagnostics.c"
#include "core_diagnostics.c"

#include "elf_memory.c"
#include "elf_globals.c"
#include "elf_string.c"
#include "elf_buffer.c"
#include "elf_user_object.c"
#include "elf_array.c"
#include "elf_table.c"

#include "elf_gc.c"
#include "elf_value_text.c"

#include "compiler.h"

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

#include "../libs/l_math.c"
#include "../libs/l_number.c"
#include "../libs/l_core.c"
#include "../libs/l_gc.c"
#include "../libs/l_vm.c"
#include "../libs/l_table.c"
#include "../libs/l_array.c"
#include "../libs/l_string.c"
#include "../libs/l_buffer.c"

static void init_runtime_storage(elf_State *state)
{
	state->frame_stack_size = 4096;
	state->frame_stack = elf_arena_push_zero(&state->arena, sizeof(*state->frame_stack) * state->frame_stack_size);
	state->frame = state->frame_stack;

	state->stack_size = 4096;
	state->stack = elf_arena_push_zero(&state->arena, sizeof(*state->stack) * state->stack_size);
	state->stack_ptr = state->stack;
}

static void init_gc_storage(elf_State *state)
{
	state->gc_next_cycle_bytes = ELF_GC_NEXT_MIN;
	state->gc_reference_capacity = 4096;
	u64 reference_bytes = (u64)state->gc_reference_capacity * sizeof(*state->gc_references);
	state->gc_references = elf_alloc_zero(state, reference_bytes);
	state->gc_scratch_references = elf_alloc_zero(state, reference_bytes);
	ASSERT(state->gc_references != 0);
	ASSERT(state->gc_scratch_references != 0);
}

static void init_state_storage(elf_State *state)
{
	init_runtime_storage(state);
	init_gc_storage(state);
	elf_globals_init(&state->globals, state);
}

static void bootstrap_base_frame(elf_State *state)
{
	push_value(state, value_nil());

	state->frame->framebase = state->stack;
	state->frame->framesize = 16;
}

static void reserve_bootstrap_frame_stack_space(elf_State *state)
{
	ASSERT((state->stack_ptr - state->stack) < state->frame->framesize);

	elf_Value *stack_pointer = state->frame->framebase + state->frame->framesize;
	ASSERT(stack_pointer >= state->stack_ptr);
	value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
	state->stack_ptr = stack_pointer;
	vm_record_stack_usage(state);
}

static void bootstrap_standard_libraries(elf_State *state)
{
	elf_Value *stack_checkpoint = state->stack_ptr;

	state->metatables.string = elf_lib_string(state);
	state->metatables.integer = push_new_table(state);
	state->metatables.number = elf_lib_number(state);
	state->metatables.table = elf_lib_table(state);
	state->metatables.array = elf_lib_array(state);
	state->metatables.buffer = elf_lib_buffer(state);

	elf_Table *elf_table = elf_lib_core(state);
	elf_table_set(state, elf_table, value_from_string(elf_string_from_data(state, "buffer")), value_from_function(l_buffer_new));
	elf_table_set(state, elf_table, value_from_string(elf_string_from_data(state, "math")), value_from_table(elf_lib_math(state)));
	elf_table_set(state, elf_table, value_from_string(elf_string_from_data(state, "gc")), value_from_table(elf_lib_gc(state)));
	elf_table_set(state, elf_table, value_from_string(elf_string_from_data(state, "vm")), value_from_table(elf_lib_vm(state)));

	elf_StrSlice elf_name = {(char *)"elf", 3};
	elf_globals_set_by_name(&state->globals, elf_name, value_from_table(elf_table));

	// The library constructors push each table while building the graph. The
	// global store and metatables have dedicated roots in the collector.
	state->stack_ptr = stack_checkpoint;
}

void init_strings(elf_State *state)
{
	state->string_bucket_count = ELF_STRING_INITIAL_EXTENT;
	state->string_buckets = elf_alloc_zero(state,
		(u64)state->string_bucket_count * sizeof(*state->string_buckets));
	ASSERT(state->string_buckets != 0);
}

static void bootstrap_state(elf_State *state)
{
	init_strings(state);
	bootstrap_base_frame(state);
	bootstrap_standard_libraries(state);
	state->ref_table = elf_table_new(state);
	state->next_ref = 1;
	reserve_bootstrap_frame_stack_space(state);
}

elf_State *elf_create_state()
{
	elf_Arena arena = elf_arena_create(0);

	elf_State *state = elf_arena_push_zero(&arena, sizeof(*state));
	state->arena = arena;

	init_state_storage(state);
	bootstrap_state(state);
	elf_open_libraries(state);

	return state;
}

void elf_destroy_state(elf_State *state)
{
	if (state)
	{
		elf_globals_destroy(&state->globals);
		gc_destroy_all(state);
		elf_free(state, state->string_buckets,
			(u64)state->string_bucket_count * sizeof(*state->string_buckets));
		elf_Arena arena = state->arena;
		elf_arena_destroy(&arena);
	}
}

#include "elf_vm.c"
#include "elf_api.c"
