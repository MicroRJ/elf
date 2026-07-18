//
// See Copyright Notice In elf.h
//



static u32 load_array_index_arg(elf_State *S, int arg, elf_Table *table)
{
	elf_Value value = load_value(S, arg);
	check_value_type(S, value, ELF_VALUE_TYPE_INTEGER);
	return check_array_index(S, NO_BYTE, value_as_integer(value), elf_array_len(table));
}

ELF_FUNCTION(l_table_make_field_readonly) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_table_mark_field_readonly(S, tab, load_value(S, 1));
	return 0;
}















ELF_FUNCTION(l_table_clear) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
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
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	tab->fillcounter = 0;
	memset(tab->entries, 0, tab->nentries * sizeof(*tab->entries));
	// todo: we can't do this!
	// table_clear_entries(tab);

	push_table(S, tab);
	return 1;
}








ELF_FUNCTION(l_table_contains) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value key = load_value(S, 1);
	push_value(S, value_from_integer(elf_table_contains(S, tab, key)));
	return 1;
}











ELF_FUNCTION(l_table_get_collisions) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	push_value(S, value_from_integer(tab->ndebug));
	return 1;
}























// @doc
// :delete(key) -> any
// the key and all aliases are deleted, the u32 is also deleted,
// order is preserved and other fields are adjusted.
//
ELF_FUNCTION(l_table_delete) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value key = load_value(S, 1);

	Entry *entries = tab->entries;

	u32 slot = table_find_slot(tab->entries, tab->nentries, key);

	if (slot != TABLE_SLOT_NOT_FOUND && entry_is_key(entries[slot])) {

		u32 index = entry_index(entries[slot]);

		Entry *en;
		for (en=tab->entries; en<tab->entries+tab->nentries; ++en) {

			if (entry_is_key(*en)) {
				u32 en_index = entry_index(*en);
				if (en_index == index) {
					entry_set_tomb(en);
				}
				else {
					entry_set_index(en, en_index - (en_index > index));
				}
			}

		}

		push_value(S, elf_array_get(S, tab, index));
		elf_array_remove(S, tab, index, 1);
	}
	else {

		push_value(S, value_nil());

	}
	return 1;
}













static inline void tablebind(elf_State *S, elf_Table *tab, elf_Value key, u32 index) {
	elf_table_bind_to_index(S, tab, key, index);
}
























// @doc
// :alias(existing_key, new_key) -> (none)
// creates a new one entry of the given 'new_key' that
// points to where 'existing_key' points to
ELF_FUNCTION(l_table_alias) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value x = load_value(S, 1);
	elf_Value y = load_value(S, 2);
	elf_table_alias(S, tab, x, y);
	return 0;
}















ELF_FUNCTION(l_table_bind) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	tablebind(S, tab, load_value(S, 1), load_array_index_arg(S, 2, tab));
	return 0;
}















// @doc
// :find_aliases(key) -> table
// finds all the aliases of the given 'key' including the
// key itself
ELF_FUNCTION(l_find_aliases) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value   key = load_value(S, 1);

	elf_Table * res = elf_push_new_table(S);

	if (!value_is_nil(key)) {

		u32 i=table_find_slot_for_write(S, tab, key);

		if (i > 0) {

			Entry entry=tab->entries[i];

			if (!entry_is_dead(entry)) {

				for (i = 0; i < tab->nentries; ++i) {
					/* we also include ourselves */

					Entry alias=tab->entries[i];

					if (entry_is_dead(alias)) continue;
					if (entry_index(alias) != entry_index(entry)) continue;

					elf_array_add(S, tab, entry_key_value(alias));
				}
			}
		}
	}
	return 1;
}














