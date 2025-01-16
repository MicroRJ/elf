/*
** See Copyright Notice In elf.h
** lib_table.c
*/


/* todo: to be revised */


static int table_lib_get_meta(elf_State *);
static int table_lib_set_meta(elf_State *);
static int elf_lib_array_add(elf_State *);
// static int table_lib_xadd(elf_State *);
// static int table_lib_xremove(elf_State *);
// static int table_lib_xdelete(elf_State *);
static int elf_array_lib_get(elf_State *);
// static int table_lib_tally(elf_State *);
static int elf_array_lib_tally(elf_State *);
static int elf_lib_table_delete(elf_State *);
static int table_lib_itemize(elf_State *);
// static int table_lib_inject(elf_State *);
static int table_lib_alias(elf_State *);
static int table_lib_contains(elf_State *);
// static int table_lib_foreach(elf_State *);
// static int table_lib_bubble_sort(elf_State *);
static int table_lib_get_collisions(elf_State *);
static int table_lib_find_aliases(elf_State *);
static int lib_table_array(elf_State *);
static int lib_table_keys(elf_State *);
static int elf_lib_array_set(elf_State *);
static int elf_lib_table_merge(elf_State *);
static int lib_array_merge(elf_State *);
static int table_lib_diff(elf_State *);
static int elf_array_lib_clone(elf_State *);
static int elf_lib_array_reverse(elf_State *);
static int table_lib_clone(elf_State *);
static int elf_lib_array_slice(elf_State *);
static int elf_lib_array_swap(elf_State *);


elf_Table *new_table_lib(elf_State *R) {
	elf_CBinding lib[] = {
		{"get_meta",table_lib_get_meta},
		{"set_meta",table_lib_set_meta},
		{"length",elf_array_lib_tally},
		{"tally",elf_array_lib_tally},
		{"delete",elf_lib_table_delete},
		{"haskey",table_lib_contains},
		// {"foreach",table_lib_foreach},
		{"collisions",table_lib_get_collisions},
		{"add",elf_lib_array_add},
		{"itemize",table_lib_itemize},
		{"idx",elf_array_lib_get},
		{"array",lib_table_array},
		{"keys",lib_table_keys},
		// {"xrem",table_lib_xremove},
		// {"xdelete",table_lib_xdelete},
		// {"bubblesort",table_lib_bubble_sort},
		{"find_aliases",table_lib_find_aliases},
		{"alias",table_lib_alias},
		{"merge",elf_lib_table_merge},
		{"merge_array",lib_array_merge},
		{"reverse",elf_lib_array_reverse},
		{"clone",table_lib_clone},
		{"clone_array",elf_array_lib_clone},
		{"slice",elf_lib_array_slice},
		{"xset",elf_lib_array_set},
		{"swap",elf_lib_array_swap},
		{"diff",table_lib_diff},
	};
	/* todo: these functions are to be refactored,
	a bunch of them are rather useless and or misnamed */
	elf_Table *tab = elf_new_table(R);
	elf_tsetx_bindings(R,tab,lib,COUNTOF(lib));
	return tab;
}



int table_lib_get_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table(R,POBJ(tab)->meta);
	return 1;
}


int table_lib_set_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table(R,POBJ(tab)->meta);
	POBJ(tab)->meta = elf_get_table(R,0);
	return 1;
}

int table_lib_contains(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_Value key = elf_get_arg(S,0);
	elf_push_integer(S,slotiskey(tab,elf_table_try(tab,key)));
	return 1;
}


int table_lib_get_collisions(elf_State *S) {
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_push_integer(S,tab->ndebug);
	return 1;
}

int table_lib_itemize(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *result = elf_new_table(R);
	FOR_ARRAY(i,tab->array) {
		elf_array_add(result,tab->array[i]);
	}
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		if (elf_get_tag(R,0)==elf_tag_tab) {
			elf_Table *that = elf_get_table(R,0);
			FOR_ARRAY(j,that->array) {
				elf_array_add(result,that->array[j]);
			}
		} else {
			elf_array_add(result,elf_get_arg(R,i));
		}
	}
	return 1;
}

// todo: this is buggy!
int elf_lib_table_delete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab;
	elf_Value key;
	elf_Entry *slots;
	elf_Value *array;
	elf_i64 slot;

	tab = (elf_Table *) elf_get_this(R);
	key = elf_get_arg(R,0);
	slots = tab->slots;
	array = tab->array;
	slot = elf_table_try(tab,key);
	if ((slot < 0) || (slots[slot].key.tag == elf_tag_nil)) {
		elf_fail(R,NO_BYTE,elf_tpf("invalid key, %s, slot is %i", tag2s[key.tag], slot));
		goto _err;
	}
	elf_i64 idx = slots[slot].idx;
	slots[slot].key = (elf_Value){elf_tag_tomb};
	slots[slot].idx = 0;
	elf_i64 len = ARRAY_LENGTH(array);
	if ((idx < 0) || (idx > len-1)) {
		elf_fail(R,NO_BYTE,elf_tpf("key is invalid, points to invalid index %lli, there are %lli item(s)",idx,len));
		goto _err;
	}
	PUSHV(R,array[idx]);
	array[idx].tag = elf_tag_nil;

	// elf_i64 min = ARRAY_POP(array);
	// if (idx != min) {
	// array[idx] = array[min];
	// elf_i64 i;
	// for (i=0;i<tab->ntotal;++i) {
	// 	if (slots[i].key.tag != elf_tag_nil && slots[i].idx == min) {
	// 		slots[i].idx = idx;
	// 	}
	// }
	// }

	return 1;
	_err: elf_push_nil(R);
	return 1;
}

