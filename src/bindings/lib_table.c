//
// See Copyright Notice In elf.h
//

/* todo: these functions are to be refactored,
a bunch of them are rather useless and or misnamed */


ELF_FUNCTION(table_lib_get_meta) {
	elf_Table *tab = elf_get_table(S, -1);
	elf_push_table_raw(S, POBJ(tab)->meta);
	return 1;
}


ELF_FUNCTION(table_lib_set_meta) {
	elf_Table *tab = elf_get_table(S, -1);
	elf_push_table_raw(S, POBJ(tab)->meta);
	POBJ(tab)->meta = elf_get_table(S,0);
	return 1;
}


static inline bool tablecontains(elf_Table *tab, elf_Value key) {
	elf_IndexInt slot = elf_table_try_(tab, key);
	return slot >= 0 && !IS_DEAD_TAG(tab->entries[slot].key.tag);
}

ELF_FUNCTION(table_lib_contains) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Table *tab = elf_get_table(S, -1);
	elf_Value key = elf_get_arg(S, 0);
	elf_push_int(S, tablecontains(tab, key));
	return 1;
}


ELF_FUNCTION(table_lib_get_collisions) {
	elf_Table *tab = elf_get_table(S, -1);
	elf_push_int(S, tab->ndebug);
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
	ASSERT(elf_get_num_args(S) >= 1);

	elf_Table *tab = elf_get_table(S, -1);
	elf_Value key = elf_get_arg(S, 0);

	elf_Table_Entry *entries = tab->entries;
	elf_Value *array = tab->array;

	elf_IndexInt slot = elf_table_try_(tab, key);

	if ((slot >= 0) && (entries[slot].key.tag != elf_tag_Nil) && (entries[slot].key.tag != elf_tag_Tomb)) {

		elf_IndexInt idx = entries[slot].idx;
		elf_push_value_raw(S, array[idx]);

		elf_IndexInt i;
		for (i = 0; i < tab->ntotal; i ++) {
			if (entries[i].idx == idx) {
				entries[i].key.tag = elf_tag_Tomb;
			} else if(entries[i].idx > idx) {
				ASSERT(entries[i].idx > 0);
				entries[i].idx -= 1;
			}
		}

		memmove(array + idx, array + idx + 1, (ARRAY_LENGTH(array) - idx - 1) * sizeof(elf_Value));

		ARRAY_SET_MIN(array, ARRAY_LENGTH(array) - 1);
	} else {
		elf_push_nil(S);
	}
	return 1;
}


