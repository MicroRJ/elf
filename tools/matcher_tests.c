static void expect_match_tail(const char *text, const char *pattern, i32 expected_tail, const char *label)
{
	const char *tail = matcher_match(text, pattern);
	if (!tail || tail != text + expected_tail)
	{
		fprintf(stderr, "FAIL: %s expected tail %d, got %td\n",
			label, expected_tail, tail ? tail - text : -1);
		test_failures += 1;
	}
}

static void expect_no_match(const char *text, const char *pattern, const char *label)
{
	if (matcher_match(text, pattern))
	{
		fprintf(stderr, "FAIL: %s expected no match for text '%s' pattern '%s'\n",
			label, text, pattern);
		test_failures += 1;
	}
}

static void test_matcher_exact_text(void)
{
	expect_match_tail("alpha", "alpha", 5, "matcher matches exact text");
	expect_no_match("alpha", "alp", "matcher rejects partial literal pattern");
	expect_no_match("alpha", "alphabet", "matcher rejects overlong literal pattern");
}

static void test_matcher_question_wildcard(void)
{
	expect_match_tail("cat", "c?t", 3, "matcher question wildcard matches one character");
	expect_match_tail("cab", "???", 3, "matcher question wildcards match entire text");
	expect_no_match("cat", "c??t", "matcher question wildcard does not match zero characters");
}

static void test_matcher_star_wildcard(void)
{
	expect_match_tail("alphabet", "a*", 8, "matcher trailing star consumes to end");
	expect_match_tail("alphabet", "*bet", 8, "matcher leading star consumes through suffix");
	expect_match_tail("alphabet", "a*ha*t", 8, "matcher middle stars consume flexible ranges");
	expect_match_tail("alphabet", "a**t", 8, "matcher collapses redundant stars");
	expect_no_match("alphabet", "a*z", "matcher rejects missing suffix after star");
}

static void test_matcher_alternates(void)
{
	expect_match_tail("green", "red|green|blue", 5, "matcher matches middle alternate");
	expect_match_tail("blue", "red|green|blue", 4, "matcher matches last alternate");
	expect_no_match("yellow", "red|green|blue", "matcher rejects missing alternate");
}

static void test_matcher_parentheses_are_literals(void)
{
	expect_match_tail("(green)", "(green)", 7, "matcher treats parentheses as literals");
	expect_match_tail("green)", "green)", 6, "matcher treats closing paren as literal");
	expect_no_match("green", "green)", "matcher does not treat closing paren as terminator");
}

static void test_matcher_empty_text(void)
{
	expect_match_tail("", "", 0, "matcher matches empty text and pattern");
	expect_match_tail("", "*", 0, "matcher star matches empty text");
	expect_no_match("", "?", "matcher question does not match empty text");
	expect_no_match("", "x", "matcher literal does not match empty text");
}

static void run_matcher_tests(void)
{
	test_matcher_exact_text();
	test_matcher_question_wildcard();
	test_matcher_star_wildcard();
	test_matcher_alternates();
	test_matcher_parentheses_are_literals();
	test_matcher_empty_text();
}
