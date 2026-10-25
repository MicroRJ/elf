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

#include "logging.c"

#include "elf_runtime.h"
#include "source_diagnostics.c"
#include "error.c"

#include "elf_memory.c"

#include "elf_native_function.c"
#include "elf_symbols.c"
#include "elf_source.c"

#include "elf_string.c"
#include "elf_buffer.c"
#include "elf_user_object.c"
#include "elf_array.c"
#include "elf_table.c"

#include "elf_gc.c"
#include "elf_value_text.c"

elf_Closure *elf_closure_new(elf_State *state, elf_Prototype *prototype, elf_Value *captures, u32 capture_count)
{
	ASSERT(state);
	ASSERT(prototype);
	ASSERT(captures || !capture_count);
	ASSERT(capture_count == prototype->captures);
	if (!state || !prototype || (!captures && capture_count) || capture_count != prototype->captures) {
		return 0;
	}
	u32 size = sizeof(elf_Closure) + sizeof(elf_Value) * capture_count;
	elf_Closure *closure = elf_gc_alloc(state, ELF_OBJECT_CLOSURE, size);
	closure->prototype = prototype;
	if (capture_count) {
		value_copy_many(closure->captures, captures, capture_count);
	}
	return closure;
}

static void init_runtime_storage(elf_State *state)
{
	state->frame_stack_size = 4096;
	state->frame_stack = arena_push_zero(&state->arena, sizeof(*state->frame_stack) * state->frame_stack_size, _Alignof(StackFrame));
	state->frame = state->frame_stack;

	state->stack_size = 4096;
	state->stack = arena_push_zero(&state->arena, sizeof(*state->stack) * state->stack_size, _Alignof(elf_Value));
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
}

static void bootstrap_base_frame(elf_State *state)
{
	push_value(state, value_nil());

	state->frame->framebase = state->stack;
	state->frame->framesize = 16;
	state->frame->prototype = 0;
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
	state->ref_table = elf_table_new(state);
	state->next_ref = 1;
	reserve_bootstrap_frame_stack_space(state);
}

elf_State *elf_runtime_create_state(void)
{
	Arena arena = arena_create(0);

	elf_State *state = arena_push_zero(&arena, sizeof(*state), _Alignof(elf_State));
	state->arena = arena;

	init_state_storage(state);
	bootstrap_state(state);

	return state;
}

void elf_destroy_state(elf_State *state)
{
	if (state)
	{
		elf_sym_destroy(state->symbols);
		elf_native_functions_destroy(state);
		gc_destroy_all(state);
		elf_sources_destroy(state);
		for (u32 i = 0; i < state->prototype_count; ++i)
		{
			elf_Prototype *prototype = state->prototypes[i];
			if (prototype)
			{
				elf_free(state, prototype, prototype->allocation_size);
			}
		}
		elf_free(state, state->prototypes, (u64)state->prototype_capacity * sizeof(*state->prototypes));
		elf_free(state, state->string_buckets,
			(u64)state->string_bucket_count * sizeof(*state->string_buckets));
		Arena arena = state->arena;
		arena_destroy(&arena);
	}
}

#include "elf_vm.c"
#include "elf_api.c"
