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

#include "libs/l_math.c"
#include "libs/l_number.c"
#include "libs/l_core.c"
#include "libs/l_gc.c"
#include "libs/l_vm.c"
#include "libs/l_table.c"
#include "libs/l_array.c"
#include "libs/l_string.c"
#include "libs/l_buffer.c"

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
