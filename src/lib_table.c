/*
** See Copyright Notice In elf.h
** lib_table.c
*/


/* todo: to be revised */


static int table_lib_get_meta(elf_State *);
static int table_lib_set_meta(elf_State *);
static int table_lib_add(elf_State *);
static int table_lib_xadd(elf_State *);
static int table_lib_xremove(elf_State *);
static int table_lib_xdelete(elf_State *);
static int table_lib_index(elf_State *);
static int table_lib_tally(elf_State *);
static int table_lib_length(elf_State *);
static int table_lib_delete(elf_State *);
static int table_lib_itemize(elf_State *);
static int table_lib_inject(elf_State *);
static int table_lib_alias(elf_State *);
static int table_lib_contains(elf_State *);
static int table_lib_foreach(elf_State *);
static int table_lib_get_collisions(elf_State *);
static int table_lib_bubble_sort(elf_State *);
static int table_lib_find_aliases(elf_State *);
static int table_lib_array(elf_State *);
static int table_lib_xset(elf_State *);
static int table_lib_merge(elf_State *);
static int table_lib_xmerge(elf_State *);
static int table_lib_diff(elf_State *);
static int table_lib_xclone(elf_State *);
static int table_lib_reverse(elf_State *);
static int table_lib_clone(elf_State *);
static int table_lib_slice(elf_State *);
static int table_lib_swap(elf_State *);


elf_Table *new_table_lib(elf_State *R) {
	elf_CBinding lib[] = {
		{"get_meta",table_lib_get_meta},
		{"set_meta",table_lib_set_meta},
		{"length",table_lib_length},
		{"tally",table_lib_tally},
		{"delete",table_lib_delete},
		{"haskey",table_lib_contains},
		{"foreach",table_lib_foreach},
		{"collisions",table_lib_get_collisions},
		{"add",table_lib_add},
		{"xadd",table_lib_xadd},
		{"itemize",table_lib_itemize},
		{"inject",table_lib_inject},
		{"idx",table_lib_index},
		{"xrem",table_lib_xremove},
		{"xdelete",table_lib_xdelete},
		{"bubblesort",table_lib_bubble_sort},
		{"fndaliases",table_lib_find_aliases},
		{"alias",table_lib_alias},
		{"merge",table_lib_merge},
		{"xmerge",table_lib_xmerge},
		{"reverse",table_lib_reverse},
		{"clone",table_lib_clone},
		{"xclone",table_lib_xclone},
		{"slice",table_lib_slice},
		{"xset",table_lib_xset},
		{"swap",table_lib_swap},
		{"diff",table_lib_diff},
	};
	/* todo: these functions are to be refactored,
	a bunch of them are rather useless and or misnamed */
	elf_Table *tab = elf_new_table(R);
	elf_tsetx_bindings(R,tab,lib,COUNTOF(lib));
	return tab;
}


int table_lib_length(elf_State *R) {
	elf_Table *tab = (elf_Table*) elf_get_this(R);
	elf_add_int(R,ARRAY_LENGTH(tab->array));
	return 1;
}


int table_lib_tally(elf_State *R) {
	elf_Table *tab = (elf_Table*) elf_get_this(R);
	elf_add_int(R,ARRAY_LENGTH(tab->array));
	return 1;
}


int table_lib_contains(elf_State *S) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_Value key = elf_get_arg(S,0);
	elf_add_int(S,slotiskey(tab,elf_table_try(tab,key)));
	return 1;
}


int table_lib_get_collisions(elf_State *S) {
	elf_Table *tab = (elf_Table*) elf_get_this(S);
	elf_add_int(S,tab->ndebug);
	return 1;
}


int table_lib_get_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_add_tab(R,POBJ(tab)->meta);
	return 1;
}


int table_lib_set_meta(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_add_tab(R,POBJ(tab)->meta);
	POBJ(tab)->meta = elf_get_tab(R,0);
	return 1;
}


/* todo: ensure that [i] == :idx(i)
also xadd should be add instead, since
names with 'x' prefix work only for
arrays */
int table_lib_xadd(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int length = ARRAY_LENGTH(tab->array);
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		elf_table_set(tab,VINT(length+i),elf_get_arg(R,i));
	}
	return 0;
}


int table_lib_add(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	int i;
	for ( i = 0; i < elf_get_num_args(R); ++i ) {
		elf_array_add(tab,elf_get_arg(R,i));
	}
	return 0;
}


int table_lib_inject(elf_State *R) {
	elf_check_args(R,":inject",1,"the table, all the fields of the table are injected onto this one");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_merge_tables(tab,elf_get_tab(R,0));
	return 0;
}


