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
#include "lib_com.h"

#include "logging.c"

#include "elf_core.h"
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

#include "libs/l_math.c"
#include "libs/l_number.c"
#include "libs/l_core.c"
#include "libs/l_gc.c"
#include "libs/l_vm.c"
#include "libs/l_table.c"
#include "libs/l_array.c"
#include "libs/l_string.c"
#include "libs/l_buffer.c"

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

static void bootstrap_standard_libraries(elf_State *state)
{
	elf_Value *stack_checkpoint = state->stack_ptr;

	state->metatables.string = elf_lib_string(state);
	state->metatables.integer = push_new_table(state);
	state->metatables.number = elf_lib_number(state);
	state->metatables.table = elf_lib_table(state);
	state->metatables.array = elf_lib_array(state);
	state->metatables.buffer = elf_lib_buffer(state);

	// The library constructors push each table while building the graph. The
	// metatables have dedicated roots in the collector.
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

static elf_SymTable *bootstrap_symbols(elf_State *state)
{
	elf_SymTable *root = 0;
	elf_sym_create(&root);

	elf_SymTable *elf = elf_lib_core();

	elf_SymTable *vm = 0;
	elf_sym_create(&vm);

	elf_SymTable *gc = 0;
	elf_sym_create(&gc);

	elf_sym_fun(vm, ELF_LIT("counters"), l_vm_counters);
	elf_sym_fun(gc, ELF_LIT("counters"), l_gc_counters);
	elf_sym_fun(elf, ELF_LIT("buffer"), l_buffer_new);
	elf_sym_tab(elf, ELF_LIT("math"), elf_lib_math());
	elf_sym_tab(elf, ELF_LIT("vm"), vm);
	elf_sym_tab(elf, ELF_LIT("gc"), gc);
	elf_sym_tab(root, ELF_LIT("elf"), elf);
	state->symbols = root;
	return elf;
}

elf_State *elf_create_state()
{
	Arena arena = arena_create(0);

	elf_State *state = arena_push_zero(&arena, sizeof(*state), _Alignof(elf_State));
	state->arena = arena;

	init_state_storage(state);
	bootstrap_state(state);
	elf_SymTable *elf_symbols = bootstrap_symbols(state);
	elf_open_libraries(state, elf_symbols);
	elf_com_open_libs(state, elf_symbols);

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
