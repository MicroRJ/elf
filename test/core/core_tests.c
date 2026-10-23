#include <stdio.h>
#include <string.h>

#include "elf.h"

#include "../test.h"
#include "base.h"
#include "elf_core.h"
#include "elf_value_text.h"

#define test_fail(label) TEST_FAIL("%s", (label))
#include "string_tests.c"
#include "table_tests.c"
#include "array_tests.c"
#include "gc_tests.c"
#include "native_function_tests.c"
#include "publication_tests.c"
#include "api_tests.c"
#undef test_fail

static void run_core_tests(void)
{
	elf_State *state = elf_create_state();
	run_string_tests(state);
	run_table_tests(state);
	run_array_tests();
	run_gc_tests();
	run_native_function_tests(state);
	run_publication_tests(state);
	run_api_tests();
	elf_destroy_state(state);
}