int table_lib_itemize(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *result = elf_new_table(R);
	FOR_ARRAY(i,tab->array) {
		elf_array_add(result,tab->array[i]);
	}
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		if (elf_get_tag(R,0)==elf_TAG_TAB) {
			elf_Table *that = elf_get_tab(R,0);
			FOR_ARRAY(j,that->array) {
				elf_array_add(result,that->array[j]);
			}
		} else {
			elf_array_add(result,elf_get_arg(R,i));
		}
	}
	return 1;
}


int table_lib_index(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);
	elf_Value value = VNIL();
	if (len != 0) {
		for (int i = 0; i < elf_get_num_args(R); ++ i) {
			if (i != 0) {
				if (value.tag == elf_TAG_NIL) {
					elf_fail(R,NO_BYTE,"nil object");
				}
				/* todo: please do much better error reporting
				here, this can be hard to figure out */
				if (value.tag != elf_TAG_TAB) {
					elf_fail(R,NO_BYTE,"not a table");
				}
				if (tab == 0) {
					elf_fail(R,NO_BYTE,"nil object");
				}
			}

			elf_Int idx = elf_get_int(R,i);
			if ((idx %= len) < 0) idx += len;

			value = tab->array[idx];
			tab = value.x_tab;
		}
	}
	PUSHV(R,value);
	return 1;
}


/*
** Deletes a key and its corresponding
** value from a table.
** The algorithm is pretty slow...
*/
int table_lib_delete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value key = elf_get_arg(R,0);
	elf_Entry *slots = tab->slots;
	elf_Value *array = tab->array;
	elf_Int slot = elf_table_try(tab,key);
	if ((slot < 0) || (slots[slot].key.tag == elf_TAG_NIL)) {
		elf_fail(R,NO_BYTE,"invalid key");
		goto leave_;
	}
	elf_Int idx = slots[slot].idx;
	slots[slot].key = (elf_Value){elf_TAG_NIL};
	slots[slot].idx = 0;
	elf_Int len = ARRAY_LENGTH(array);
	if ((idx < 0) || (idx > len-1)) {
		elf_fail(R,NO_BYTE,elf_tpf("key is invalid, points to invalid index %lli, there are %lli item(s)",idx,len));
		goto leave_;
	}
	PUSHV(R,array[idx]);
	elf_Int min = ARRAY_POP(array);
	// if (idx != min) {
		//NOTE: Swap the items, then iterate to
		//find references and update them...
	array[idx] = array[min];
	elf_Int i;
	for (i=0;i<tab->ntotal;++i) {
		if (slots[i].key.tag != elf_TAG_NIL && slots[i].idx == min) {
			slots[i].idx = idx;
		}
	}
	// }

	return 1;
	leave_: elf_add_nil(R);
	return 1;
}


int table_lib_xdelete(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);
	if (len != 0) {
		if (ISOBJT(elf_get_tag(R,0))) {
			elf_Node *object = elf_get_obj(R,0);
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
	} else elf_add_nil(R);
	return 1;
}


int table_lib_xremove(elf_State *R) {
	ASSERT(elf_get_num_args(R) >= 1);
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int len = ARRAY_LENGTH(tab->array);

	if (len != 0) {
		if (ISOBJT(elf_get_tag(R,0))) {
			elf_Node *object = elf_get_obj(R,0);
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
	} else elf_add_nil(R);
	return 1;
}


int table_lib_alias(elf_State *R) {
	elf_check_args(R,":alias",2,"(key of any, alias of any) -> none, adds a new entry to the table (alias) that points to where (key) points");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_table_alias(R,tab,elf_get_arg(R,0),elf_get_arg(R,1));
	return 0;
}


int table_lib_find_aliases(elf_State *R) {
	elf_check_args(R,":fndaliases",1,"the key to find aliases for");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value key = elf_get_arg(R,0);
	elf_Table *list = elf_new_table(R);
	if (key.tag != elf_TAG_NIL) {
		elf_Int slot = elf_table_try(tab,key);
		if (slotiskey(tab,slot)) {
			elf_Entry entry = tab->slots[slot];
			elf_Int i;
			for (i=0;i<tab->ntotal;++i) {
				/* we also include ourselves */
				elf_Entry it = tab->slots[i];
				if (it.idx != entry.idx) continue;
				if (it.key.tag == elf_TAG_NIL) continue;
				elf_array_add(list,it.key);
			}
		}
	}
	return 1;
}


int table_lib_bubble_sort(elf_State *R) {
	#if 0
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
			elf_add_cls(R,cls);
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
	#endif
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
	// 	if (it.key.tag == elf_TAG_NIL) continue;
	// 	R->stack[k] = it.k;
	// 	R->stack[v] = tab->array[it.idx];
	// 	/* todo: should yield boolean to signal whether to
	// 	stop or not */
	// 	int ny = elf_call(R,tab,0,0,2,0);
	// 	if (ny != 0) if (elf_get_int(R,0) != 1) break;
	// }
	return 0;
}


/* todo: account for keyless values */
elf_Table *elf_tabcopy(elf_State *S, elf_Table *tab) {

	elf_debugger("not impl");

	elf_Table *copy = elf_alloc_table(S);
	elf_Int i;
	for (i=0;i<tab->ntotal;++i) {
		elf_Entry it = tab->slots[i];
		if (it.key.tag == elf_TAG_NIL) continue;
		elf_Value item = tab->array[it.idx];
		elf_table_set(copy,it.key,item);
	}
	return copy;
}


/* this function only clones keyed values... */
elf_Table *elf_clone_table(elf_State *S, elf_Table *tab) {
	elf_Table *clone = elf_alloc_table(S);
	elf_Int i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elf_Entry it = tab->slots[i];
		if (it.key.tag == elf_TAG_NIL) continue;
		elf_table_set(clone,it.key,tab->array[it.idx]);
	}
	return clone;
}


