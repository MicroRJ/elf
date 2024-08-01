/*
** See Copyright Notice In elf.h
** elf-tab.c
** Table
*/


elTable *elf_new_table_metatable(elState *R) {
	/* todo: these functions are to be refactored,
	a bunch of them are rather useless and or misnamed */
	struct { char *n; elBinding b; } elf_table_lib[] = {
		{"length",elf_table_libfn_length},
		{"tally",elf_table_libfn_tally},
		{"delete",elf_table_libfn_delete},
		{"haskey",elf_table_libfn_contains},
		{"lookup",elf_table_libfn_lookup},
		{"iter",elf_table_libfn_foreach},
		{"collisions",elf_table_libfn_get_collisions},
		{"add",elf_table_libfn_add},
		{"xadd",elf_table_libfn_xadd},
		{"itemize",elf_table_libfn_itemize},
		{"inject",elf_table_libfn_inject},
		{"idx",elf_table_libfn_index},
		{"xrem",elf_table_libfn_xremove},
		{"xdelete",elf_table_libfn_xdelete},
		{"bubblesort",elf_table_libfn_bubble_sort},
		{"fndaliases",elf_table_libfn_find_aliases},
		{"alias",elf_table_libfn_alias},
		{"merge",elf_table_libfn_merge},
		{"xmerge",elf_table_libfn_xmerge},
		{"reverse",elf_table_libfn_reverse},
		{"clone",elf_table_libfn_clone},
		{"xclone",elf_table_libfn_xclone},
		{"slice",elf_table_libfn_slice},
		{"xset",elf_table_libfn_xset},
		{"swap",elf_table_libfn_swap},
		{"diff",elf_table_libfn_diff},
		{0},
	};

	elTable *tab = elf_add_new_table(R);
	for (int i = 0; elf_table_lib[i].n != 0; ++ i) {
		elf_table_set_binding_field(R,tab,elf_table_lib[i].n,elf_table_lib[i].b);
	}
	return tab;
}


elTable *elf_new_table_of_length(elState *R, elInteger ntotal) {
	elTable *table = elf_new_object(R,OBJ_TAB,sizeof(elTable));
	table->obj.metatable = R->metatables.table;
	/* What if this is the first table... */
	// elASSERT(!! table->obj.metatable);

	table->ntotal = ntotal;
	table->nslots = 0;
	table->slots = elf_clear_alloc(elHEAP_ALLOCATOR,ntotal*sizeof(elEntry));
	return table;
}


elTable *elf_new_table(elState *R) {
	return elf_new_table_of_length(R,4);
}


void elf_dealloc_table(elTable *tab) {
	elf_dealloc(elHEAP_ALLOCATOR,tab->slots);
	elf_xarray_delete(tab->array);
	tab->array = 0;
	tab->slots = 0;
}


/*
** 	Traverses the table until it finds a match
** or a nil slot for this particular key.
**
** 	If the result is -1 it means no nil slot
** nor match found, this is most likely an
** error as it could mean the table has reached
** max capacity.
**
** 	Then you have to check whether the slot
** is nil, which means no match, use the
** result to modify the slot and value as desired.
**
** todo: have a dedicated function for looking up
** by string and another one by integers... since
** they are the most common...
**
*/
elInteger elf_table_try(elTable *tab, elValue key) {
	if ((tab == 0 || tab->obj.color == GC_RED) || (key.tag == TAG_NIL)) {
		elf_debugger("test-break");
		return -2;
	}
	/* this particular function uses double hashing,
	which should allow us to get more resolution out
	of the hash value, the first hash computes the
	starting index, and the secondary hash computes
	the step by which we increment.
	Since the increment depends on the data, it
	should reduce clustering, and in practice it
	has proven to be drastically more efficient
	than linear probing. */
	elEntry *slots = tab->slots;
	elInteger ntotal = tab->ntotal;
	elInteger hash = elf_table_get_value_hash(key);
	elInteger head = hash % ntotal;
	elInteger tail = head;
	elHashId walk = elf_table_rehash(hash)|1;
	do {
		elValue x = slots[tail].k;
		if (x.tag == TAG_NIL) return tail;
		if (elf_tabvaleq(&x,&key)) return tail;
		tail = (tail+walk) % ntotal;
		LDODEBUG( tab->ncollisions ++ );
	} while(head != tail);
	return -1;
}


