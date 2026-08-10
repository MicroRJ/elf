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
#include "platform.h"

#include "logging.c"

#include "core.h"
#include "helpers.h"
#include "source_diagnostics.c"
#include "core_diagnostics.c"

#include "atom/atom.c"
#include "table/table.c"

#include "gc.c"
#include "value_text.c"

#include "compiler.h"

static void table_set_atom_table(elf_State *state, elf_Table *parent, const char *name, elf_Table *table)
{
	elf_Value key = value_from_atom(elf_atom_from_data(state, name));
	elf_Value value = value_from_table(table);
	elf_table_set(state, parent, key, value);
}

static elf_Table *new_binding_table(elf_State *state, const elf_Binding *bindings, u32 count)
{
	elf_Table *table = push_new_table(state);
	for (u32 i = 0; i < count; ++i)
	{
		elf_Value key = value_from_atom(elf_atom_from_data(state, bindings[i].name));
		elf_Value value = value_from_function(bindings[i].function);
		elf_table_set(state, table, key, value);
	}
	return table;
}

#include "libs/l_native.c"
#include "libs/l_math.c"
#include "libs/l_core.c"
#include "libs/l_debug.c"
#include "libs/l_table.c"
#include "libs/l_string.c"

static void init_runtime_storage(elf_State *state)
{
	state->frame_stack_size = 4096;
	state->frame_stack = elf_arena_push_zero(&state->arena, sizeof(*state->frame_stack) * state->frame_stack_size);

	state->stack_size = 4096;
	state->stack = elf_arena_push_zero(&state->arena, sizeof(*state->stack) * state->stack_size);
	state->stack_ptr = state->stack;
}

static void init_gc_storage(elf_State *state)
{
	state->gc_next_cycle_bytes = ELF_GC_NEXT_MIN;
	state->gc_reference_capacity = 4096;
	state->gc_references = calloc(state->gc_reference_capacity, sizeof(*state->gc_references));
	state->gc_scratch_references = calloc(state->gc_reference_capacity, sizeof(*state->gc_scratch_references));
}

static void init_state_storage(elf_State *state)
{
	init_runtime_storage(state);
	init_gc_storage(state);
}

static void bootstrap_base_frame(elf_State *state)
{
	push_value(state, value_nil());

	state->frame.framebase = state->stack;
	state->frame.framesize = 16;
}

static void reserve_bootstrap_frame_stack_space(elf_State *state)
{
	ASSERT((state->stack_ptr - state->stack) < state->frame.framesize);

	elf_Value *stack_pointer = state->frame.framebase + state->frame.framesize;
	ASSERT(stack_pointer >= state->stack_ptr);
	value_zero_many(state->stack_ptr, stack_pointer - state->stack_ptr);
	state->stack_ptr = stack_pointer;
}

static void bootstrap_standard_libraries(elf_State *state)
{
	elf_Value *stack_checkpoint = state->stack_ptr;

	state->metatables.atom = elf_lib_string(state);
	state->metatables.integer = push_new_table(state);
	state->metatables.number = push_new_table(state);
	state->metatables.table = elf_lib_table(state);

	elf_Table *elf_table = elf_lib_core(state);
	table_set_atom_table(state, elf_table, "math", elf_lib_math(state));
	table_set_atom_table(state, elf_table, "debug", elf_lib_debug(state));

	state->globals = push_new_table(state);
	table_set_atom_table(state, state->globals, "elf", elf_table);

	// The library constructors push each table while building the graph. Only
	// the globals table needs to remain on the stack as its GC root; every
	// standard library is reachable through globals, while metatables have
	// dedicated roots in the collector.
	state->stack_ptr = stack_checkpoint;
	push_table(state, state->globals);
}

void init_atoms(elf_State *state)
{
	state->atom_bucket_count = ELF_ATOM_INITIAL_EXTENT;
	state->atom_buckets = calloc(state->atom_bucket_count, sizeof(*state->atom_buckets));
}

static void bootstrap_state(elf_State *state)
{
	init_atoms(state);
	elf_init_compiler_atoms(state);
	bootstrap_base_frame(state);
	bootstrap_standard_libraries(state);
	state->ref_table = elf_new_table_rogue(state);
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

	return state;
}

void elf_destroy_state(elf_State *state)
{
	if (state)
	{
		elf_arena_destroy(&state->arena);
	}
}

static int run_bytecode_frame(elf_State *state, StackFrame frame);
#include "call.c"
#include "vm.c"
