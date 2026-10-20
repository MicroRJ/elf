#include <stdio.h>
#include <string.h>

#include "elf.h"

#include "base.h"
#include "elf_core.h"
#include "elf_value_text.h"

static int test_failures;

static void test_fail(const char *label)
{
	fprintf(stderr, "FAIL: %s\n", label);
	test_failures += 1;
}

#include "string_tests.c"
#include "table_tests.c"
#include "array_tests.c"
#include "gc_tests.c"
#include "globals_tests.c"
#include "api_tests.c"

int main(void)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);

	elf_State *state = elf_create_state();
	run_string_tests(state);
	run_table_tests(state);
	run_array_tests();
	run_gc_tests();
	run_globals_tests(state);
	run_api_tests();
	elf_destroy_state(state);

	if (test_failures)
	{
		fprintf(stderr, "core tests failed: %d\n", test_failures);
		return 1;
	}

	return 0;
}
