//
// See Copyright Notice In elf.h
//







// todo: merge with the version in internal_main.c!
static Index loadindex(elf_State *S, int x, Tab tab) {
	Index index = loadint(S, x);
	if (index < 0) index += heap_array_length(tab->array);

	if (index < 0 || index >= heap_array_length(tab->array)) {
		reporterrorf(S, -1, "'%lli': index out of bounds", index);
	}
	return index;
}











ELF_FUNCTION(l_table_make_field_readonly) {
	Tab tab = loadtable(S, 0);
	_table_markfieldreadonly(S, tab, loadvalue(S, 1));
	return 0;
}









ELF_FUNCTION(l_table_get_meta) {
	Tab tab = loadtable(S, 0);
	pushtab(S, getmeta(tab));
	return 1;
}










ELF_FUNCTION(l_table_set_meta) {
	Tab tab = loadtable(S, 0);
	Tab meta = loadtable(S, 1);
	setmeta(tab, meta);
	pushtab(S, meta);
	return 1;
}


















ELF_FUNCTION(l_table_clear) {
	Tab tab = loadtable(S, 0);
	// todo: we can't do this!
	// table_clear(tab);
	return 0;
}

//
//
//
//
//
//
//
//

ELF_FUNCTION(l_table_clear_entries) {
	Tab tab = loadtable(S, 0);
	tab->fillcounter = 0;
	memset(tab->entries, 0, tab->nentries * sizeof(*tab->entries));
	// todo: we can't do this!
	// table_clear_entries(tab);

	pushtab(S, tab);
	return 1;
}








ELF_FUNCTION(l_table_contains) {
	Tab tab = loadtable(S, 0);
	Val key = loadvalue(S, 1);
	pushint(S, _table_contains(S, tab, key));
	return 1;
}











ELF_FUNCTION(l_table_get_collisions) {
	Tab tab = loadtable(S, 0);
	pushint(S, tab->ndebug);
	return 1;
}











static inline void arrayremove(Value *values, Index index, Index num) {
	HArray *array = d_array_raw(values);
	memmove(values + index, values + index + num, (array->min - index - num) * sizeof(*values));
	array->min -= num;
}













// @doc
// :delete(key) -> any
// the key and all aliases are deleted, the index is also deleted,
// order is preserved and other fields are adjusted.
//
ELF_FUNCTION(l_table_delete) {
	Tab tab = loadtable(S, 0);
	Val key = loadvalue(S, 1);


	Entry *entries = tab->entries;
	Value *values = tab->values;
	HArray *array = d_array_raw(values);


	Index slot = _table_findentry_internal(tab->entries, tab->nentries, key);

	if ((slot >= 0) && (entries[slot].key.tag != ELF_TNIL) && (entries[slot].key.tag != ELF_TTOMB)) {

		Index index = entries[slot].index;

		Entry *en;
		for (en=tab->entries; en<tab->entries+tab->nentries; ++en) {

			// disable
			if (en->index == index) {
				if (en->key.tag != ELF_TNIL) {
					en->key.tag = ELF_TTOMB;
				}
			}
			else {
				// fixup index
				en->index -= en->index > index;
			}

		}


		pushvalueunsafe(S, values[index]);

		arrayremove(values, index, 1);
	}
	else {

		pushnil(S);

	}
	return 1;
}













static inline void tablebind(elf_State *S, Tab tab, V key, Index index) {
	_table_bindtoindex(S, tab, key, index);
}
























// @doc
// :alias(existing_key, new_key) -> (none)
// creates a new one entry of the given 'new_key' that
// points to where 'existing_key' points to
ELF_FUNCTION(l_table_alias) {
	Tab tab = loadtable(S, 0);
	V x = loadvalue(S, 1);
	V y = loadvalue(S, 2);
	tablealias(S, tab, x, y);
	return 0;
}















ELF_FUNCTION(l_table_bind) {
	Tab tab = loadtable(S, 0);
	tablebind(S, tab, loadvalue(S, 1), loadindex(S, 2, tab));
	return 0;
}