static inline void aliastable(elf_Table *tab, elf_Value key, elf_Value alias) {
	elf_Int slot = elf_table_try_(tab,key);
	if (slot >= 0) {
		if (!IS_DEAD_TAG(tab->entries[slot].key.tag)) {
			elf_Int aliasslot = elf_table_try_(tab, alias);
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
	elf_Table *tab = elf_get_table(S, -1);
	aliastable(tab, elf_get_arg(S,0), elf_get_arg(S,1));
	return 0;
}

// @doc
// :find_aliases(key) -> table
// finds all the aliases of the given 'key' including the
// key itself
ELF_FUNCTION(l_find_aliases) {
	elf_Table *tab = elf_get_table(S, -1);

	elf_Value key = elf_get_arg(S,0);
	elf_Table *aliases = elf_new_table(S);

	elf_IndexInt i;
	elf_Table_Entry entry;

	if (key.tag != elf_tag_Nil) {

		i = elf_table_try_(tab, key);

		if (i > 0) {

			entry = tab->slots[i];

			if (!IS_DEAD_TAG(entry.key.tag)) {

				for (i = 0; i < tab->ntotal; ++i) {
					/* we also include ourselves */

					elf_Table_Entry alias = tab->slots[i];

					if (IS_DEAD_TAG(alias.key.tag)) continue;
					if (alias.idx != entry.idx) continue;

					elf_array_add_k(aliases, alias.key);
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
	elf_Table *tab = elf_get_table(S, -1);

	elf_IndexInt i;
	for (i = 0; i < tab->ntotal; i ++) {
		elf_Table_Entry entry = tab->slots[i];
		// omit non-keys
		if ((entry.key.tag == elf_tag_Nil)
		||  (entry.key.tag == elf_tag_Tomb))
		{
			continue;
		}
		elf_array_add_k(tab, entry.key);
	}
	return 1;
}


//
// @doc :table_merge
// merges all the tables passed in, keys are not overwritten
// * - this works per entry, so loose values aren't kept
// * - if no additional tables are passed in then this is equivalent
// * to creating a clone of the table, but additionally, loose values are lost.
ELF_FUNCTION(l_table_merge) {
	elf_Table *sum = elf_new_table(S);

	elf_IndexInt slot;
	elf_Table_Entry entry;

	for (int i = -1; i < elf_get_num_args(S); ++ i ) {
		elf_Table *merger = elf_get_table(S, i);
		for (slot = 0; slot < merger->ntotal; ++ slot) {
			entry = merger->slots[slot];
			if ((entry.key.tag == elf_tag_Nil)
			||  (entry.key.tag == elf_tag_Tomb))
			{
				continue;
			}
			elf_raw_table_set(sum, entry.key, merger->array[entry.idx]);
		}
	}
	sum->obj.meta = elf_get_table(S, -1)->obj.meta;
	return 1;
}

int table_lib_diff(elf_State *R) {
	elf_check_num_args(R,":diff",1,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sub = elf_get_table(R,0);
	if (sub == 0) {
		elf_error(R,NO_BYTE,"argument is nil");
	}
	elf_Table *dif = elf_alloc_table(R);
	elf_Int i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elf_Table_Entry it = tab->slots[i];
		if (it.key.tag == elf_tag_Nil) continue;
		if (tablecontains(sub,it.key)) continue;
		elf_raw_table_set(dif,it.key,tab->array[it.idx]);
	}
	elf_push_table_raw(R,dif);
	return 1;
}


ELF_FUNCTION(l_array_length) {
	elf_Table *tab = elf_get_table(S, -1);
	elf_push_int(S, ARRAY_LENGTH(tab->array));
	return 1;
}

int elf_array_lib_get(elf_State *R) {
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_IndexInt len = ARRAY_LENGTH(tab->array);
	elf_Value value = VALUE_NIL();
	if (len != 0) {
		for (int i = 0; i < elf_get_num_args(R); ++ i) {
			if (i != 0) {
				if (value.tag == elf_tag_Nil) {
					elf_error(R,NO_BYTE,"nil object");
				}
				/* todo: please do much better error reporting
				here, this can be hard to figure out */
				if (value.tag != elf_tag_Table) {
					elf_error(R,NO_BYTE,"not a table");
				}
				if (tab == 0) {
					elf_error(R,NO_BYTE,"nil object");
				}
			}

			elf_IndexInt idx = elf_get_intarg(R,i);
			if ((idx %= len) < 0) idx += len;

			value = tab->array[idx];
			tab = value.x_tab;
		}
	}
	PUSHV(R,value);
	return 1;
}

int elf_lib_array_add(elf_State *R) {
	elf_Table * tab=(elf_Table *)elf_get_this(R);
	int i;
	for (i=0;i<elf_get_num_args(R);i++) {
		elf_array_add_k(tab,elf_get_arg(R,i));
	}
	return 0;
}

int elf_lib_array_set(elf_State *R) {
	elf_check_num_args(R,":array_set",2,"the value, and the index where to place the value");

	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_Value value = elf_get_arg(R,0);

	elf_IndexInt len=ARRAY_LENGTH(tab->array);
	elf_IndexInt idx=elf_get_intarg(R,1);
	if ((idx%=len)<0)idx+=len;
	tab->array[idx] = value;
	return 0;
}


int elf_lib_array_swap(elf_State *R) {
	elf_check_num_args(R,":array_swap",2,"the two indexes to swap");
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_IndexInt x = elf_get_intarg(R,0);
	elf_IndexInt y = elf_get_intarg(R,1);
	elf_Value temp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = temp;
	return 0;
}

int lib_array_merge(elf_State *R) {
	elf_check_num_args(R,":array_merge",1
	, "takes: the array to merge, all values of the arrays"
	" are added into a new array");
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_Table * add = elf_get_table(R,0);
	elf_Table * res = elf_new_table(R);
	elf_IndexInt i;
	for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
		ARRAY_ADD(res->array,tab->array[i]);
	}
	for (i=0;i<ARRAY_LENGTH(add->array);++i) {
		ARRAY_ADD(res->array,add->array[i]);
	}
	return 1;
}

int elf_array_lib_clone(elf_State *R) {
	elf_check_num_args(R,":xclone",0,"");
	elf_Table * tab = (elf_Table * ) elf_get_this(R);
	elf_Table * clone = elf_alloc_table(R);
	elf_IndexInt i;
	for ( i = 0; i < ARRAY_LENGTH(tab->array); i += 1 ) {
		ARRAY_ADD(clone->array,tab->array[i]);
	}
	elf_push_table_raw(R,clone);
	return 1;
}

int elf_lib_array_reverse(elf_State *R) {
	elf_check_num_args(R,":reverse",0,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int n = ARRAY_LENGTH(tab->array);
	elf_Int i;
	elf_Value *array = tab->array;
	for (i = 0; i < n >> 1; i += 1) {
		elf_Value value = array[i];
		array[i] = array[n-1-i];
		array[n-1-i] = value;
	}
	return 0;
}


int elf_lib_array_slice(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int x = 0;
	elf_Int y = ARRAY_LENGTH(tab->array);
	if (elf_get_num_args(R) >= 1) x = elf_get_intarg(R,0);
	if (elf_get_num_args(R) >= 2) y = elf_get_intarg(R,1);
	elf_Table *slice = elf_new_table(R);
	while (x < y) {
		elf_array_add_k(slice,tab->array[x ++]);
	}
	return 1;
}




elf_Binding table_metafuncs[] = {
	// object
	{"get_meta"     , table_lib_get_meta         },
	{"set_meta"     , table_lib_set_meta         },

	// table
	{"haskey"       , table_lib_contains         },
	{"collisions"   , table_lib_get_collisions   },
	{"get_keys"     , l_table_get_keys           },
	{"find_aliases" , l_find_aliases             },
	{"alias"        , l_table_alias              },
	{"merge"        , l_table_merge              },
	{"diff"         , table_lib_diff             },

	// aware of both
	{"delete"       , l_table_delete             },

	// array part
	{"length"       , l_array_length       },
	{"add"          , elf_lib_array_add          },
	{"idx"          , elf_array_lib_get          },
	{"replace"      , elf_lib_array_set          },
	{"reverse"      , elf_lib_array_reverse      },
	// {"resize"       , elf_lib_array_reverse      },
	{"merge_array"  , lib_array_merge            },
	{"clone_array"  , elf_array_lib_clone        },
	{"slice"        , elf_lib_array_slice        },
	{"swap"         , elf_lib_array_swap         },
};