// @doc returns an elf_Value * containg all the keys for this table
ELF_FUNCTION(l_table_get_keys) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

	elf_push_new_table(S);

	u32 i;
	for (i = 0; i < tab->nentries; i ++) {
		Entry entry = tab->entries[i];
		// omit non-keys
		if (entry_is_dead(entry)) {
			continue;
		}

		push_value(S, entry_key_value(entry));
		elf_arr_add(S);
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

	u32 slot;

	elf_Table * sum = elf_push_new_table(S);

	for (int i = 0; i < nargs; ++ i) {
		elf_Value merger_value = load_value(S, i);
		check_value_type(S, merger_value, ELF_VALUE_TYPE_TABLE);
		elf_Table * merger = value_as_table(merger_value);
		for (slot = 0; slot < merger->nentries; ++ slot) {
			Entry entry = merger->entries[slot];

			if (entry_is_dead(entry)) continue;

			// todo:
			push_value(S, entry_key_value(entry));
			push_value(S, elf_array_get(S, merger, entry_index(entry)));
			elf_tab_set(S);
		}
	}

	return 1;
}


















ELF_FUNCTION(l_table_fork) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value sub_value = load_value(S, 1);
	check_value_type(S, sub_value, ELF_VALUE_TYPE_TABLE);
	elf_Table * sub = value_as_table(sub_value);

	if (sub == 0) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE, "argument is nil");
	}

	elf_push_new_table(S);

	u32 i;
	for (i = 0; i < tab->nentries; ++i) {

		Entry entry = tab->entries[i];

		if (entry_is_dead(entry)) continue;
		elf_Value key = entry_key_value(entry);
		if (elf_table_contains(S, sub, key)) continue;

		// todo: remove this!
		push_value(S, key);
		push_value(S, elf_array_get(S, tab, entry_index(entry)));
		elf_tab_set(S);
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DIRECT elf_Value * FUNCTIONS //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ELF_FUNCTION(l_array_length)
{
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	push_value(S, value_from_integer(elf_array_len(tab)));
	return 1;
}

ELF_FUNCTION(l_array_index)
{
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	u32 idx = load_array_index_arg(S, 1, tab);
	push_value(S, elf_array_get(S, tab, idx));
	return 1;
}

// @doc: returns the index of the first added item
ELF_FUNCTION(l_array_add)
{
	elf_Value table_value = load_value(S, 0);
	check_value_type(S, table_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *table = value_as_table(table_value);
	elf_Value value = load_value(S, 1);
	u32 index = elf_array_add(S, table, value);
	push_value(S, value_from_integer(index));

	for (int i = 2; i < nargs; i++)
	{
		value = load_value(S, i);
		elf_array_add(S, table, value);
	}

	return 1;
}

// :repl(u32, value)
//		@u32 to replace with @elf_Value
ELF_FUNCTION(l_array_repl) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	u32 idx = load_array_index_arg(S, 1, tab);
	elf_Value value = load_value(S, 2);

	// todo: barrier
	elf_array_set(S, tab, idx, value);
	return 0;
}

ELF_FUNCTION(l_array_swap)
{
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	u32 x = load_array_index_arg(S, 1, tab);
	u32 y = load_array_index_arg(S, 2, tab);

	elf_array_swap(S, tab, x, y);
	return 0;
}















ELF_FUNCTION(l_array_slice) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

	u32 x = 0;
	u32 y = elf_array_len(tab);
	if (nargs >= 2) x = load_array_index_arg(S, 0, tab);
	if (nargs >= 3) y = load_array_index_arg(S, 1, tab);

	elf_Table * slice = elf_push_new_table(S);
	while (x < y) {
		elf_array_add(S, slice, elf_array_get(S, tab, x ++));
	}
	return 1;
}

ELF_FUNCTION(l_array_remove) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	u32 index = load_array_index_arg(S, 1, tab);
	u32 num = 1;

	push_value(S, elf_array_get(S, tab, index));
	elf_array_remove(S, tab, index, num);

	return 1;
}












