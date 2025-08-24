//
// See Copyright Notice In elf.h
//



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


static inline bool tablecontains(Tab t, V k) {
	Index s = tabletry(t, k);
	return s >= 0 && !isdead(t->entries[s].key);
}



ELF_FUNCTION(l_table_contains) {
	Tab tab = loadtable(S, 0);
	Val key = loadvalue(S, 1);
	pushint(S, tablecontains(tab, key));
	return 1;
}



ELF_FUNCTION(l_table_get_collisions) {
	Tab tab = loadtable(S, 0);
	pushint(S, tab->ndebug);
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

	Tab tab = loadtable(S, 0);
	Val key = loadvalue(S, 0);

	TEntry *entries = tab->entries;
	Val      *array = tab->  array;

	Index slot = tabletry(tab, key);

	if ((slot >= 0) && (entries[slot].key.tag != ELF_TNIL) && (entries[slot].key.tag != ELF_TTOMB)) {

		Index idx = entries[slot].idx;
		pushvalueunsafe(S, array[idx]);

		Index i;
		for (i = 0; i < tab->ntotal; i ++) {
			if (entries[i].idx == idx) {
				entries[i].key.tag = ELF_TTOMB;
			} else if(entries[i].idx > idx) {
				ASSERT(entries[i].idx > 0);
				entries[i].idx -= 1;
			}
		}

		memmove(array + idx, array + idx + 1, (darr_l(array) - idx - 1) * sizeof(V));

		ARRAY_SET_MIN(array, darr_l(array) - 1);
	} else {
		elf_pushnil(S);
	}
	return 1;
}


static inline void tablealias(Tab tab, V key, V alias) {
	Index slot = tabletry(tab,key);
	if (slot >= 0) {
		if (!isdead(tab->entries[slot].key)) {
			Index aliasslot = tabletry(tab, alias);
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
	Tab tab = loadtable(S, 0);
	tablealias(tab, loadvalue(S, 1), loadvalue(S, 2));
	return 0;
}


// @doc
// :find_aliases(key) -> table
// finds all the aliases of the given 'key' including the
// key itself
ELF_FUNCTION(l_find_aliases) {
	Tab tab = loadtable(S, 0);
	V   key = loadvalue(S, 1);

	Tab res = pushtable(S);

	if (!visnil(key)) {

		Index i=tabletry(tab, key);

		if (i > 0) {

			TEntry entry=tab->entries[i];

			if (!isdead(entry.key)) {

				for (i = 0; i < tab->ntotal; ++i) {
					/* we also include ourselves */

					TEntry alias=tab->entries[i];

					if (isdead(alias.key)) continue;
					if (alias.idx != entry.idx) continue;

					arrayadd(S, tab, alias.key);
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
	Tab tab = loadtable(S, 0);

	elf_pushtab(S);

	Index i;
	for (i = 0; i < tab->ntotal; i ++) {
		TEntry entry = tab->slots[i];
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

	Tab sum = pushtable(S);

	for (int i = 0; i < nargs; ++ i) {
		Tab merger = loadtable(S, i);
		for (slot = 0; slot < merger->ntotal; ++ slot) {
			TEntry entry = merger->entries[slot];

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
		elf_error(S, NO_BYTE, "argument is nil");
	}

	pushtable(S);

	Index i;
	for (i = 0; i < tab->ntotal; ++i) {

		TEntry entry = tab->entries[i];

		if (isdead(entry.key)) continue;
		if (tablecontains(sub, entry.key)) continue;

		pushvalueunsafe(S, entry.key);
		pushvalueunsafe(S, tab->array[entry.idx]);
		elf_setfield(S);
	}

	return 1;
}



ELF_FUNCTION(l_array_length) {
	Tab tab = loadtable(S, 0);
	pushint(S, darr_l(tab->array));
	return 1;
}



static Index loadindex(elf_State *S, int stk, Tab tab) {
	Index index = loadint(S, stk);
	if (index < 0) index += darr_l(tab->array);

	if (index < 0 || index >= darr_l(tab->array)) {
		elf_errorf(S, -1, "'%lli': index out of bounds", index);
	}
	return index;
}



ELF_FUNCTION(l_array_index) {
	Tab tab = loadtable(S, 0);
	Index idx = loadindex(S, 1, tab);
	pushvalueunsafe(S, tab->array[idx]);
	return 1;
}



ELF_FUNCTION(l_array_add) {
	Tab tab = loadtable(S, 0);

	V v = loadvalue(S, 1);
	Index index = arrayadd(S, tab, v);
	pushint(S, index);

	for (int i = 2; i < nargs; i++) {
		v = loadvalue(S, i);
		index = arrayadd(S, tab, v);
		pushint(S, index);
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
	Index x   = loadindex(S, 1, tab);
	Index y   = loadindex(S, 2, tab);

	if (x < 0) x += darr_l(tab->array);

	V temp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = temp;
	return 0;
}


ELF_FUNCTION(l_array_pop) {
	Tab tab = loadtable(S, 0);

	Dynamic_Array *darr = d_array_raw(tab->array);
	pushvalueunsafe(S, tab->array[-- darr->min]);
	return 1;
}


ELF_FUNCTION(l_array_merge) {

	Tab tab = loadtable(S, 0);
	Tab add = loadtable(S, 1);

	Tab sum = pushtable(S);

	Index i;
	for (i=0;i<darr_l(tab->array);++i) {
		darr_add(sum->array, tab->array[i]);
	}
	for (i=0;i<darr_l(add->array);++i) {
		darr_add(sum->array, tab->array[i]);
	}
	return 1;
}



ELF_FUNCTION(l_array_clone) {
	Tab tab = loadtable(S, 0);

	elf_pushtab(S);

	elf_Index i;
	for (i=0;i<darr_l(tab->array);++i)
	{
		pushvalueunsafe(S, tab->array[i]);
		elf_arrayadd(S);
	}
	return 1;
}

ELF_FUNCTION(l_array_reverse) {

	Tab tab = loadtable(S, 0);
	elf_Index n = darr_l(tab->array);

	V *array = tab->array;

	elf_Index i;
	for (i = 0; i < n >> 1; i += 1) {
		V value = array[i];
		array[i] = array[n-1-i];
		array[n-1-i] = value;
	}
	return 0;
}


ELF_FUNCTION(l_array_slice) {
	Tab tab = loadtable(S, 0);

	Index x = 0;
	Index y = darr_l(tab->array);
	if (nargs >= 2) x = loadint(S, 0);
	if (nargs >= 3) y = loadint(S, 1);

	Tab slice = pushtable(S);
	while (x < y) {
		darr_add(slice->array, tab->array[x ++]);
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
	{"repl"         , l_array_repl         },
	{"reverse"      , l_array_reverse      },
	{"merge_array"  , l_array_merge        },
	{"clone_array"  , l_array_clone        },
	{"slice"        , l_array_slice        },
};