#include <stdio.h>
#include <string.h>

#include "elf.h"

#include <dy_io.h>
#include <dy_process.h>

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

#define test_fail(label) TEST_FAIL("%s", (label))
#include "lexer_tests.c"
#include "parser_tests.c"
#include "unparse_tests.c"
#include "checker_tests.c"
#include "ir_tests.c"
#include "backend_tests.c"
#include "vm_tests.c"
#include "../script_tests.c"
#undef test_fail

static void run_compiler_tests(void)
{
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
}
