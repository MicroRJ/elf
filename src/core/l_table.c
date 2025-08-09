//
// See Copyright Notice In elf.h
//


ELF_FUNCTION(l_table_get_meta) {
	elf_getmetatab(S, args + 0);
	return 1;
}


ELF_FUNCTION(l_table_set_meta) {
	elf_setmetatab(S, args + 0, args + 1);
	return 0;
}


static inline bool tablecontains(elf_Table *tab, elf_Value key) {
	index_t slot = elf_table_try_(tab, key);
	return slot >= 0 && !isdead(tab->entries[slot].key);
}

ELF_FUNCTION(l_table_contains) {
	ASSERT((nargs - 1) == 1);
	elf_Table *tab = f_checktable(S, -1);
	elf_Value key = loadvalue(S, 0);
	elf_pushint(S, tablecontains(tab, key));
	return 1;
}


ELF_FUNCTION(l_table_get_collisions) {
	elf_Table *tab = f_checktable(S, -1);
	elf_pushint(S, tab->ndebug);
	return 1;
}

// @doc
// :delete(key) -> any
// deletes the all entries for the key specified, the item they
// point to is removed from the array and the array is shifted,
// all other keys are fixed to point to their new item
// positions
// the return value is the item that was deleted!
ELF_FUNCTION(l_table_delete) {
	ASSERT((nargs - 1) >= 1);

	elf_Table *tab = f_checktable(S, -1);
	elf_Value key = loadvalue(S, 0);

	elf_Table_Entry *entries = tab->entries;
	elf_Value *array = tab->array;

	index_t slot = elf_table_try_(tab, key);

	if ((slot >= 0) && (entries[slot].key.tag != ELF_TNIL) && (entries[slot].key.tag != ELF_TTOMB)) {

		index_t idx = entries[slot].idx;
		pushvalue(S, array[idx]);

		index_t i;
		for (i = 0; i < tab->ntotal; i ++) {
			if (entries[i].idx == idx) {
				entries[i].key.tag = ELF_TTOMB;
			} else if(entries[i].idx > idx) {
				ASSERT(entries[i].idx > 0);
				entries[i].idx -= 1;
			}
		}

		memmove(array + idx, array + idx + 1, (darr_l(array) - idx - 1) * sizeof(elf_Value));

		ARRAY_SET_MIN(array, darr_l(array) - 1);
	} else {
		elf_pushnil(S);
	}
	return 1;
}


static inline void aliastable(elf_Table *tab, elf_Value key, elf_Value alias) {
	elf_Integer slot = elf_table_try_(tab,key);
	if (slot >= 0) {
		if (!isdead(tab->entries[slot].key)) {
			elf_Integer aliasslot = elf_table_try_(tab, alias);
			tab->entries[aliasslot].key = alias;
			tab->entries[aliasslot].idx = tab->entries[slot].idx;
		}
	}
}


// @doc
// :alias(existing_key, new_key) -> (none)
// creates a new one entry of the given 'new_key' that
// points to where 'existing_key' points to
ELF_FUNCTION(l_table_alias) {
	elf_Table *tab = f_checktable(S, -1);
	aliastable(tab, loadvalue(S,0), loadvalue(S,1));
	return 0;
}


// @doc
// :find_aliases(key) -> table
// finds all the aliases of the given 'key' including the
// key itself
ELF_FUNCTION(l_find_aliases) {
	elf_Table *tab = f_checktable(S, -1);

	elf_Value key = loadvalue(S,0);

	elf_pushtab(S);

	index_t i;
	elf_Table_Entry entry;

	if (key.tag != ELF_TNIL) {

		i = elf_table_try_(tab, key);

		if (i > 0) {

			entry = tab->slots[i];

			if (!isdead(entry.key)) {

				for (i = 0; i < tab->ntotal; ++i) {
					/* we also include ourselves */

					elf_Table_Entry alias = tab->slots[i];

					if (isdead(alias.key)) continue;
					if (alias.idx != entry.idx) continue;

					pushvalue(S, alias.key);
					elf_arrayadd(S);
				}
			}
		}
	}
	return 1;
}