// @doc
// :find_aliases(key) -> table
// finds all the aliases of the given 'key' including the
// key itself
ELF_FUNCTION(l_find_aliases) {
	Tab tab = loadtable(S, 0);
	V   key = loadvalue(S, 1);

	Tab res = pushnewtable(S);

	if (!is_nil(key)) {

		Index i=_table_tryresize(S, tab, key);

		if (i > 0) {

			IndexValue entry=tab->entries[i];

			if (!isdead(entry.key)) {

				for (i = 0; i < tab->ntotal; ++i) {
					/* we also include ourselves */

					IndexValue alias=tab->entries[i];

					if (isdead(alias.key)) continue;
					if (alias.idx != entry.idx) continue;

					_table_arrayadd(S, tab, alias.key);
				}
			}
		}
	}
	return 1;
}














// @doc returns an array containg all the keys for this table
ELF_FUNCTION(l_table_get_keys) {
	Tab tab = loadtable(S, 0);

	elf_pushtab(S);

	Index i;
	for (i = 0; i < tab->ntotal; i ++) {
		IndexValue entry = tab->slots[i];
		// omit non-keys
		if (isdead(entry.key)) {
			continue;
		}

		pushvalueunsafe(S, entry.key);
		elf_arrayadd(S);
	}
	return 1;
}











//
// todo: all these functions that return a new table should inherit
// the parent's metatable
//
// @doc :table_merge
// merges all the tables passed in, keys are not overwritten
// * - this works per entry, so loose values aren't kept
// * - if no additional tables are passed in then this is equivalent
// * to creating a clone of the table, but additionally, loose values are lost.
ELF_FUNCTION(l_table_merge) {

	Index slot;

	Tab sum = pushnewtable(S);

	for (int i = 0; i < nargs; ++ i) {
		Tab merger = loadtable(S, i);
		for (slot = 0; slot < merger->ntotal; ++ slot) {
			IndexValue entry = merger->entries[slot];

			if (isdead(entry.key)) continue;

			// todo:
			pushvalueunsafe(S, entry.key);
			pushvalueunsafe(S, merger->array[entry.idx]);
			elf_setfield(S);
		}
	}

	return 1;
}


















ELF_FUNCTION(l_table_fork) {
	Tab tab = loadtable(S, 0);
	Tab sub = loadtable(S, 1);

	if (sub == 0) {
		reporterror(S, NO_BYTE, "argument is nil");
	}

	pushnewtable(S);

	Index i;
	for (i = 0; i < tab->ntotal; ++i) {

		IndexValue entry = tab->entries[i];

		if (isdead(entry.key)) continue;
		if (_table_contains(S, sub, entry.key)) continue;

		// todo: remove this!
		pushvalueunsafe(S, entry.key);
		pushvalueunsafe(S, tab->array[entry.idx]);
		elf_setfield(S);
	}

	return 1;
}












ELF_FUNCTION(l_array_length) {
	Tab tab = loadtable(S, 0);
	pushint(S, heap_array_length(tab->array));
	return 1;
}














ELF_FUNCTION(l_array_index) {
	Tab tab = loadtable(S, 0);
	Index idx = loadindex(S, 1, tab);
	pushvalueunsafe(S, tab->array[idx]);
	return 1;
}

















// @doc: returns the index of the first added item
ELF_FUNCTION(l_array_add) {
	Tab tab = loadtable(S, 0);

	V v = loadvalue(S, 1);
	Index index = _table_arrayadd(S, tab, v);
	pushint(S, index);

	for (int i = 2; i < nargs; i++) {
		v = loadvalue(S, i);
		_table_arrayadd(S, tab, v);
	}

	return 1;
}

















// the index of the thing to replace, the value to replace
// it with
ELF_FUNCTION(l_array_repl) {
	Tab   tab = loadtable(S, 0);
	Index idx = loadindex(S, 1, tab);
	V     val = loadvalue(S, 2);

	// todo: barrier
	tab->array[idx] = val;
	return 0;
}


















ELF_FUNCTION(l_array_swap) {
	Tab tab = loadtable(S, 0);
	Index x = loadindex(S, 1, tab);
	Index y = loadindex(S, 2, tab);

	swap_values(&tab->array[x], &tab->array[y]);
	return 0;
}















ELF_FUNCTION(l_array_slice) {
	Tab tab = loadtable(S, 0);

	Index x = 0;
	Index y = heap_array_length(tab->array);
	if (nargs >= 2) x = loadindex(S, 0, tab);
	if (nargs >= 3) y = loadindex(S, 1, tab);

	Tab slice = pushnewtable(S);
	while (x < y) {
		heap_array_add(slice->array, tab->array[x ++]);
	}
	return 1;
}








