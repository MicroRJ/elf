#include <stdio.h>

#include "elf.h"

#include "base.h"
#include "system.h"
#include "core.h"
#include "helpers.h"

static volatile elf_Integer bench_sink;

static i64 bench_now(void)
{
	return sys_get_performance_counter();
}

static double bench_elapsed_s(i64 start)
{
	i64 elapsed = sys_get_performance_counter() - start;
	return elapsed / (double) sys_get_performance_counter_frequency();
}

static elf_Value bench_atom_key(elf_State *state, const char *text)
{
	elf_Value value = {};
	value = value_from_atom(elf_atom_from_data(state, text));
	return value;
}

static elf_Value bench_int_key(elf_Integer integer)
{
	elf_Value value = {};
	value = value_from_integer(integer);
	return value;
}

static void bench_report(const char *name, u32 iterations, double seconds)
{
	double ns_per_lookup = (seconds * 1000000000.0) / iterations;
	double lookups_per_s = iterations / seconds;

	printf("%-24s %10u lookups  %9.3f ms  %8.2f ns/lookup  %10.0f lookups/s\n",
		name, iterations, seconds * 1000.0, ns_per_lookup, lookups_per_s);
}

static double bench_table_get(elf_State *state, elf_Table *table, elf_Value *keys, u32 key_count, u32 iterations)
{
	i64 start = bench_now();

	for (u32 i = 0; i < iterations; ++i) {
		elf_Value value = elf_table_get_or_nil(state, table, keys[i & (key_count - 1)]);
		bench_sink += value.x_int;
	}

	return bench_elapsed_s(start);
}

static void fill_atom_table(elf_State *state, elf_Table *table, elf_Value *hit_keys, elf_Value *miss_keys, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "field.%u", i);
		hit_keys[i] = bench_atom_key(state, name);
		elf_table_set(state, table, hit_keys[i], bench_int_key(i));

		snprintf(name, sizeof(name), "missing.%u", i);
		miss_keys[i] = bench_atom_key(state, name);
	}
}

static void fill_int_table(elf_State *state, elf_Table *table, elf_Value *hit_keys, elf_Value *miss_keys, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		hit_keys[i] = bench_int_key(i);
		miss_keys[i] = bench_int_key(i + count);
		elf_table_set(state, table, hit_keys[i], bench_int_key(i));
	}
}

int main(void)
{
	enum {
		KEY_COUNT = 4096,
		ITERATIONS = 5000000,
	};

	prof_begin_frame();

	elf_State *state = elf_create_state();

	elf_Value *atom_hit_keys = calloc(KEY_COUNT, sizeof(*atom_hit_keys));
	elf_Value *atom_miss_keys = calloc(KEY_COUNT, sizeof(*atom_miss_keys));
	elf_Value *int_hit_keys = calloc(KEY_COUNT, sizeof(*int_hit_keys));
	elf_Value *int_miss_keys = calloc(KEY_COUNT, sizeof(*int_miss_keys));

	elf_Table *atom_table = elf_push_new_table(state);
	elf_Table *int_table = elf_push_new_table(state);

	fill_atom_table(state, atom_table, atom_hit_keys, atom_miss_keys, KEY_COUNT);
	fill_int_table(state, int_table, int_hit_keys, int_miss_keys, KEY_COUNT);

	printf("table lookup benchmarks (%u keys, %u iterations each)\n", KEY_COUNT, ITERATIONS);

	double atom_hit_seconds = 0;
	PROF_BLOCK("bench.table.atom_hit")
	{
		atom_hit_seconds = bench_table_get(state, atom_table, atom_hit_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("atom hit", ITERATIONS, atom_hit_seconds);

	double atom_miss_seconds = 0;
	PROF_BLOCK("bench.table.atom_miss")
	{
		atom_miss_seconds = bench_table_get(state, atom_table, atom_miss_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("atom miss", ITERATIONS, atom_miss_seconds);

	double integer_hit_seconds = 0;
	PROF_BLOCK("bench.table.integer_hit")
	{
		integer_hit_seconds = bench_table_get(state, int_table, int_hit_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("integer hit", ITERATIONS, integer_hit_seconds);

	double integer_miss_seconds = 0;
	PROF_BLOCK("bench.table.integer_miss")
	{
		integer_miss_seconds = bench_table_get(state, int_table, int_miss_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("integer miss", ITERATIONS, integer_miss_seconds);

	printf("sink: %lld\n", bench_sink);
	prof_dump();
	return 0;
}
