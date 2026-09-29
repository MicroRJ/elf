#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include <dayan.h>

#include "base.h"
#include "core.h"
#include "helpers.h"

static volatile elf_Int bench_integer_sink;
static volatile elf_Num bench_number_sink;

static void bench_require(b32 condition, const char *message)
{
	if (condition) return;
	fprintf(stderr, "benchmark error: %s\n", message);
	exit(EXIT_FAILURE);
}

static i64 bench_now(void)
{
	return (i64)day_counter();
}

static double bench_elapsed_s(i64 start)
{
	i64 elapsed = (i64)day_counter() - start;
	return elapsed / (double)day_counter_frequency();
}

static elf_Value bench_string_key(elf_State *state, const char *text)
{
	elf_Value value = {};
	value = value_from_string(elf_string_from_data(state, text));
	return value;
}

static elf_Value bench_int_key(elf_Int integer)
{
	elf_Value value = {};
	value = value_from_integer(integer);
	return value;
}

static void bench_report(const char *name, const char *unit, u32 iterations, double seconds)
{
	double ns_per_iteration = (seconds * 1000000000.0) / iterations;
	double iterations_per_s = iterations / seconds;

	printf("%-24s %10u %-10s %9.3f ms  %8.2f ns/%-7s %10.0f/s\n",
		name, iterations, unit, seconds * 1000.0, ns_per_iteration, unit, iterations_per_s);
}

static double bench_table_get(elf_State *state, elf_Table *table, elf_Value *keys, u32 key_count, u32 iterations)
{
	elf_Int sink = 0;
	i64 start = bench_now();

	for (u32 i = 0; i < iterations; ++i) {
		elf_Value value = elf_table_get_or_nil(state, table, keys[i & (key_count - 1)]);
		sink += value.x_int;
	}

	double seconds = bench_elapsed_s(start);
	bench_integer_sink += sink;
	return seconds;
}

static int bench_compare_seconds(const void *left, const void *right)
{
	double x = *(const double *)left;
	double y = *(const double *)right;
	return (x > y) - (x < y);
}

static double bench_median(double *samples, u32 count)
{
	qsort(samples, count, sizeof(*samples), bench_compare_seconds);
	return samples[count / 2];
}

static elf_Ref bench_compile_program(elf_State *state, const char *name, const char *source)
{
	elf_StrSlice text = {(char *)source, (elf_u64)strlen(source)};
	bench_require(elf_push_code_source(state, name, text, 0) == ELF_ERROR_NONE, "could not compile benchmark source");
	elf_push_nil(state);
	bench_require(elf_call(state, 1, 1) == 1, "benchmark file did not return one value");
	bench_require(elf_is_callable(state, -1), "benchmark file did not return a function");
	elf_Ref program = elf_create_ref(state, -1);
	bench_require(program != ELF_NO_REF, "could not retain benchmark function");
	elf_pop(state, 1);
	return program;
}

static double bench_run_integer_program(elf_State *state, elf_Ref program, u32 iterations, elf_Int expected)
{
	bench_require(elf_push_ref(state, program), "benchmark function reference expired");
	elf_push_nil(state);
	elf_push_int(state, iterations);
	i64 start = bench_now();
	u32 result_count = elf_call(state, 2, 1);
	double seconds = bench_elapsed_s(start);
	bench_require(result_count == 1, "benchmark function did not return one value");
	elf_Int result = 0;
	bench_require(elf_to_int(state, -1, &result), "benchmark result was not an integer");
	bench_require(result == expected, "benchmark returned the wrong integer result");
	bench_integer_sink += result;
	elf_pop(state, 1);
	return seconds;
}

static double bench_run_number_program(elf_State *state, elf_Ref program, u32 iterations, elf_Num expected)
{
	bench_require(elf_push_ref(state, program), "benchmark function reference expired");
	elf_push_nil(state);
	elf_push_int(state, iterations);
	i64 start = bench_now();
	u32 result_count = elf_call(state, 2, 1);
	double seconds = bench_elapsed_s(start);
	bench_require(result_count == 1, "benchmark function did not return one value");
	elf_Num result = 0;
	bench_require(elf_type(state, -1) == ELF_VALUE_TYPE_NUMBER, "benchmark result did not use the number path");
	bench_require(elf_to_num(state, -1, &result), "benchmark result was not a number");
	bench_require(result == expected, "benchmark returned the wrong number result");
	bench_number_sink += result;
	elf_pop(state, 1);
	return seconds;
}

