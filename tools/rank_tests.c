static void expect_rank_order(RankValue *values, u32 count, const i64 *expected, const char *label)
{
	for (u32 i = 0; i < count; ++i)
	{
		if (values[i].rank != expected[i])
		{
			fprintf(stderr, "FAIL: %s rank[%u] expected %lld, got %lld\n"
			,	label, i, expected[i], values[i].rank);
			test_failures += 1;
			return;
		}
	}
}

static void test_rank_values(void)
{
	RankValue empty[1] = {};
	rank_values(empty, empty);

	RankValue single[] = {{42, value_from_integer(1)}};
	rank_values(single, single + ARRAY_COUNT(single));
	const i64 single_expected[] = {42};
	expect_rank_order(single, ARRAY_COUNT(single), single_expected, "single rank");

	RankValue reversed[] =
	{
		{5, value_from_integer(5)},
		{4, value_from_integer(4)},
		{3, value_from_integer(3)},
		{2, value_from_integer(2)},
		{1, value_from_integer(1)},
	};
	rank_values(reversed, reversed + ARRAY_COUNT(reversed));
	const i64 reversed_expected[] = {1, 2, 3, 4, 5};
	expect_rank_order(reversed, ARRAY_COUNT(reversed), reversed_expected, "reversed ranks");

	RankValue mixed[] =
	{
		{0, value_from_integer(10)},
		{-8, value_from_integer(20)},
		{3, value_from_integer(30)},
		{3, value_from_integer(40)},
		{-8, value_from_integer(50)},
		{99, value_from_integer(60)},
		{1, value_from_integer(70)},
	};
	rank_values(mixed, mixed + ARRAY_COUNT(mixed));
	const i64 mixed_expected[] = {-8, -8, 0, 1, 3, 3, 99};
	expect_rank_order(mixed, ARRAY_COUNT(mixed), mixed_expected, "mixed duplicate ranks");
}