ELF_FUNCTION(l_array_remove) {
	Tab tab = loadtable(S, 0);
	Index index = loadindex(S, 1, tab);
	Index num = 1;

	pushvalueunsafe(S, tab->values[index]);
	arrayremove(tab->values, index, num);

	return 1;
}












// todo: maybe it should return the trimmed elements,
// to make consistent with :slice
// { 1, 2, 3, 4 }:trim(-2) == 3
// { 1, 2, 3, 4 }:trim(-1) == 4
// { 1, 2, 3, 4 }:trim( 2) == 2
// { 1, 2, 3, 4 }:trim( 1) == 1
ELF_FUNCTION(l_array_trim) {
	Tab tab = loadtable(S, 0);
	int num = loadint(S, 1);

	HArray *darr = d_array_raw(tab->values);

	if (num < 0) {
		darr->min += num;
		pushvalueunsafe(S, tab->values[darr->min]);
	}
	else {
		pushvalueunsafe(S, tab->values[num - 1]);
		darr->min -= num;
		memmove(tab->values, tab->values + num, darr->min * sizeof(* tab->values));
	}
	return 1;
}













ELF_FUNCTION(l_array_merge) {
	Tab sum = loadtable(S, 0);
	pushtab(S, sum);

	Index i, j;

	for (i = 1; i < nargs; ++ i) {
		Tab tab = loadtable(S, i);

		for (j = 0; j < _table_arraylen(tab); ++ j) {

			Value v = _table_arrayget(S, tab, j);
			_table_arrayadd(S, sum, v);

		}
	}
	return 1;
}











// todo: deep copy!
ELF_FUNCTION(l_array_clone) {
	Tab tab = loadtable(S, 0);

	Table *clone = pushnewtable(S);

	// todo: hello????? memcpy?
	Index i;
	for (i=0; i<heap_array_length(tab->array); ++i) {
		heap_array_add(clone->array, tab->array[i]);
	}
	return 1;
}












ELF_FUNCTION(l_array_reverse) {

	Tab tab = loadtable(S, 0);
	Index n = heap_array_length(tab->array);

	V *array = tab->array;

	Index i;
	for (i = 0; i < n >> 1; i += 1) {
		swap_values(&array[i], &array[n-1-i]);
	}
	return 0;
}















ELF_FUNCTION(l_table_get_pairs) {
	Tab tab = loadtable(S, 0);

	Tab res = pushnewtable(S);

	Index i;
	for (i = 0; i < tab->nentries; ++ i) {
		Entry entry = tab->entries[i];

		// omit non-keys
		if (isdead(entry.key)) {
			continue;
		}

		// todo: prealloc!
		heap_array_add(res->array, entry.key);
		heap_array_add(res->array, tab->array[entry.index]);
	}
	return 1;
}















ELF_FUNCTION(l_table_ordered_pairs)
{
	STATIC_ASSERT(sizeof(RankValue) == sizeof(IndexValue));
	STATIC_ASSERT(offsetof(RankValue, value) == offsetof(IndexValue,   key));
	STATIC_ASSERT(offsetof(RankValue,  rank) == offsetof(IndexValue, index));


	Tab tab = loadtable(S, 0);
	Tab res = pushnewtable(S);

	Index nen = tab->nentries;
	Entry *es = tab->entries;

	// todo: optional ranking
	RankValue *rv = malloc(nen * sizeof(*rv));
	memcpy(rv, es, nen * sizeof(*rv));
	rank(rv, rv + nen);


	Value *arr = new_heap_array(sizeof(*arr), tab->fillcounter * 2);
	res->array = arr;

	// todo: can unrolling help a bit since we're fetching from
	// two buffers or will the compiler figure this out?
	Index c, i;
	for(c=0, i=0; i<nen; ++i) {
		if (!isdead(rv[i].value)) {
			arr[c ++] = rv[i].value;
			arr[c ++] = tab->array[rv[i].rank];
		}
	}

	free(rv);

	return 1;
}

// todo: should this return a new table or no?
ELF_FUNCTION(l_array_filter) {
	Tab tab = loadtable(S, 0);
	V filter = loadrulecheck(S, 1, TRULE_CALLABLE);

	V *new_array = 0;

	Index l = heap_array_length(tab->array);
	Index i;

	for (i=0; i<l; ++i) {
		Value value = tab->array[i];
		pushvalueunsafe(S, filter);
		pushthis(S);
		pushvalueunsafe(S, value);
		elf_call(S, 2, 1);
		int keep = popint(S);
		if (keep) {
			heap_array_add(new_array, value);
		}
	}

	// todo:
	free_heap_array(tab->array);

	tab->array = new_array;

	// return self
	pushtab(S, tab);
	return 1;
}

