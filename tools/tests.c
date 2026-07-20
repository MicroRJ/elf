#include <stdio.h>
#include <string.h>

#include "elf.h"

#include "base.h"
#include "platform.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "ir.h"
#include "lower.h"
#include "bytecode_debug.h"
#include "table.h"
#include "value_text.h"
#include "rank.h"
#include "logging.c"
#include "rank.c"
#include "lexer.c"
#include "ast.c"
#include "parse.c"
#include "parse_json.c"
#include "ir.c"
#include "lower.c"
#include "bytecode_gen.h"
#include "bytecode_gen.c"

static int test_failures;

static void test_fail(const char *label)
{
	log_linef(LOG_LEVEL_ERROR, "FAIL: %s", label);
	test_failures += 1;
}

#include "atom_tests.c"
#include "table_tests.c"
#include "gc_tests.c"
#include "stack_api_tests.c"
#include "arena_tests.c"
#include "path_tests.c"
#include "matcher_tests.c"
#include "rank_tests.c"
#include "lexer_tests.c"
#include "parser_tests.c"
#include "json_tests.c"
#include "unparse_tests.c"
#include "ir_tests.c"
#include "backend_tests.c"
#include "vm_tests.c"
#include "smoke_tests.c"

int main(void)
{
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);

	prof_begin_frame();

	elf_State *state = elf_create_state();
	if (strcmp(elf_version(), ELF_VERSION) != 0) {
		test_fail("public Elf version matches the linked runtime");
	}

	run_atom_tests(state);
	run_table_tests(state);
	run_gc_tests();
	run_stack_api_tests();
	run_arena_tests();
	run_path_tests();
	run_matcher_tests();
	test_rank_values();
	run_lexer_tests(state);
	run_parser_tests(state);
	run_json_tests(state);
	run_unparse_tests(state);
	run_ir_tests(state);
	run_backend_tests();
	run_vm_tests();
	run_smoke_tests();

	prof_dump();

	if (test_failures) {
		fprintf(stderr, "tests failed: %d\n", test_failures);
		return 1;
	}

	log_line(LOG_LEVEL_SUCCESS, "tests ok");
	return 0;
}