elInteger elf_table_tryS(elTable *tab, char *contents) {
	if ((tab == 0 || tab->obj.color == GC_RED) || (contents == 0)) {
		elf_debugger("invalid-table");
		return -2;
	}
	elInteger length = strlen(contents);
	elEntry *slots = tab->slots;
	elInteger ntotal = tab->ntotal;
	elInteger hash = elf_tabhashstr(contents);
	elInteger head = hash % ntotal;
	elInteger tail = head;
	elHashId walk = elf_table_rehash(hash)|1;
	do {
		elValue x = slots[tail].key;
		if (x.tag == TAG_NIL) return tail;
		if (x.tag == TAG_STR) {
			if (x.x_str->contents == contents) {
				return tail;
			}
			if ((x.x_str->hash == hash) && (x.x_str->length == length) && S_eq(x.x_str->contents,contents)) {
				return tail;
			}
		}
		tail = (tail+walk) % ntotal;
		LDODEBUG( tab->ncollisions ++ );
	} while(head != tail);
	return -1;
}


elInteger elf_tabslot2index(elTable *table, elInteger slot) {
	return table->slots[slot].i;
}


elValue elf_tabslot2value(elTable *table, elInteger slot) {
	return table->array[table->slots[slot].i];
}


elBool elf_table_is_key(elTable *table, elInteger slot) {
	return slot >- 1 && table->slots[slot].k.tag != TAG_NIL;
}


void elf_tabslotsetkeyval(elTable *table, elInteger slot, elValue k, elInteger i) {
	table->slots[slot].k = k;
	table->slots[slot].i = i;
}


void elf_check_table(elTable *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		// LDODEBUG( table->ncollisions = 0 );
		/* todo: better strat */
		elTable new_table = * table;
		new_table.ntotal = table->ntotal << 2;
		if (new_table.ntotal < table->ntotal) elNOCODE;
		new_table.slots = elf_clear_alloc(elHEAP_ALLOCATOR,new_table.ntotal * sizeof(elEntry));

		for (int i = 0; i < table->ntotal; ++ i) {
			elEntry slot = table->slots[i];
			if (slot.k.tag == TAG_NIL) continue;

			elInteger newslot = elf_table_try(&new_table,slot.k);
			if (newslot < 0) elNOCODE;

			new_table.slots[newslot] = slot;
		}

		elf_dealloc(elHEAP_ALLOCATOR,table->slots);

		table->ntotal = new_table.ntotal;
		table->slots = new_table.slots;
	}
}


elBool elf_table_set(elTable *table, elValue k, elValue v) {
	elf_check_table(table);
	elInteger slot = elf_table_try(table,k);
	/* todo: instead return an error here */
	if (slot < 0) elNOCODE;
	elEntry *entry = table->slots + slot;
	if (!elf_table_is_key(table,slot)) {
		elInteger i = elf_xarray_growby(table->array,1);
		table->array[i] = v;

		table->slots[slot].k = k;
		table->slots[slot].i = i;
		table->nslots ++;
		return elFalse;
	} else {
		table->array[entry->i] = v;
		return elTrue;
	}
}


elValue elf_table_lookup(elTable *tab, elValue k) {
	elInteger slot = elf_table_try(tab,k);
	if (slot == -2) elNOCODE;
	if (elf_table_is_key(tab,slot)) {
		return elf_tabslot2value(tab,slot);
	}
	return (elValue){TAG_NIL,0};
}


elInteger elf_table_lookup_index(elTable *table, elValue k) {
	elASSERT((k.tag == TAG_INT || k.tag == TAG_NUM) || k.x_obj != 0);
	/* todo: why do we check the table here? */
	elf_check_table(table);
	elInteger slot = elf_table_try(table,k);
	if (slot < 0) elNOCODE;
	if (!elf_table_is_key(table,slot)) {
		elInteger i = elf_xarray_growby(table->array,1);
		table->array[i] = (elValue){TAG_NIL};
		table->slots[slot].k = k;
		table->slots[slot].i = i;
		table->nslots ++;
	}
	return elf_tabslot2index(table,slot);
}



