//
// See Copyright Notice In elf.h
//

static void rank_values(RankValue *start, RankValue *end)
{
	while (end - start > 1)
	{
		i64 pivot = start->rank;
		RankValue *less = start;
		RankValue *greater = end - 1;
		RankValue *cursor = start + 1;

		while (cursor <= greater)
		{
			if (cursor->rank < pivot)
			{
				RankValue temp = *cursor;
				*cursor++ = *less;
				*less++ = temp;
			}
			else if (cursor->rank > pivot)
			{
				RankValue temp = *cursor;
				*cursor = *greater;
				*greater-- = temp;
			}
			else
			{
				cursor++;
			}
		}

		if (less - start < end - cursor)
		{
			rank_values(start, less);
			start = cursor;
		}
		else
		{
			rank_values(cursor, end);
			end = less;
		}
	}
}
