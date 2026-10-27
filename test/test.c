#include <stdio.h>
#include <string.h>

#include "test.h"
#include "runtime/runtime_tests.c"
#include "compiler/compiler_tests.c"

int main(void)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	// Start tests on a new line because Manny also prints status.
	fprintf(stderr, "\n");

	run_runtime_tests();
	run_compiler_tests();
	return test_finish("tests");
}
