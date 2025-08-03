//
// See Copyright Notice In elf.h
//

/* todo: these functions are to be refactored,
a bunch of them are rather useless and or misnamed */


int table_lib_get_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table_raw(R,POBJ(tab)->meta);
	return 1;
}


int table_lib_set_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table_raw(R,POBJ(tab)->meta);
	POBJ(tab)->meta = elf_get_table(R,0);
	return 1;
}

ELF_FUNCTION(table_lib_contains) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_Value key = elf_get_arg(S, 0);
	elf_push_int(S, slotiskey(tab, elf_table_try(tab, key)));
	return 1;
}


ELF_FUNCTION(table_lib_get_collisions) {
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_push_int(S,tab->ndebug);
	return 1;
}

int table_lib_itemize(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *result = elf_new_table(R);
	FOR_ARRAY(i,tab->array) {
		elf_array_add_raw(result,tab->array[i]);
	}
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		if (elf_get_argtag(R,0)==elf_tag_Table) {
			elf_Table *that = elf_get_table(R,0);
			FOR_ARRAY(j,that->array) {
				elf_array_add_raw(result,that->array[j]);
			}
		} else {
			elf_array_add_raw(result,elf_get_arg(R,i));
		}
	}
	return 1;
}
//
// table:delete()
// The following conditions are met:
// Only one array item is to be removed, but there's
// no limit as to how many keys can be removed.
// That is because multiple keys can have the same
// array index.
// The array part must remain in the same order,
// keys pointing to the target index must be
// removed.
// All other keys must point to their previous values.
int elf_lib_table_delete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value key = elf_get_arg(R,0);
	elf_Table_Entry *slots = tab->slots;
	elf_Value *array = tab->array;
	int slot = elf_table_try(tab,key);
	if ((slot >= 0) && (slots[slot].key.tag != elf_tag_Nil) && (slots[slot].key.tag != elf_tag_tomb)) {
		int idx = slots[slot].idx;
		elf_push_value_raw(R,array[idx]);
		for(int i=0; i<tab->ntotal; i++){
			if(slots[i].idx==idx){
				slots[i].key.tag = elf_tag_tomb;
			}else if(slots[i].idx>idx){
				ASSERT(slots[i].idx>0);
				slots[i].idx -= 1;
			}
		}
		memmove(array+idx,array+idx+1,(ARRAY_LENGTH(array)-idx-1)*sizeof(elf_Value));
		ARRAY_SET_MIN(array,ARRAY_LENGTH(array)-1);
	}else{
		elf_push_nil(R);
	}
	return 1;
}