static void bench_vm_integer_program(const char *name, const char *source, u32 iterations, elf_Int expected)
{
	enum { SAMPLE_COUNT = 7 };
	double samples[SAMPLE_COUNT];
	elf_State *state = elf_create_state();
	elf_Ref program = bench_compile_program(state, name, source);
	bench_run_integer_program(state, program, iterations, expected);

	for (u32 i = 0; i < SAMPLE_COUNT; ++i) {
		samples[i] = bench_run_integer_program(state, program, iterations, expected);
	}

	bench_report(name, "iterations", iterations, bench_median(samples, SAMPLE_COUNT));
	elf_release_ref(state, program);
	elf_destroy_state(state);
}

static void bench_vm_number_program(const char *name, const char *source, u32 iterations, elf_Num expected)
{
	enum { SAMPLE_COUNT = 7 };
	double samples[SAMPLE_COUNT];
	elf_State *state = elf_create_state();
	elf_Ref program = bench_compile_program(state, name, source);
	bench_run_number_program(state, program, iterations, expected);

	for (u32 i = 0; i < SAMPLE_COUNT; ++i) {
		samples[i] = bench_run_number_program(state, program, iterations, expected);
	}

	bench_report(name, "iterations", iterations, bench_median(samples, SAMPLE_COUNT));
	elf_release_ref(state, program);
	elf_destroy_state(state);
}

static void bench_compile_repeatedly(const char *source, u32 iterations)
{
	elf_State *state = elf_create_state();
	bench_require(state->modules == 0, "new state preallocates compiled modules");
	u64 arena_before = state->arena.in_use;
	u64 bytecode_count = 0;
	u64 function_count = 0;
	u64 integer_constant_count = 0;
	u64 number_constant_count = 0;
	elf_StrSlice text = {(char *)source, (elf_u64)strlen(source)};

	i64 start = bench_now();
	for (u32 i = 0; i < iterations; ++i)
	{
		elf_Module *previous_module = state->modules;
		bench_require(elf_push_code_source(state, "benchmark.compile", text, 0) == ELF_ERROR_NONE, "could not compile benchmark source");
		elf_Module *module = state->modules;
		bench_require(module && module != previous_module, "compile did not publish a distinct module");
		bench_require(value_as_closure(state->stack_ptr[-1])->function.index == 0, "module entry is not local");
		bytecode_count += module->bytecode_count;
		function_count += module->bytecode_function_count;
		integer_constant_count += module->integer_constant_count;
		number_constant_count += module->number_constant_count;
		elf_pop(state, 1);
	}
	double seconds = bench_elapsed_s(start);
	u64 retained_arena_storage = state->arena.in_use - arena_before;
	u64 array_storage =
		bytecode_count * sizeof(Bytecode) +
		function_count * sizeof(BcFunction) +
		integer_constant_count * sizeof(i64) +
		number_constant_count * sizeof(f64);
	u64 non_array_arena_storage = retained_arena_storage - array_storage;

	bench_report("compile source", "compiles", iterations, seconds);
	printf("  growth: %llu bytecodes, %llu functions, %llu integer constants, %llu number constants\n",
		bytecode_count, function_count, integer_constant_count, number_constant_count);
	printf("  retained state-arena storage: %llu bytes (%llu bytes outside finalized arrays)\n",
		retained_arena_storage, non_array_arena_storage);
	printf("  preallocated compiled-program arrays per state: 0 bytes\n");
	elf_destroy_state(state);
}