// todo: maybe it should return the trimmed elements,
// to make consistent with :slice
// { 1, 2, 3, 4 }:trim(-2) == 3
// { 1, 2, 3, 4 }:trim(-1) == 4
// { 1, 2, 3, 4 }:trim( 2) == 2
// { 1, 2, 3, 4 }:trim( 1) == 1
ELF_FUNCTION(l_array_trim) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value num_value = load_value(S, 1);
	check_value_type(S, num_value, ELF_VALUE_TYPE_INTEGER);
	i64 num = value_as_integer(num_value);
	u32 len = elf_array_len(tab);

	if (num == 0) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "array trim count cannot be zero");
	}

	if (num < 0) {
		u32 count = (u32)-num;
		if (count > len) {
			report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1
			,	"array trim out of bounds: count %u, length %u"
			,	count, len);
		}

		u32 index = len - count;
		push_value(S, elf_array_get(S, tab, index));
		elf_array_remove(S, tab, index, count);
	}
	else {
		u32 count = (u32)num;
		if (count > len) {
			report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1
			,	"array trim out of bounds: count %u, length %u"
			,	count, len);
		}

		push_value(S, elf_array_get(S, tab, count - 1));
		elf_array_remove(S, tab, 0, count);
	}
	return 1;
}













ELF_FUNCTION(l_array_merge) {
	elf_Value sum_value = load_value(S, 0);
	check_value_type(S, sum_value, ELF_VALUE_TYPE_TABLE);
	elf_Table * sum = value_as_table(sum_value);
	push_table(S, sum);

	u32 i, j;

	for (i = 1; i < nargs; ++ i) {
		elf_Value tab_value = load_value(S, i);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

		for (j = 0; j < elf_array_len(tab); ++ j) {

			elf_Value value = elf_array_get(S, tab, j);
			elf_array_add(S, sum, value);

		}
	}
	return 1;
}











// todo: deep copy!
ELF_FUNCTION(l_array_clone) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

	elf_Table *clone = elf_push_new_table(S);

	// todo: hello????? memcpy?
	u32 i;
	for (i=0; i<elf_array_len(tab); ++i) {
		elf_array_add(S, clone, elf_array_get(S, tab, i));
	}
	return 1;
}












ELF_FUNCTION(l_array_reverse) {

	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	u32 n = elf_array_len(tab);

	u32 i;
	for (i = 0; i < n >> 1; i += 1) {
		elf_array_swap(S, tab, i, n - 1 - i);
	}
	return 0;
}















ELF_FUNCTION(l_table_get_pairs) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

	elf_Table * res = elf_push_new_table(S);

	u32 i;
	for (i = 0; i < tab->nentries; ++ i) {
		Entry entry = tab->entries[i];

		// omit non-keys
		if (entry_is_dead(entry)) {
			continue;
		}

		// todo: prealloc!
		elf_array_add(S, res, entry_key_value(entry));
		elf_array_add(S, res, elf_array_get(S, tab, entry_index(entry)));
	}
	return 1;
}















ELF_FUNCTION(l_table_ordered_pairs)
{
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Table * res = elf_push_new_table(S);

	u32 nen = tab->nentries;
	Entry *es = tab->entries;

	// todo: optional ranking
	RankValue *rv = malloc(nen * sizeof(*rv));
	for (u32 i = 0; i < nen; ++ i) {
		rv[i].rank = entry_index(es[i]);
		rv[i].value = entry_key_value(es[i]);
	}
	rank_values(rv, rv + nen);


	// todo: can unrolling help a bit since we're fetching from
	// two buffers or will the compiler figure this out?
	u32 i;
	for(i=0; i<nen; ++i) {
		if (!value_is_dead(rv[i].value)) {
			elf_array_add(S, res, rv[i].value);
			elf_array_add(S, res, elf_array_get(S, tab, rv[i].rank));
		}
	}

	free(rv);

	return 1;
}

// todo: should this return a new table or no?
ELF_FUNCTION(l_array_filter) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value filter = load_value(S, 1);
	check_value_type_rule(S, filter, TRULE_CALLABLE);

	u32 l = elf_array_len(tab);
	u32 i;
	u32 write_index = 0;

	for (i=0; i<l; ++i) {
		elf_Value value = elf_array_get(S, tab, i);
		push_value(S, filter);
		push_value(S, load_value(S, 0));
		push_value(S, value);
		elf_call(S, 2, 1);
		elf_Value keep_value = pop_value(S);
		check_value_type(S, keep_value, ELF_VALUE_TYPE_INTEGER);
		int keep = value_as_integer(keep_value);
		if (keep) {
			elf_array_set(S, tab, write_index ++, value);
		}
	}

	if (write_index < l) {
		elf_array_remove(S, tab, write_index, l - write_index);
	}

	// return self
	push_table(S, tab);
	return 1;
}