ELF_FUNCTION(l_array_rank)
{
	Tab tab = loadtable(S, 0);

	V fn;
	if (nargs > 1) {
		fn = loadrulecheck(S, 1, TRULE_CALLABLE);
	}

	Index i, l = heap_array_length(tab->array);
	RankValue *rv = malloc(l * sizeof(*rv));

	Value __rank;
	to_str(&__rank, _string_new(S, "__rank"));

	for (i=0; i<l; ++i) {
		Value value = tab->array[i];

		Index rank;
		if (nargs > 1) {
			// todo: elf_call doesn't have to write the results where the function is at!
			pushvalueunsafe(S, fn);
			pushthis(S);
			pushvalueunsafe(S, value);
			elf_call(S, 2, 1);
			rank = popint(S);
		}
		else {
			V v = _table_arrayget(S, tab, i);
			typecheck(S, v, ELF_TTABLE);
			Value r = _table_getornil(S, as_table(v), __rank);
			typecheck(S, r, ELF_TINTEGER);
			rank = as_int(r);
		}

		rv[i].rank  = rank;
		rv[i].value = value;
	}

	rank(rv, rv + l);

	for (i=0; i<l; ++i) {
		tab->array[i]=rv[i].value;
	}
	free(rv);

	pushtab(S, tab);
	return 1;
}









static inline Index callsortfn(elf_State *S, V fn, V a, V b) {
	pushvalueunsafe(S, fn);
	pushthis(S);
	pushvalueunsafe(S, a);
	pushvalueunsafe(S, b);
	// todo: why is the result placed where the function is at!
	elf_call(S, 3, 1);
	return popint(S);
}












static void quicksort(elf_State *S, V fn, V *head, V *tail) {

	retry:
	if (tail-head <= 1) return;

	V pivo = *head;

	V *lt = head + 0;
	V *gt = tail - 1;
	V *i  = head + 1;

	while (i <= gt) {

		Index c = callsortfn(S, fn, *i, pivo);

		if (c < 0) {
			swap_values(i, lt ++);
			i ++;
		}
		else if (c > 0) {
			swap_values(i, gt --);
		}
		else {
			i ++;
		}
	}

	if (lt - head < tail - i) {
		quicksort(S, fn, head, lt);
		head = i;
		goto retry;
	}
	else {
		quicksort(S, fn, i, tail);
		tail = lt;
		goto retry;
	}
}









ELF_FUNCTION(l_array_sort) {
	Tab tab = loadtable(S, 0);
	V fun = loadcallable(S, 1);
	quicksort(S, fun, tab->array, tab->array + heap_array_length(tab->array));
	pushtab(S, tab);
	return 1;
}










static const elf_Binding l_table[] = {
	// object
	{"get_meta"     , l_table_get_meta         },
	{"set_meta"     , l_table_set_meta         },

	// table
	{"clear",         l_table_clear            },
	{"clear_entries", l_table_clear_entries    },

	{"ordered_pairs" , l_table_ordered_pairs   },
	{"get_pairs"     , l_table_get_pairs       },

	{"make_field_readonly", l_table_make_field_readonly },

	{"haskey"       , l_table_contains         },
	{"collisions"   , l_table_get_collisions   },
	{"get_keys"     , l_table_get_keys         },
	{"find_aliases" , l_find_aliases           },


	{"alias"        , l_table_alias            },
	{"bind"         , l_table_bind             },


	{"merge"        , l_table_merge            },
	{"fork"         , l_table_fork             },

	// aware of both
	{"delete"       , l_table_delete       },

	// array part
	{"sort"         , l_array_sort         },
	{"rank"         , l_array_rank         },
	{"filter"       , l_array_filter       },


	{"array_remove" , l_array_remove       },
	{"trim"         , l_array_trim         },
	{"swap"         , l_array_swap         },
	{"length"       , l_array_length       },
	{"tally"        , l_array_length       },
	{"add"          , l_array_add          },
	{"idx"          , l_array_index        },
	{"repl"         , l_array_repl         },
	{"reverse"      , l_array_reverse      },
	{"merge_array"  , l_array_merge        },
	{"clone_array"  , l_array_clone        },
	{"slice"        , l_array_slice        },
};