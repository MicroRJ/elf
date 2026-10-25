#include <stdio.h>
#include <string.h>

#include "test.h"
#include "runtime/runtime_tests.c"
#include "compiler/compiler_tests.c"

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	if (argc == 2 && strcmp(argv[1], "--runtime-error-diagnostic-child") == 0)
	{
		return vm_run_runtime_error_diagnostic_child();
	}

	// Start tests on a new line because Manny also prints status.
	fprintf(stderr, "\n");

	run_runtime_tests();
	run_compiler_tests();
	return test_finish("tests");
}