ELF_FUNCTION(l_array_rank)
{
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);

	elf_Value fn;
	if (nargs > 1) {
		fn = load_value(S, 1);
		check_value_type_rule(S, fn, TRULE_CALLABLE);
	}

	u32 i, l = elf_array_len(tab);
	RankValue *rv = malloc(l * sizeof(*rv));

	elf_Value __rank;
	__rank = value_from_atom(elf_atom_from_data(S, "__rank"));

	for (i=0; i<l; ++i) {
		elf_Value value = elf_array_get(S, tab, i);

		i64 rank;
		if (nargs > 1) {
			// todo: elf_call doesn't have to write the results where the function is at!
			push_value(S, fn);
			push_value(S, load_value(S, 0));
			push_value(S, value);
			elf_call(S, 2, 1);
			elf_Value rank_value = pop_value(S);
			check_value_type(S, rank_value, ELF_VALUE_TYPE_INTEGER);
			rank = value_as_integer(rank_value);
		}
		else {
			elf_Value value = elf_array_get(S, tab, i);
			check_value_type(S, value, ELF_VALUE_TYPE_TABLE);
			elf_Value r = elf_table_get_or_nil(S, value_as_table(value), __rank);
			check_value_type(S, r, ELF_VALUE_TYPE_INTEGER);
			rank = value_as_integer(r);
		}

		rv[i].rank  = rank;
		rv[i].value = value;
	}

	rank_values(rv, rv + l);

	for (i=0; i<l; ++i) {
		elf_array_set(S, tab, i, rv[i].value);
	}
	free(rv);

	push_table(S, tab);
	return 1;
}









static inline i64 callsortfn(elf_State *S, elf_Value fn, elf_Value a, elf_Value b) {
	push_value(S, fn);
	push_value(S, load_value(S, 0));
	push_value(S, a);
	push_value(S, b);
	// todo: why is the result placed where the function is at!
	elf_call(S, 3, 1);
	elf_Value result = pop_value(S);
	check_value_type(S, result, ELF_VALUE_TYPE_INTEGER);
	return value_as_integer(result);
}












static void quicksort(elf_State *S, elf_Value fn, elf_Table *tab, u32 head, u32 tail) {

	retry:
	if (tail - head <= 1) return;

	elf_Value pivo = elf_array_get(S, tab, head);

	u32 lt = head;
	u32 gt = tail - 1;
	u32 i  = head + 1;

	while (i <= gt) {

		i64 c = callsortfn(S, fn, elf_array_get(S, tab, i), pivo);

		if (c < 0) {
			elf_array_swap(S, tab, i, lt);
			lt ++;
			i ++;
		}
		else if (c > 0) {
			elf_array_swap(S, tab, i, gt);
			gt --;
		}
		else {
			i ++;
		}
	}

	if (lt - head < tail - i) {
		quicksort(S, fn, tab, head, lt);
		head = i;
		goto retry;
	}
	else {
		quicksort(S, fn, tab, i, tail);
		tail = lt;
		goto retry;
	}
}









ELF_FUNCTION(l_array_sort) {
	elf_Value tab_value = load_value(S, 0);
	check_value_type(S, tab_value, ELF_VALUE_TYPE_TABLE);
	elf_Table *tab = value_as_table(tab_value);
	elf_Value fun = load_value(S, 1);
	check_value_type_rule(S, fun, TRULE_CALLABLE);
	quicksort(S, fun, tab, 0, elf_array_len(tab));
	push_table(S, tab);
	return 1;
}










static const elf_Binding l_table[] = {
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

	// elf_Value * part
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

static elf_Table *elf_lib_table(elf_State *state)
{
	return new_binding_table(state, l_table, ARRAY_COUNT(l_table));
}