//
// @doc
// returns an array containg all the keys for this table
//
ELF_FUNCTION(l_table_get_keys) {
	elf_Table *tab = f_checktable(S, -1);

	elf_pushtab(S);

	index_t i;
	for (i = 0; i < tab->ntotal; i ++) {
		elf_Table_Entry entry = tab->slots[i];
		// omit non-keys
		if (isdead(entry.key)) {
			continue;
		}

		pushvalue(S, entry.key);
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
	elf_pushtab(S);

	index_t slot;
	elf_Table_Entry entry;

	for (int i = -1; i < (nargs - 1); ++ i ) {
		elf_Table *merger = f_checktable(S, i);
		for (slot = 0; slot < merger->ntotal; ++ slot) {
			entry = merger->slots[slot];

			if (isdead(entry.key)) continue;

			pushvalue(S, entry.key);
			pushvalue(S, merger->array[entry.idx]);
			elf_setfield(S);
		}
	}

	return 1;
}

ELF_FUNCTION(l_table_fork) {
	elf_Table *tab = f_checktable(S, -1);
	elf_Table *sub = f_checktable(S,  0);

	if (sub == 0) {
		elf_error(S, NO_BYTE, "argument is nil");
	}

	elf_pushtab(S);

	index_t i;
	for (i = 0; i < tab->ntotal; ++i) {

		elf_Table_Entry entry = tab->entries[i];

		if (isdead(entry.key)) continue;
		if (tablecontains(sub, entry.key)) continue;

		pushvalue(S, entry.key);
		pushvalue(S, tab->array[entry.idx]);
		elf_setfield(S);
	}

	return 1;
}


ELF_FUNCTION(l_array_length) {
	elf_Table *tab = f_checktable(S, -1);
	elf_pushint(S, darr_l(tab->array));
	return 1;
}

static void checktab(elf_State *inter, elf_stkid stk) {
	if (!istab(inter->stack[stk])) {
		elf_errorf(inter, -1, "'%s': expected 'table'", tag2s[vgettag(inter->stack[stk])]);
	}
}


ELF_FUNCTION(l_array_index) {
	checktab(S, args + 0);
	elf_Table *tab = f_checktable(S, -1);

	index_t idx = elf_toint(S, args + 1);
	index_t len = darr_l(tab->array);

	if (idx < 0 || idx >= len) {
		elf_errorf(S, -1, "'%lli': index out of bounds", idx);
	}

	// if ((idx %= len) < 0) idx += len;

	pushvalue(S, tab->array[idx]);
	return 1;
}

ELF_FUNCTION(l_array_add) {
	elf_Table *tab = f_checktable(S, -1);

	for (int i = 0; i < nargs - 1; i++) {
		elf_Value itm = loadvalue(S, i);
		elf_raw_array_add(tab, itm);
	}
	return 0;
}


ELF_FUNCTION(l_array_replace) {

	elf_Table *tab = f_checktable(S, -1);
	elf_Value value = loadvalue(S, 0);

	index_t len = darr_l(tab->array);
	index_t idx = f_checkint(S, 1);
	if ((idx %= len) < 0) idx += len;

	tab->array[idx] = value;
	return 0;
}


ELF_FUNCTION(l_array_swap) {
	elf_Table *tab = f_checktable(S, -1);
	index_t x = elf_toint(S, args + 0);
	index_t y = elf_toint(S, args + 1);

	if (y < 0) y += darr_l(tab->array);
	if (x < 0) x += darr_l(tab->array);

	elf_Value temp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = temp;
	return 0;
}



ELF_FUNCTION(l_array_pop) {
	elf_Table *tab = f_checktable(S, -1);

	Dynamic_Array *darr = d_array_raw(tab->array);
	pushvalue(S, tab->array[-- darr->min]);

	return 1;
}


ELF_FUNCTION(l_array_merge) {

	elf_Table *tab = f_checktable(S, -1);
	elf_Table *add = f_checktable(S,  0);

	elf_Table *sum = elf_alloc_table(S);
	vsettab(S->stack_ptr ++, tab);

	index_t i;
	for (i=0;i<darr_l(tab->array);++i) {
		d_array_add(sum->array, tab->array[i]);
	}
	for (i=0;i<darr_l(add->array);++i) {
		d_array_add(sum->array, tab->array[i]);
	}
	return 1;
}

ELF_FUNCTION(l_array_clone) {
	elf_Table *tab = f_checktable(S, -1);

	elf_pushtab(S);

	index_t i;
	for (i=0;i<darr_l(tab->array);++i)
	{
		pushvalue(S, tab->array[i]);
		elf_arrayadd(S);
	}
	return 1;
}

ELF_FUNCTION(l_array_reverse) {

	elf_Table *tab = f_checktable(S, -1);
	index_t n = darr_l(tab->array);

	elf_Value *array = tab->array;

	index_t i;
	for (i = 0; i < n >> 1; i += 1) {
		elf_Value value = array[i];
		array[i] = array[n-1-i];
		array[n-1-i] = value;
	}
	return 0;
}


ELF_FUNCTION(l_array_slice) {
	elf_Table *tab = f_checktable(S, -1);

	index_t x = 0;
	index_t y = darr_l(tab->array);
	if ((nargs - 1) >= 1) x = f_checkint(S,0);
	if ((nargs - 1) >= 2) y = f_checkint(S,1);

	elf_pushtab(S);
	while (x < y) {
		pushvalue(S, tab->array[x ++]);
		elf_arrayadd(S);
	}
	return 1;
}




elf_Binding table_metafuncs[] = {
	// object
	{"get_meta"     , l_table_get_meta         },
	{"set_meta"     , l_table_set_meta         },

	// table
	{"haskey"       , l_table_contains         },
	{"collisions"   , l_table_get_collisions   },
	{"get_keys"     , l_table_get_keys           },
	{"find_aliases" , l_find_aliases             },
	{"alias"        , l_table_alias              },
	{"merge"        , l_table_merge              },
	{"fork"         , l_table_fork               },

	// aware of both
	{"delete"       , l_table_delete       },

	// array part
	{"pop"          , l_array_pop          },
	{"swap"         , l_array_swap         },
	{"length"       , l_array_length       },
	{"tally"        , l_array_length       },
	{"add"          , l_array_add          },
	{"idx"          , l_array_index        },
	{"replace"      , l_array_replace      },
	{"reverse"      , l_array_reverse      },
	{"merge_array"  , l_array_merge        },
	{"clone_array"  , l_array_clone        },
	{"slice"        , l_array_slice        },
};