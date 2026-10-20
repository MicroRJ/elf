#include <stdio.h>
#include <string.h>

#include "elf.h"

#include "base.h"
#include "elf_core.h"
#include "libs.h"
#include "atom.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "checker.h"
#include "entity.h"
#include "ir.h"
#include "ir_emitter.h"
#include "bytecode.h"
#include "elf_value_text.h"

// Compiler sources included by the unity tests.
#include "logging.c"
#include "lexer.c"
#include "bc_emitter.h"

static int test_failures;

static void test_fail(const char *label)
{
	log_linef(LOG_LEVEL_ERROR, "FAIL: %s", label);
	test_failures += 1;
}

#include "lexer_tests.c"
#include "parser_tests.c"
#include "unparse_tests.c"
#include "checker_tests.c"
#include "ir_tests.c"
#include "backend_tests.c"
#include "vm_tests.c"
#include "../script_tests.c"
#include "smoke_tests.c"

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	if (argc == 2 && strcmp(argv[1], "--runtime-error-diagnostic-child") == 0)
	{
		return vm_run_runtime_error_diagnostic_child();
	}

	elf_State *state = elf_create_state();
	run_lexer_tests(state);
	run_parser_tests(state);
	run_unparse_tests(state);
	run_checker_tests();
	run_ir_tests(state);
	run_backend_tests();
	run_vm_tests();
	run_script_tests();

	elf_destroy_state(state);

	if (test_failures) {
		fprintf(stderr, "tests failed: %d\n", test_failures);
		return 1;
	}

	log_line(LOG_LEVEL_SUCCESS, "tests ok");
	return 0;
}
