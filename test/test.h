#ifndef ELF_TEST_H
#define ELF_TEST_H

#include <dy.h>
#include <dy_io.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef dy_u32 u32;
typedef dy_u64 u64;
typedef dy_i64 i64;
typedef dy_b32 b32;

typedef struct
{
	const char *current_test;
	u32 tests;
	u32 failed_tests;
	u32 checks;
	u32 failures;
	u32 test_failure_start;
}
TestContext;

static TestContext test_context;

static void test_print_status(const char *name, b32 passed)
{
	static b32 colors_enabled;
	if (!colors_enabled)
	{
		dy_enable_console_colors(DY_STANDARD_ERROR);
		colors_enabled = 1;
	}

	const char *color = passed ? "\x1b[32m" : "\x1b[31m";
	const char *status = passed ? "PASS" : "FAIL";
	fprintf(stderr, "TEST [%s] %s%s\x1b[0m\n", name, color, status);
}

static void test_fail_at(const char *file, int line, const char *function, const char *format, ...)
{
	if (test_context.current_test && test_context.failures == test_context.test_failure_start)
	{
		test_print_status(test_context.current_test, 0);
	}
	else if (!test_context.current_test)
	{
		test_print_status(function, 0);
	}

	fprintf(stderr, "  %s:%d: ", file, line);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);
	test_context.failures += 1;
}

static void test_begin(const char *name)
{
	test_context.current_test = name;
	test_context.test_failure_start = test_context.failures;
	test_context.tests += 1;
}

static void test_end(void)
{
	if (test_context.failures != test_context.test_failure_start)
	{
		test_context.failed_tests += 1;
	}
	else
	{
		test_print_status(test_context.current_test, 1);
	}

	test_context.current_test = 0;
}

static void test_expect_i64_at(const char *file, int line, const char *function, const char *expression, i64 actual, i64 expected)
{
	test_context.checks += 1;
	if (actual != expected)
	{
		test_fail_at(file, line, function, "%s expected %lld, got %lld", expression, (long long)expected, (long long)actual);
	}
}

static void test_expect_u64_at(const char *file, int line, const char *function, const char *expression, u64 actual, u64 expected)
{
	test_context.checks += 1;
	if (actual != expected)
	{
		test_fail_at(file, line, function, "%s expected %llu, got %llu", expression, (unsigned long long)expected, (unsigned long long)actual);
	}
}

static void test_expect_ptr_at(const char *file, int line, const char *function, const char *expression, const void *actual, const void *expected)
{
	test_context.checks += 1;
	if (actual != expected)
	{
		test_fail_at(file, line, function, "%s expected %p, got %p", expression, expected, actual);
	}
}

static void test_expect_string_at(const char *file, int line, const char *function, const char *expression, const char *actual, const char *expected)
{
	test_context.checks += 1;
	b32 equal = actual == expected || (actual && expected && strcmp(actual, expected) == 0);
	if (!equal)
	{
		test_fail_at(file, line, function, "%s expected \"%s\", got \"%s\"", expression, expected ? expected : "<null>", actual ? actual : "<null>");
	}
}

static int test_finish(const char *suite)
{
	if (test_context.failures)
	{
		fprintf(stderr, "%s: %u of %u test(s) failed (%u failed check(s))\n", suite, test_context.failed_tests, test_context.tests, test_context.failures);
		fprintf(stderr, "\n\x1b[31m================ TESTS FAILED ================\x1b[0m\n");
		return 1;
	}

	fprintf(stderr, "%s: %u test(s) passed\n", suite, test_context.tests);
	fprintf(stderr, "\n\x1b[32m================== TESTS OK ==================\x1b[0m\n");
	return 0;
}

#define TEST_FAIL(...) do { test_context.checks += 1; test_fail_at(__FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)
#define TEST_EXPECT(expression) do { test_context.checks += 1; if (!(expression)) { test_fail_at(__FILE__, __LINE__, __func__, "expected %s", #expression); } } while (0)
#define TEST_REQUIRE(expression) do { test_context.checks += 1; if (!(expression)) { test_fail_at(__FILE__, __LINE__, __func__, "required %s", #expression); return; } } while (0)
#define TEST_EXPECT_I64(actual, expected) test_expect_i64_at(__FILE__, __LINE__, __func__, #actual, (i64)(actual), (i64)(expected))
#define TEST_EXPECT_U64(actual, expected) test_expect_u64_at(__FILE__, __LINE__, __func__, #actual, (u64)(actual), (u64)(expected))
#define TEST_EXPECT_PTR(actual, expected) test_expect_ptr_at(__FILE__, __LINE__, __func__, #actual, (const void *)(actual), (const void *)(expected))
#define TEST_EXPECT_STRING(actual, expected) test_expect_string_at(__FILE__, __LINE__, __func__, #actual, (actual), (expected))
#define TEST_RUN(name, function, ...) do { test_begin((name)); function(__VA_ARGS__); test_end(); } while (0)

#endif