void elf_table_alias(elState *S, elTable *tab, elValue key, elValue alias) {
	elf_check_table(tab);
	elInteger key_slot = elf_table_try(tab,key);
	if (elf_table_is_key(tab,key_slot)) {
		elInteger alias_slot = elf_table_try(tab,alias);
		tab->slots[alias_slot].k = alias;
		tab->slots[alias_slot].i = tab->slots[key_slot].i;
	} else elf_throw(S,NO_BYTE,"attempted to alias a key that was never added");
}


void elf_table_set_binding_field(elState *R, elTable *obj, char *name, elBinding b) {
	elf_table_set(obj,elf_string_value(elf_new_string(R,name)),elf_binding_value(b));
}


void elf_table_field_alias(elState *S, elTable *tab, char *key, elValue alias) {
	return elf_table_alias(S,tab,elf_string_value(elf_new_string(S,key)),alias);
}


elValue elf_table_get_field(elTable *tab, elString *key) {
	return elf_table_lookup(tab,elf_string_value(key));
}


elNumber elf_tabgetnum(elTable *tab, elString *key) {
	elValue val = elf_table_lookup(tab,elf_string_value(key));
	return elf_tonum(val);
}


elInteger elf_table_get_integer(elTable *tab, elString *key) {
	elValue val = elf_table_lookup(tab,elf_string_value(key));
	return elf_toint(val);
}


elString *elf_tabgetstr(elTable *tab, elString *key) {
	return elf_table_lookup(tab,elf_string_value(key)).x_str;
}


elTable *elf_get_table_table(elTable *tab, elString *key) {
	return elf_table_lookup(tab,elf_string_value(key)).x_tab;
}


elInteger elf_tabiadd(elTable *table, elValue v) {
	return elf_xarray_growby(table->array,1);
}


void elf_table_add(elTable *table, elValue v) {
	ARRAY_ADD(table->array,v);
}


void elf_table_set_string_field(elTable *tab, elString *key, elString *val) {
	elf_table_set(tab,elf_string_value(key),elf_string_value(val));
}


void elf_table_set_integer_field(elTable *tab, elString *key, elInteger val) {
	elf_table_set(tab,elf_string_value(key),elf_integer_value(val));
}


void elf_tabsetnumfld(elTable *tab, elString *key, elNumber val) {
	elf_table_set(tab,elf_string_value(key),elf_number_value(val));
}


void elf_table_set_table_field(elTable *tab, elString *key, elTable *val) {
	elf_table_set(tab,elf_string_value(key),elf_tab(val));
}


/* metatable */


int elf_table_libfn_length(elState *R) {
	elTable *tab = (elTable*) elf_get_this(R);
	elf_add_integer(R,array_length(tab->array));
	return 1;
}


int elf_table_libfn_tally(elState *R) {
	elTable *tab = (elTable*) elf_get_this(R);
	elf_add_integer(R,array_length(tab->array));
	return 1;
}


int elf_table_libfn_contains(elState *c) {
	elASSERT(c->f->x == 1);
	elTable *table = (elTable*) elf_get_this(c);
	elValue k = elf_get_value(c,0);
	elf_add_integer(c,elf_table_is_key(table,elf_table_try(table,k)));
	return 1;
}


int elf_table_libfn_lookup(elState *c) {
	elASSERT(c->f->x == 1);
	elValue k = elf_get_value(c,0);
	elTable *table = (elTable*) c->f->obj;
	elf_add_value(c,elf_table_lookup(table,k));
	return 1;
}


int elf_table_libfn_get_collisions(elState *c) {
	elTable *table = (elTable*) c->f->obj;
	elf_add_integer(c,table->ncollisions);
	return 1;
}


/* todo: ensure that [i] == :idx(i)
also xadd should be add instead, since
names with 'x' prefix work only for
arrays */
int elf_table_libfn_xadd(elState *R) {
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger i;
	elInteger length = array_length(tab->array);
	for (i = 0; i < elf_get_num_args(R); ++ i) {
		elf_table_set(tab,elf_integer_value(length+i),elf_get_value(R,i));
	}
	return 0;
}