#if 0
int table_lib_xdelete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);
	if (len != 0) {
		if (IS_OBJ_TAG(elf_get_argtag(R,0))) {
			elf_Object *object = elf_get_object_arg_raw(R,0);
			/* todo: Speed */
			elf_Value *item = 0;
			elf_Int idx;
			for ( idx = 0; idx < len; idx += 1 ) {
				if (tab->array[idx].x_obj == object) {
					item = &tab->array[idx];
					break;
				}
			}
			if (item == 0) {
				/* todo: maybe not crash here */
				elf_error(R,NO_BYTE,"item does not belong");
			}
			ASSERT((item - tab->array) == idx);
			PUSHV(R,*item);
			elf_Int min = ARRAY_POP(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {
			elf_Int idx = elf_get_intarg(R,0);
			if ((idx %= len) < 0) idx += len;

			elf_Int min = ARRAY_POP(tab->array);
			PUSHV(R,tab->array[idx]);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		}
	} else elf_push_nil(R);
	return 1;
}


int table_lib_xremove(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);

	if (len != 0) {
		if (IS_OBJ_TAG(elf_get_argtag(R,0))) {
			elf_Object *object = elf_get_object_arg_raw(R,0);
			/* todo: lookup can be removed if tag came
			after the data instead so that obj addr was
			the same as value addr! Otherwise this is expensive!  */
			elf_Value *item = 0;
			elf_Int idx;
			for (idx=0;idx<len;++idx) {
				if (tab->array[idx].x_obj == object) {
					item = &tab->array[idx];
					break;
				}
			}
			ASSERT((item - tab->array) == idx);
			// elf_Int idx = item - tab->array;
			// if (item < tab->array || item > tab->array + len - 1) {
			// 	elf_error(R,NO_BYTE,"item does not belong");
			// }
			if (item == 0) {
				elf_error(R,NO_BYTE,"item does not belong");
			}
			PUSHV(R,*item);
			elf_Int min = ARRAY_POP(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {

			elf_Int idx = elf_get_intarg(R,0);
			if ((idx %= len) < 0) idx += len;

			elf_Int min = ARRAY_POP(tab->array);
			PUSHV(R,tab->array[idx]);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		}
	} else elf_push_nil(R);
	return 1;
}
#endif


// (key of any, alias of any) -> none, adds a new entry to the table (alias) that points to where (key) points
ELF_FUNCTION(table_lib_alias) {
	elf_Table *tab = (elf_Table *) elf_get_this(S);
	elf_table_alias(tab, elf_get_arg(S,0), elf_get_arg(S,1));
	return 0;
}

// get a list of entries mapped to the same key
ELF_FUNCTION(table_lib_find_aliases) {
	elf_Table *tab = (elf_Table *) elf_get_this(S);

	elf_Value key = elf_get_arg(S,0);
	elf_Table *aliases = elf_new_table(S);

	if (key.tag != elf_tag_Nil) {

		elf_Int slot = elf_table_try(tab,key);

		if (slotiskey(tab,slot)) {
			elf_Table_Entry entry = tab->slots[slot];
			for (elf_i64 i = 0; i < tab->ntotal; ++i) {
				/* we also include ourselves */
				elf_Table_Entry it = tab->slots[i];
				if (it.idx != entry.idx) continue;
				if (it.key.tag == elf_tag_Nil) continue;
				elf_array_add_raw(aliases,it.key);
			}
		}
	}
	return 1;
}


#if 0
int table_lib_bubble_sort(elf_State *R) {
	elf_check_num_args(R,":bubblesort",1,"comparator function");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value *arr = tab->array;
	elf_Closure *cls = elf_get_closure(R,0);
	bool sorted = 0;
	do {
		sorted = 1;
		elf_Int i;
		for (i=0;i<ARRAY_LENGTH(arr)-1;++i) {
			elf_Value *top = GET_TOP(R);
			elf_push_closure_raw(R,cls);
			PUSHV(R,arr[i+0]);
			PUSHV(R,arr[i+1]);
			NO_CODE;
			int r = elf_call(R,2,1);
			ASSERT(r == 1);
			// if (elf_get_intarg(R,base))
			{
				elf_Value tmp = arr[i+0];
				arr[i+0] = arr[i+1];
				arr[i+1] = tmp;
				sorted = 0;
			}
			SET_TOP(R,top);
		}
	} while(sorted != 1);
	return 0;
}


int table_lib_foreach(elf_State *R) {
	NO_CODE;
	// ASSERT(R->frame->nx == 1);
	// elf_Table *tab = (elf_Table *) elf_get_this(R);
	// elf_Closure *cls = elf_get_closure(R,0);
	// elf_StackId k = elf_local_alloc(R,1);
	// elf_StackId v = elf_local_alloc(R,1);
	// elf_Int i;
	// for (i=0;i<tab->ntotal;++i) {
	// 	elf_Table_Entry it = tab->slots[i];
	// 	if (it.key.tag == elf_tag_Nil) continue;
	// 	R->stack[k] = it.k;
	// 	R->stack[v] = tab->array[it.idx];
	// 	/* todo: should yield boolean to signal whether to
	// 	stop or not */
	// 	int ny = elf_call(R,tab,0,0,2,0);
	// 	if (ny != 0) if (elf_get_intarg(R,0) != 1) break;
	// }
	return 0;
}
#endif

/* todo: move to table.c
this function only clones keyed values... */
elf_Table *elf_clone_table(elf_State *S, elf_Table *tab) {
	elf_Table *clone = elf_alloc_table(S);
	elf_Int i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elf_Table_Entry it = tab->slots[i];
		if (it.key.tag == elf_tag_Nil) continue;
		elf_table_set_raw(clone,it.key,tab->array[it.idx]);
	}
	return clone;
}


ELF_FUNCTION(lib_table_keys) {
	elf_Table *tab = (elf_Table *) elf_get_this(S);
	elf_Table *array = elf_new_table(S);
	for (elf_i64 i = 0; i < tab->ntotal; i++) {
		elf_Table_Entry entry = tab->slots[i];
		if (entry.key.tag == elf_tag_Nil || entry.key.tag == elf_tag_tomb) continue;
		elf_array_add_raw(array,entry.key);
	}
	elf_push_table_raw(S,array);
	return 1;
}

ELF_FUNCTION(lib_table_array) {
	elf_check_num_args(S,":array",0,"the table to get a copy of as an array");
	elf_Table *tab = (elf_Table *) elf_get_this(S);
	elf_Table *array = elf_new_table(S);
	elf_Int i;
	for (i = 0; i < ARRAY_LENGTH(tab->array); ++i) {
		elf_array_add_raw(array,tab->array[i]);
	}
	return 1;
}


int table_lib_clone(elf_State *R) {
	elf_check_num_args(R,":clone",0,"the table to clone");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table_raw(R,elf_clone_table(R,tab));
	return 1;
}

int elf_lib_table_merge(elf_State *R) {
	elf_check_num_args(R,":merge",1,"the tables to merge into a new table (keys only), if no arguments are passed in, this acts like a clone");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sum = elf_new_table(R);
	elf_table_merge(sum,tab);
	for ( int i = 0; i < elf_get_num_args(R); ++ i ) {
		elf_table_merge(sum,elf_get_table(R,i));
	}
	sum->obj.meta = tab->obj.meta;
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
		if (elf_table_contains(sub,it.key)) continue;
		elf_table_set_raw(dif,it.key,tab->array[it.idx]);
	}
	elf_push_table_raw(R,dif);
	return 1;
}