#if 0
int table_lib_xdelete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);
	if (len != 0) {
		if (ISOBJT(elf_get_tag(R,0))) {
			elf_Object *object = elf_get_obj(R,0);
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
				elf_fail(R,NO_BYTE,"item does not belong");
			}
			ASSERT((item - tab->array) == idx);
			PUSHV(R,*item);
			elf_Int min = ARRAY_POP(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {
			elf_Int idx = elf_get_int(R,0);
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
		if (ISOBJT(elf_get_tag(R,0))) {
			elf_Object *object = elf_get_obj(R,0);
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
			// 	elf_fail(R,NO_BYTE,"item does not belong");
			// }
			if (item == 0) {
				elf_fail(R,NO_BYTE,"item does not belong");
			}
			PUSHV(R,*item);
			elf_Int min = ARRAY_POP(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {

			elf_Int idx = elf_get_int(R,0);
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

int table_lib_alias(elf_State *R) {
	elf_check_args(R,":alias",2,"(key of any, alias of any) -> none, adds a new entry to the table (alias) that points to where (key) points");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_table_alias(R,tab,elf_get_arg(R,0),elf_get_arg(R,1));
	return 0;
}


int table_lib_find_aliases(elf_State *R) {
	elf_check_args(R,":find_aliases",1,"the key to find aliases for");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value key = elf_get_arg(R,0);
	elf_Table *list = elf_new_table(R);
	if (key.tag != elf_tag_nil) {
		elf_Int slot = elf_table_try(tab,key);
		if (slotiskey(tab,slot)) {
			elf_Entry entry = tab->slots[slot];
			elf_Int i;
			for (i=0;i<tab->ntotal;++i) {
				/* we also include ourselves */
				elf_Entry it = tab->slots[i];
				if (it.idx != entry.idx) continue;
				if (it.key.tag == elf_tag_nil) continue;
				elf_array_add(list,it.key);
			}
		}
	}
	return 1;
}


#if 0
int table_lib_bubble_sort(elf_State *R) {
	elf_check_args(R,":bubblesort",1,"comparator function");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value *arr = tab->array;
	elf_Closure *cls = elf_get_cls(R,0);
	elf_Bool sorted = 0;
	do {
		sorted = 1;
		elf_Int i;
		for (i=0;i<ARRAY_LENGTH(arr)-1;++i) {
			elf_Value *top = GET_TOP(R);
			elf_push_closure(R,cls);
			PUSHV(R,arr[i+0]);
			PUSHV(R,arr[i+1]);
			NO_CODE;
			int r = elf_call(R,2,1);
			ASSERT(r == 1);
			// if (elf_get_int(R,base))
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
	// elf_Closure *cls = elf_get_cls(R,0);
	// elf_StackId k = elf_local_alloc(R,1);
	// elf_StackId v = elf_local_alloc(R,1);
	// elf_Int i;
	// for (i=0;i<tab->ntotal;++i) {
	// 	elf_Entry it = tab->slots[i];
	// 	if (it.key.tag == elf_tag_nil) continue;
	// 	R->stack[k] = it.k;
	// 	R->stack[v] = tab->array[it.idx];
	// 	/* todo: should yield boolean to signal whether to
	// 	stop or not */
	// 	int ny = elf_call(R,tab,0,0,2,0);
	// 	if (ny != 0) if (elf_get_int(R,0) != 1) break;
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
		elf_Entry it = tab->slots[i];
		if (it.key.tag == elf_tag_nil) continue;
		elf_table_set(clone,it.key,tab->array[it.idx]);
	}
	return clone;
}


int lib_table_keys(elf_State *S) {
	elf_Table *tab = (elf_Table *) elf_get_this(S);
	elf_Table *array = elf_new_table(S);
	for (elf_i64 i = 0; i < tab->ntotal; i++) {
		elf_Entry entry = tab->slots[i];
		if (entry.key.tag == elf_tag_nil || entry.key.tag == elf_tag_tomb) continue;
		elf_array_add(array,entry.key);
	}
	elf_push_table(S,array);
	return 1;
}

int lib_table_array(elf_State *S) {
	elf_check_args(S,":array",0,"the table to get a copy of as an array");
	elf_Table *tab = (elf_Table *) elf_get_this(S);
	elf_Table *array = elf_new_table(S);
	elf_Int i;
	for (i = 0; i < ARRAY_LENGTH(tab->array); ++i) {
		elf_array_add(array,tab->array[i]);
	}
	return 1;
}


int table_lib_clone(elf_State *R) {
	elf_check_args(R,":clone",0,"the table to clone");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_push_table(R,elf_clone_table(R,tab));
	return 1;
}

int elf_lib_table_merge(elf_State *R) {
	elf_check_args(R,":merge",1,"the tables to merge into a new table (keys only), if no arguments are passed in, this acts like a clone");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sum = elf_new_table(R);
	elf_merge_tables(sum,tab);
	for ( int i = 0; i < elf_get_num_args(R); ++ i ) {
		elf_merge_tables(sum,elf_get_table(R,i));
	}
	sum->obj.meta = tab->obj.meta;
	return 1;
}

int table_lib_diff(elf_State *R) {
	elf_check_args(R,":diff",1,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sub = elf_get_table(R,0);
	if (sub == 0) {
		elf_fail(R,NO_BYTE,"argument is nil");
	}
	elf_Table *dif = elf_alloc_table(R);
	elf_Int i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elf_Entry it = tab->slots[i];
		if (it.key.tag == elf_tag_nil) continue;
		if (elf_table_contains(sub,it.key)) continue;
		elf_table_set(dif,it.key,tab->array[it.idx]);
	}
	elf_push_table(R,dif);
	return 1;
}