int elf_table_libfn_add(elState *R) {
	elTable *tab = (elTable *) elf_get_this(R);
	int i;
	for ( i = 0; i < elf_get_num_args(R); ++i ) {
		elf_table_add(tab,elf_get_value(R,i));
	}
	return 0;
}


void elf_merge_tables(elTable *tab, elTable *merger) {
	elInteger i;
	for (i=0;i<merger->ntotal;++i) {
		elEntry it = merger->slots[i];
		if (it.k.tag == TAG_NIL) continue;
		elf_table_set(tab,it.k,merger->array[it.i]);
	}
}


int elf_table_libfn_inject(elState *R) {
	elf_check_args(R,":inject",1,"the table, all the fields of the table are injected onto this one");
	elTable *tab = (elTable *) elf_get_this(R);
	elf_merge_tables(tab,elf_get_table(R,0));
	return 0;
}


int elf_table_libfn_itemize(elState *R) {
	elTable *tab = (elTable *) elf_get_this(R);
	elTable *result = elf_add_new_table(R);
	elf_varforj(tab->array) {
		elf_table_add(result,tab->array[j]);
	}
	int i;
	for (i=0;i<R->call->nx;++i) {
		if (elf_get_tag(R,0) == TAG_TAB) {
			elTable *that = elf_get_table(R,0);
			elf_varforj(that->array) {
				elf_table_add(result,that->array[j]);
			}
		} else {
			elf_table_add(result,elf_get_value(R,i));
		}
	}
	return 1;
}


int elf_table_libfn_index(elState *R) {
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger len = array_length(tab->array);
	elValue value = elf_nil_value();
	if (len != 0) {
		for (int i = 0; i < elf_get_num_args(R); ++ i) {
			if (i != 0) {
				if (value.tag == TAG_NIL) {
					elf_throw(R,NO_BYTE,"nil object");
				}
				/* todo: please do much better error reporting
				here, this can be hard to figure out */
				if (value.tag != TAG_TAB) {
					elf_throw(R,NO_BYTE,"not a table");
				}
				if (tab == 0) {
					elf_throw(R,NO_BYTE,"nil object");
				}
			}

			elInteger idx = elf_get_integer(R,i);
			if ((idx %= len) < 0) idx += len;

			value = tab->array[idx];
			tab = value.x_tab;
		}
	}
	elf_add_value(R,value);
	return 1;
}


/*
** Deletes a key and its corresponding
** value from a table.
** The algorithm is pretty slow...
*/
int elf_table_libfn_delete(elState *R) {
	elASSERT(R->call->x >= 1);
	elTable *tab = (elTable *) elf_get_this(R);
	elValue key = elf_get_value(R,0);
	elEntry *slots = tab->slots;
	elValue *array = tab->array;
	elInteger slot = elf_table_try(tab,key);
	if ((slot < 0) || (slots[slot].k.tag == TAG_NIL)) {
		elf_throw(R,NO_BYTE,"invalid key");
		goto leave_;
	}
	elInteger idx = slots[slot].i;
	slots[slot].k = (elValue){TAG_NIL};
	slots[slot].i = 0;
	elInteger len = array_length(array);
	if ((idx < 0) || (idx > len-1)) {
		elf_throw(R,NO_BYTE,elf_tpf("key is invalid, points to invalid index %lli, there are %lli item(s)",idx,len));
		goto leave_;
	}
	elf_add_value(R,array[idx]);
	elInteger min = elf_xarray_pop(array);
	// if (idx != min) {
		//NOTE: Swap the items, then iterate to
		//find references and update them...
	array[idx] = array[min];
	elInteger i;
	for (i=0;i<tab->ntotal;++i) {
		if (slots[i].k.tag != TAG_NIL && slots[i].i == min) {
			slots[i].i = idx;
		}
	}
	// }

	return 1;
	leave_: eld_add_nil(R);
	return 1;
}