int lib_table_set_length(elf_State *R) {
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_i32 length = elf_get_intarg(R,0);
	ARRAY_SET_MIN(tab->array,length);
	return 0;
}


int lib_table_get_length(elf_State *R) {
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_push_int(R,ARRAY_LENGTH(tab->array));
	return 1;
}

int elf_array_lib_get(elf_State *R) {
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_i64 len = ARRAY_LENGTH(tab->array);
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

			elf_i64 idx = elf_get_intarg(R,i);
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
		elf_array_add_raw(tab,elf_get_arg(R,i));
	}
	return 0;
}

int elf_lib_array_set(elf_State *R) {
	elf_check_num_args(R,":array_set",2,"the value, and the index where to place the value");

	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_Value value = elf_get_arg(R,0);

	elf_i64 len=ARRAY_LENGTH(tab->array);
	elf_i64 idx=elf_get_intarg(R,1);
	if ((idx%=len)<0)idx+=len;
	tab->array[idx] = value;
	return 0;
}


int elf_lib_array_swap(elf_State *R) {
	elf_check_num_args(R,":array_swap",2,"the two indexes to swap");
	elf_Table * tab = (elf_Table *) elf_get_this(R);
	elf_i64 x = elf_get_intarg(R,0);
	elf_i64 y = elf_get_intarg(R,1);
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
	elf_i64 i;
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
	elf_i64 i;
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
		elf_array_add_raw(slice,tab->array[x ++]);
	}
	return 1;
}




elf_Binding table_metafuncs[] = {
		// {"foreach",table_lib_foreach},
		// {"xrem",table_lib_xremove},
		// {"xdelete",table_lib_xdelete},
		// {"bubblesort",table_lib_bubble_sort},
	{"get_meta"     , table_lib_get_meta         },
	{"set_meta"     , table_lib_set_meta         },
	{"length"       , lib_table_get_length       },
	{"set_length"   , lib_table_set_length       },
	{"tally"        , lib_table_get_length       },
	{"delete"       , elf_lib_table_delete       },
	{"haskey"       , table_lib_contains         },
	{"collisions"   , table_lib_get_collisions   },
	{"add"          , elf_lib_array_add          },
	{"itemize"      , table_lib_itemize          },
	{"idx"          , elf_array_lib_get          },
	{"array"        , lib_table_array            },
	{"keys"         , lib_table_keys             },
	{"find_aliases" , table_lib_find_aliases     },
	{"alias"        , table_lib_alias            },
	{"merge"        , elf_lib_table_merge        },
	{"merge_array"  , lib_array_merge            },
	{"reverse"      , elf_lib_array_reverse      },
	{"clone"        , table_lib_clone            },
	{"clone_array"  , elf_array_lib_clone        },
	{"slice"        , elf_lib_array_slice        },
	{"xset"         , elf_lib_array_set          },
	{"swap"         , elf_lib_array_swap         },
	{"diff"         , table_lib_diff             },
};