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
#include "ir.h"
#include "ir_emitter.h"
#include "bytecode_debug.h"
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

#include "string_tests.c"
#include "table_tests.c"
#include "array_tests.c"
#include "gc_tests.c"
#include "globals_tests.c"
#include "api_tests.c"
#include "arena_tests.c"
#include "lexer_tests.c"
#include "parser_tests.c"
#include "json_tests.c"
#include "unparse_tests.c"
#if 0
#include "ir_tests.c"
#endif
#include "backend_tests.c"
#include "vm_tests.c"
#include "smoke_tests.c"

int main(void)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);

	prof_begin_capture();

	elf_State *state = elf_create_state();
	if (strcmp(elf_version(), ELF_VERSION) != 0) {
		test_fail("public elf version matches the linked runtime");
	}

	run_string_tests(state);
	run_table_tests(state);
	run_array_tests();
	run_gc_tests();
	run_globals_tests(state);
	run_api_tests();
	run_arena_tests();
	run_lexer_tests(state);
	run_parser_tests(state);
	run_json_tests(state);
	run_unparse_tests(state);
	run_backend_tests();
	run_vm_tests();
	run_smoke_tests();

	prof_dump();
	elf_destroy_state(state);

	if (test_failures) {
		fprintf(stderr, "tests failed: %d\n", test_failures);
		return 1;
	}

	log_line(LOG_LEVEL_SUCCESS, "tests ok");
	return 0;
}