int elf_table_libfn_xdelete(elState *R) {
	elASSERT(elf_get_num_args(R) >= 1);
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger len = array_length(tab->array);
	if (len != 0) {
		if (elf_isobj(elf_get_tag(R,0))) {
			elObject *object = elf_get_object(R,0);
			/* todo: Speed */
			elValue *item = elNIL;
			elInteger idx;
			for ( idx = 0; idx < len; idx += 1 ) {
				if (tab->array[idx].x_obj == object) {
					item = &tab->array[idx];
					break;
				}
			}
			if (item == 0) {
				/* todo: maybe not crash here */
				elf_throw(R,NO_BYTE,"item does not belong");
			}
			elASSERT((item - tab->array) == idx);
			elf_add_value(R,*item);
			elInteger min = elf_xarray_pop(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {
			elInteger idx = elf_get_integer(R,0);
			if ((idx %= len) < 0) idx += len;

			elInteger min = elf_xarray_pop(tab->array);
			elf_add_value(R,tab->array[idx]);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		}
	} else eld_add_nil(R);
	return 1;
}


int elf_table_libfn_xremove(elState *R) {
	elASSERT(elf_get_num_args(R) >= 1);
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger len = array_length(tab->array);

	if (len != 0) {
		if (elf_isobj(elf_get_tag(R,0))) {
			elObject *object = elf_get_object(R,0);
			/* todo: lookup can be removed if tag came
			after the data instead so that obj addr was
			the same as value addr! Otherwise this is expensive!  */
			elValue *item = elNIL;
			elInteger idx;
			for (idx=0;idx<len;++idx) {
				if (tab->array[idx].x_obj == object) {
					item = &tab->array[idx];
					break;
				}
			}
			elASSERT((item - tab->array) == idx);
			// elInteger idx = item - tab->array;
			// if (item < tab->array || item > tab->array + len - 1) {
			// 	elf_throw(R,NO_BYTE,"item does not belong");
			// }
			if (item == elNIL) {
				elf_throw(R,NO_BYTE,"item does not belong");
			}
			elf_add_value(R,*item);
			elInteger min = elf_xarray_pop(tab->array);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		} else {

			elInteger idx = elf_get_integer(R,0);
			if ((idx %= len) < 0) idx += len;

			elInteger min = elf_xarray_pop(tab->array);
			elf_add_value(R,tab->array[idx]);
			if (idx != min) {
				tab->array[idx] = tab->array[min];
			}
		}
	} else eld_add_nil(R);
	return 1;
}


int elf_table_libfn_alias(elState *R) {
	elf_check_args(R,":alias",2,"(key of any, alias of any) -> none, adds a new entry to the table (alias) that points to where (key) points");
	elTable *tab = (elTable *) elf_get_this(R);
	elf_table_alias(R,tab,elf_get_value(R,0),elf_get_value(R,1));
	return 0;
}


int elf_table_libfn_find_aliases(elState *R) {
	elf_check_args(R,":fndaliases",1,"the key to find aliases for");
	elTable *tab = (elTable *) elf_get_this(R);
	elValue key = elf_get_value(R,0);
	elTable *list = elf_add_new_table(R);
	if (key.tag != TAG_NIL) {
		elInteger slot = elf_table_try(tab,key);
		if (elf_table_is_key(tab,slot)) {
			elEntry entry = tab->slots[slot];
			elInteger i;
			for (i=0;i<tab->ntotal;++i) {
				/* we also include ourselves */
				elEntry it = tab->slots[i];
				if (it.i != entry.i) continue;
				if (it.k.tag == TAG_NIL) continue;
				elf_table_add(list,it.k);
			}
		}
	}
	return 1;
}


int elf_table_libfn_bubble_sort(elState *R) {
	elf_check_args(R,":bubblesort",1,"comparator function");
	elTable *tab = (elTable *) elf_get_this(R);
	elValue *arr = tab->array;
	elClosure *cls = elf_get_closure(R,0);
	elBool sorted = false;
	do {
		sorted = elTrue;
		elInteger i;
		for (i=0;i<array_length(arr)-1;++i) {
			elValue *top = elf_get_stack_top(R);
			elRegId base = elf_add_closure(R,cls);
			elf_add_value(R,arr[i+0]);
			elf_add_value(R,arr[i+1]);
			int r = elf_call_function(R,base,2,1);
			elASSERT(r == 1);
			if (elf_get_integer(R,base)) {
				elValue tmp = arr[i+0];
				arr[i+0] = arr[i+1];
				arr[i+1] = tmp;
				sorted = false;
			}
			elf_set_stack_top(R,top);
		}
	} while(sorted != elTrue);
	return 0;
}


// void mergesortutil(elState *R, elValue *array, elInteger tally, elValue fn) {
// }


int elf_table_libfn_foreach(elState *R) {
	elASSERT(R->frame->nx == 1);
	elTable *tab = (elTable *) elf_get_this(R);
	elClosure *cls = elf_get_closure(R,0);
	elRegId k = elf_local_alloc(R,1);
	elRegId v = elf_local_alloc(R,1);
	elInteger i;
	for (i=0;i<tab->ntotal;++i) {
		elEntry it = tab->slots[i];
		if (it.k.tag == TAG_NIL) continue;
		R->stack[k] = it.k;
		R->stack[v] = tab->array[it.i];
		/* todo: should yield boolean to signal whether to
		stop or not */
		int ny = elf_callex(R,R->frame->obj,0,0,2,0);
		if (ny != 0) if (elf_get_integer(R,0) != elTrue) break;
	}
	return 0;
}


/* todo: account for keyless values */
elTable *elf_tabcopy(elState *S, elTable *tab) {

	elf_debugger("not impl");

	elTable *copy = elf_new_table(S);
	elInteger i;
	for (i=0;i<tab->ntotal;++i) {
		elEntry it = tab->slots[i];
		if (it.k.tag == TAG_NIL) continue;
		elValue item = tab->array[it.i];
		elf_table_set(copy,it.k,item);
	}
	return copy;
}


/* this function only clones keyed values... */
elTable *elf_clone_table(elState *S, elTable *tab) {
	elTable *clone = elf_new_table(S);
	elInteger i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elEntry it = tab->slots[i];
		if (it.k.tag == TAG_NIL) continue;
		elf_table_set(clone,it.k,tab->array[it.i]);
	}
	return clone;
}


int elf_table_libfn_array(elState *S) {
	elf_check_args(S,":array",0,"the table to get a copy of as an array");
	elTable *tab = (elTable *) elf_get_this(S);

	elTable *array = elf_add_new_table(S);
	elInteger i;
	for (i = 0; i < array_length(tab->array); ++i) {
		elf_table_add(array,tab->array[i]);
	}
	return 1;
}


int elf_table_libfn_clone(elState *R) {
	elf_check_args(R,":clone",0,"the table to clone");
	elTable *tab = (elTable *) elf_get_this(R);
	elf_add_table(R,elf_clone_table(R,tab));
	return 1;
}


int elf_table_libfn_slice(elState *R) {
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger x = 0;
	elInteger y = array_length(tab->array);
	if (elf_get_num_args(R) >= 1) x = elf_get_integer(R,0);
	if (elf_get_num_args(R) >= 2) y = elf_get_integer(R,1);
	elTable *slice = elf_add_new_table(R);
	while (x < y) {
		elf_table_add(slice,tab->array[x ++]);
	}
	return 1;
}


int elf_table_libfn_xset(elState *R) {
	elf_check_args(R,":xset",2,"the value, and the index where to place the value");

	elTable *tab = (elTable *) elf_get_this(R);
	elValue value = elf_get_value(R,0);

	elInteger len = array_length(tab);
	elInteger idx = elf_get_integer(R,1);
	if ((idx %= len) < 0) idx += len;

	tab->array[idx] = value;
	return 0;
}


int elf_table_libfn_swap(elState *R) {
	elf_check_args(R,":swap",2,"the two indexes to swap");
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger x = elf_get_integer(R,0);
	elInteger y = elf_get_integer(R,1);
	elValue tmp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = tmp;
	return 0;
}


int elf_table_libfn_merge(elState *R) {
	elf_check_args(R,":merge",1,"the tables to merge into a new table (keys only), if no arguments are passed in, this acts like a clone");
	elTable *tab = (elTable *) elf_get_this(R);
	elTable *sum = elf_add_new_table(R); /* <- */
	elf_merge_tables(sum,tab);
	for ( int i = 0; i < elf_get_num_args(R); ++ i ) {
		elf_merge_tables(sum,elf_get_table(R,i));
	}
	sum->obj.metatable = tab->obj.metatable;
	return 1;
}

/* todo: could this be renamed to make more clear? */
int elf_table_libfn_xmerge(elState *R) {
	elf_check_args(R,":xmerge",1,"the table to merge, all values are of the table are added to a new one, unline :merge, :xmerge will not check for duplicates");
	elTable *tab = (elTable *) elf_get_this(R);
	elTable *merger = elf_get_table(R,0);
	elInteger i;
	for (i=0;i<array_length(merger->array);++i) {
		ARRAY_ADD(tab->array,merger->array[i]);
	}
	return 0;
}


int elf_table_libfn_xclone(elState *R) {
	elf_check_args(R,":xclone",0,"");
	elTable *tab = (elTable *) elf_get_this(R);
	elTable *clone = elf_new_table(R);
	elInteger i;
	for ( i = 0; i < array_length(tab->array); i += 1 ) {
		ARRAY_ADD(clone->array,tab->array[i]);
	}
	elf_add_table(R,clone);
	return 1;
}




int elf_table_libfn_reverse(elState *R) {
	elf_check_args(R,":reverse",0,"");
	elTable *tab = (elTable *) elf_get_this(R);
	elInteger n = array_length(tab->array);
	elInteger i;
	elValue *array = tab->array;
	for (i = 0; i < n >> 1; i += 1) {
		elValue value = array[i];
		array[i] = array[n-1-i];
		array[n-1-i] = value;
	}
	return 0;
}

elBool elf_table_contains(elTable *tab, elValue key) {
	return elf_table_is_key(tab,elf_table_try(tab,key));
}

int elf_table_libfn_diff(elState *R) {
	elf_check_args(R,":diff",1,"the subtrahend, the result contains all the values of this table that are not present in the subtrahend");
	elTable *tab = (elTable *) elf_get_this(R);
	elTable *sub = elf_get_table(R,0);
	if (sub == elNIL) {
		elf_throw(R,NO_BYTE,"argument is nil");
	}
	elTable *dif = elf_new_table(R);
	elInteger i;
	for ( i = 0; i < tab->ntotal; ++i ) {
		elEntry it = tab->slots[i];
		if (it.key.tag == TAG_NIL) continue;
		if (elf_table_contains(sub,it.key)) continue;
		elf_table_set(dif,it.key,tab->array[it.i]);
	}
	elf_add_table(R,dif);
	return 1;
}


/* Some of the hash functions and comments
were borrowed from the great Sean Barrett (stb) */
elHashId elf_table_rehash(elHashId hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}


#if 1
// FNV-1a
elHashId elf_tabhashstr (char *bytes) {
	elHashId hash = 2166136261u;
	while (*bytes) {
		hash ^= *bytes++;
		hash *= 16777619;
	}
	return hash;
}
#else
elHashId elf_tabhashstr(char *bytes) {
	elHashId hash = 0;
	while (*bytes) {
		hash = (hash << 7) + (hash >> 25) + *bytes++;
	}
	return hash + (hash >> 16);
}
#endif


elHashId elf_tabhashptr(elAddr *p) {
   // typically lacking in low bits and high bits
	elHashId hash = elf_table_rehash((elHashId)(elInteger)p);
	hash += hash << 16;

   // pearson's shuffle
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return elf_table_rehash(hash);
}


elBool elf_tabvaleq(elValue *x, elValue *y) {
	if (x->tag != y->tag) {
		return false;
	}
	switch (x->tag) {
		case TAG_STR: {
			return elf_streq(x->s,y->s);
		}
		case TAG_OBJ:
		case TAG_SYS: case TAG_INT: case TAG_NUM:
		case TAG_TAB: case TAG_CLS: case TAG_BID: {
			return x->x_int == y->x_int;
		}
		default: elNOCODE;
	}
	return false;
}


elInteger elf_table_get_value_hash(elValue v) {
	switch (v.tag) {
		case TAG_STR: {
			elASSERT(v.x_str != 0);
			elASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case TAG_OBJ:
		case TAG_TAB: case TAG_CLS: case TAG_SYS:
		case TAG_INT: case TAG_NUM: case TAG_BID: {
			return elf_tabhashptr(v.p);
		}
		default: elNOCODE;
	}
	return false;
}