int table_lib_array(elf_State *S) {
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
	elf_add_tab(R,elf_clone_table(R,tab));
	return 1;
}


int table_lib_slice(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int x = 0;
	elf_Int y = ARRAY_LENGTH(tab->array);
	if (elf_get_num_args(R) >= 1) x = elf_get_int(R,0);
	if (elf_get_num_args(R) >= 2) y = elf_get_int(R,1);
	elf_Table *slice = elf_new_table(R);
	while (x < y) {
		elf_array_add(slice,tab->array[x ++]);
	}
	return 1;
}


int table_lib_xset(elf_State *R) {
	elf_check_args(R,":xset",2,"the value, and the index where to place the value");

	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Value value = elf_get_arg(R,0);

	elf_Int len = ARRAY_LENGTH(tab);
	elf_Int idx = elf_get_int(R,1);
	if ((idx %= len) < 0) idx += len;

	tab->array[idx] = value;
	return 0;
}


int table_lib_swap(elf_State *R) {
	elf_check_args(R,":swap",2,"the two indexes to swap");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int x = elf_get_int(R,0);
	elf_Int y = elf_get_int(R,1);
	elf_Value tmp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = tmp;
	return 0;
}


int table_lib_merge(elf_State *R) {
	elf_check_args(R,":merge",1,"the tables to merge into a new table (keys only), if no arguments are passed in, this acts like a clone");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sum = elf_new_table(R); /* <- */
	elf_merge_tables(sum,tab);
	for ( int i = 0; i < elf_get_num_args(R); ++ i ) {
		elf_merge_tables(sum,elf_get_tab(R,i));
	}
	sum->obj.meta = tab->obj.meta;
	return 1;
}

/*
** CHANGELOG 8/22/24: Now xmerge properly returns a new
** table, like merge does...
** todo: could this be renamed to make more clear?
*/
int table_lib_xmerge(elf_State *R) {
	elf_check_args(R,":xmerge",1,"the table to merge, all values are of the table are added to a new one, unline :merge, :xmerge will not check for duplicates");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *add = elf_get_tab(R,0);
	elf_Table *sum = elf_new_table(R); /* <- */
	elf_Int i;
	for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
		ARRAY_ADD(sum->array,tab->array[i]);
	}
	for (i=0;i<ARRAY_LENGTH(add->array);++i) {
		ARRAY_ADD(sum->array,add->array[i]);
	}
	return 1;
}


int table_lib_xclone(elf_State *R) {
	elf_check_args(R,":xclone",0,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *clone = elf_alloc_table(R);
	elf_Int i;
	for ( i = 0; i < ARRAY_LENGTH(tab->array); i += 1 ) {
		ARRAY_ADD(clone->array,tab->array[i]);
	}
	elf_add_tab(R,clone);
	return 1;
}


int table_lib_reverse(elf_State *R) {
	elf_check_args(R,":reverse",0,"");
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


int table_lib_diff(elf_State *R) {
	elf_check_args(R,":diff",1,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Table *sub = elf_get_tab(R,0);
	if (sub == 0) {
		elf_fail(R,NO_BYTE,"argument is nil");
	}
	elf_Table *dif = elf_alloc_table(R);
	elf_Int i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elf_Entry it = tab->slots[i];
		if (it.key.tag == elf_TAG_NIL) continue;
		if (elf_table_contains(sub,it.key)) continue;
		elf_table_set(dif,it.key,tab->array[it.idx]);
	}
	elf_add_tab(R,dif);
	return 1;
}