static void fill_string_table(elf_State *state, elf_Table *table, elf_Value *hit_keys, elf_Value *miss_keys, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "field.%u", i);
		hit_keys[i] = bench_string_key(state, name);
		elf_table_set(state, table, hit_keys[i], bench_int_key(i));

		snprintf(name, sizeof(name), "missing.%u", i);
		miss_keys[i] = bench_string_key(state, name);
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
		VM_ITERATIONS = 1000000,
		CLOSURE_ITERATIONS = 100000,
		COMPILE_ITERATIONS = 500,
	};
	static const char dispatch_source[] =
		"ret fun(n) {\n"
		"\tacc := 0\n"
		"\tfor i := 0; i < n; i += 1 ? {\n"
		"\t\tacc += i\n"
		"\t\tacc -= i\n"
		"\t\tacc += i\n"
		"\t\tacc -= i\n"
		"\t\tacc += 1\n"
		"\t}\n"
		"\tret acc\n"
		"}\n";
	static const char call_source[] =
		"ret fun(n) {\n"
		"\tstep := fun(value) { ret value + 1 }\n"
		"\tvalue := 0\n"
		"\tfor i := 0; i < n; i += 1 ? {\n"
		"\t\tvalue = step(value)\n"
		"\t}\n"
		"\tret value\n"
		"}\n";
	static const char integer_constant_source[] =
		"ret fun(n) {\n"
		"\tacc := 0\n"
		"\tfor i := 0; i < n; i += 1 ? {\n"
		"\t\tacc += i + 3\n"
		"\t\tacc += i + 5\n"
		"\t\tacc += i + 7\n"
		"\t\tacc += i + 11\n"
		"\t\tacc += i + 13\n"
		"\t\tacc += i + 17\n"
		"\t\tacc += i + 19\n"
		"\t\tacc += i + 23\n"
		"\t}\n"
		"\tret acc\n"
		"}\n";
	static const char number_constant_source[] =
		"ret fun(n) {\n"
		"\tvalue := 0.0\n"
		"\tfor i := 0; i < n; i += 1 ? {\n"
		"\t\tvalue += 1.25\n"
		"\t\tvalue += 2.5\n"
		"\t\tvalue -= 0.75\n"
		"\t}\n"
		"\tret value\n"
		"}\n";
	static const char closure_source[] =
		"ret fun(n) {\n"
		"\tresult := 0\n"
		"\tfor i := 0; i < n; i += 1 ? {\n"
		"\t\tcurrent := fun() { ret i }\n"
		"\t\tresult += current() - i + 1\n"
		"\t}\n"
		"\tret result\n"
		"}\n";

	prof_begin_capture();

	elf_State *state = elf_create_state();

	elf_Value *string_hit_keys = calloc(KEY_COUNT, sizeof(*string_hit_keys));
	elf_Value *string_miss_keys = calloc(KEY_COUNT, sizeof(*string_miss_keys));
	elf_Value *int_hit_keys = calloc(KEY_COUNT, sizeof(*int_hit_keys));
	elf_Value *int_miss_keys = calloc(KEY_COUNT, sizeof(*int_miss_keys));

	elf_Table *string_table = push_new_table(state);
	elf_Table *int_table = push_new_table(state);

	fill_string_table(state, string_table, string_hit_keys, string_miss_keys, KEY_COUNT);
	fill_int_table(state, int_table, int_hit_keys, int_miss_keys, KEY_COUNT);

	printf("table lookup benchmarks (%u keys, %u iterations each)\n", KEY_COUNT, ITERATIONS);

	double string_hit_seconds = 0;
	PROF_BLOCK("bench.table.string_hit")
	{
		string_hit_seconds = bench_table_get(state, string_table, string_hit_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("string hit", "lookups", ITERATIONS, string_hit_seconds);

	double string_miss_seconds = 0;
	PROF_BLOCK("bench.table.string_miss")
	{
		string_miss_seconds = bench_table_get(state, string_table, string_miss_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("string miss", "lookups", ITERATIONS, string_miss_seconds);

	double integer_hit_seconds = 0;
	PROF_BLOCK("bench.table.integer_hit")
	{
		integer_hit_seconds = bench_table_get(state, int_table, int_hit_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("integer hit", "lookups", ITERATIONS, integer_hit_seconds);

	double integer_miss_seconds = 0;
	PROF_BLOCK("bench.table.integer_miss")
	{
		integer_miss_seconds = bench_table_get(state, int_table, int_miss_keys, KEY_COUNT, ITERATIONS);
	}
	bench_report("integer miss", "lookups", ITERATIONS, integer_miss_seconds);

	printf("\nVM benchmarks (median of 7 measured runs after one warmup)\n");
	bench_vm_integer_program("VM dispatch", dispatch_source, VM_ITERATIONS, VM_ITERATIONS);
	bench_vm_integer_program("function calls", call_source, VM_ITERATIONS, VM_ITERATIONS);
	bench_vm_integer_program("integer constants", integer_constant_source, VM_ITERATIONS,
		4ll * VM_ITERATIONS * (VM_ITERATIONS - 1ll) + 98ll * VM_ITERATIONS);
	bench_vm_number_program("number constants", number_constant_source, VM_ITERATIONS, 3.0 * VM_ITERATIONS);
	bench_vm_integer_program("closure create/call", closure_source, CLOSURE_ITERATIONS, CLOSURE_ITERATIONS);

	printf("\nCompiler and storage benchmark\n");
	bench_compile_repeatedly(closure_source, COMPILE_ITERATIONS);

	printf("sinks: %lld %.3f\n", bench_integer_sink, bench_number_sink);
	prof_dump();
	elf_destroy_state(state);
	return 0;
}
