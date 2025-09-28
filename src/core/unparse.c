//
// See Copyright Notice In elf.h
//








static void rank(RankValue *s, RankValue *e) {
	retry:
	if (e-s <= 1) return;

	Rank p = s->rank;

	RankValue *l = s + 0;
	RankValue *g = e - 1;
	RankValue *i = s + 1;
	RankValue  t;

	while (i <= g) {
		if (i->rank < p) {
			t = *i; *i ++ = *l; *l ++ = t;
		}
		else if (i->rank > p) {
			t = *i; *i    = *g; *g -- = t;
		}
		else {
			i ++;
		}
	}

	if (l - s < e - i) {
		rank(s, l);
		s = i;
		goto retry;
	}
	else {
		rank(i, e);
		e = l;
		goto retry;
	}
}


//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//




static void tabs(Stringer *sb, int num) {
	sb_repeatchar(sb, '\t', num);
}




static inline bool unparsable(Value v) {
	return is_num(v) || is_int(v) || is_str(v) || is_tab(v);
}




static int unparse(elf_State *S, Stringer *sb, V v, int level);




static void tableunparse(elf_State *S, Stringer *sb, Tab tab, int level) {

	sb_writetextf(sb, "{\n");

	Index i;
	int itemcounter = 0;

	// todo:
	// figure this out, or pass in flags to determine
	// whether to omit the hash part or the array part
	if (tab->fillcounter) {

		Index nen = tab->nentries;
		Entry *es = tab->entries;

		// todo: optional ranking
		RankValue *rv = malloc(nen * sizeof(*rv));
		memcpy(rv, es, nen * sizeof(*rv));
		rank(rv, rv + nen);


		for (i = 0; i < tab->nentries; ++ i) {

			V key = rv[i].value;
			V value = tab->array[rv[i].rank];

			// IndexValue entry = tab->slots[i];
			// Index index = entry.idx;
			// V key = entry.key;
			// V value = tab->array[index];


			if (!is_num(key) && !is_int(key) && !is_str(key)) {
				continue;
			}


			if (!unparsable(value)) {
				continue;
			}

			if (itemcounter ++) sb_writetextf(sb, ",\n");
			tabs(sb, level + 1);

			unparse(S, sb, key, 1);

			sb_writetextf(sb, " = ");

			unparse(S, sb, value, level + 1);
		}


		free(rv);
	}
	else {
		FOR_ARRAY(i, tab->array) {
			V value = tab->array[i];

			if (!unparsable(value)) {
				continue;
			}

			if (itemcounter ++) sb_writetextf(sb, ",\n");

			tabs(sb, level + 1);

			unparse(S, sb, value, level + 1);
		}

	}

	sb_writetextf(sb, "\n");

	tabs(sb, level);

	sb_writetextf(sb, "}");
}




// todo: cyclic references will break this
// todo: performance!
static int unparse(elf_State *S, Stringer *sb, V v, int level) {
	int noerror = true;

	switch (v.tag)
	{
		case ELF_TNIL: {
			sb_writetextf(sb, "nil");
		} break;

		case ELF_TINTEGER:  {
			sb_writetextf(sb, "%lli", v.x_int);
		} break;

		case ELF_TNUMBER: {
			sb_writetextf(sb, "%f", v.x_num);
		} break;

		case ELF_TSTRING: {
			sb_writechar(sb, '"');
			sb_writetextesc(sb, strt(as_string(v)));
			sb_writechar(sb, '"');
		} break;

		case ELF_TTABLE: {
			tableunparse(S, sb, as_table(v), level);
		} break;

		default: noerror = false;
	}

	return noerror;
}