//
// See Copyright Notice In elf.h
//


void elf_table_recycle(elf_Table *tab) {
	free(tab->slots);
	ARRAY_DELETE(tab->array);
	tab->array = 0;
	tab->slots = 0;
}

void elf_init_table(elf_State *S, elf_Table *table, elf_i64 num_initial_entries) {
	table->ndebug = 0;
	table->slots  = calloc(1, num_initial_entries * sizeof(elf_Table_Entry));
	table->ntotal = num_initial_entries;
	table->nslots = 0;
}

elf_Table *elf_alloc_table2(elf_State *S, elf_i64 num_initial_entries) {
	elf_Table *table = elf_gc_alloc(S, GC_TAB, sizeof(elf_Table));
	table->obj.meta = S->metatables.table;

	elf_init_table(S, table, num_initial_entries);
	return table;
}

// todo: just make this take the length and if the
// length is zero then use some default value
elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R, 4);
}


static void elf_table_resize_maybe(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 1;
		new_table.slots = calloc(1,new_table.ntotal * sizeof(elf_Table_Entry));

		FOR_RANGE(i, 0, table->ntotal) {
			elf_Table_Entry prev_entry = table->slots[i];

			if (prev_entry.key.tag != elf_tag_nil && prev_entry.key.tag != elf_tag_tomb) {

				elf_i64 prev_index = elf_table_try(&new_table,prev_entry.key);
				ASSERT(prev_index >= 0);

				new_table.slots[prev_index] = prev_entry;
			}
		}
		free(table->slots);

		table->ntotal = new_table.ntotal;
		table->slots = new_table.slots;
	}
}


// todo: optimize
#if 1


static inline elf_HashInt hash_value(elf_Value *v) {
	if (v->tag == elf_tag_str) {
		return elf_get_string_hash(v->x_str);
	}
	//
	// Note for myself:
	//
	//	The hash-tables implementations I know, from
	// stb mainly, do something to hash integer bits,
	//	and so I did the same thing.
	//
	//	One day however I decided to get rid of it because
	// in my head I couldn't see how it could possibly make
	// a difference, I mean it's already an integer, right?
	//
	//	Then as I kept making games that relied more and more
	// on integer lookups, I started noticing unusually high
	// CPU usage and significantly lower FPS.
	//
	//	I don't remember exactly how I figured this out,
	// I think  maybe by stepping thru with the debugger
	// and seeing how many hash misses an integer lookup
	// would get.
	//
	// So yes, integer hashing is crucial, the performance
	// drop was because of all the time spent doing the
	// lookups because of all the misses.
	return hash64(v->x_i64);
}

static inline bool value_equals(elf_Value *x, elf_Value *y) {
	if(x->x_i32==y->x_i32 && x->y_i32==y->y_i32){
		return true;
	}
	if (x->tag==y->tag) {
		if(x->tag==elf_tag_str){
			if (x->x_str->hash==y->x_str->hash){
				if (x->x_str->length==y->x_str->length){
					return text_eq(x->x_str->text,y->x_str->text);
				}
			}
		}
	}
	return 0;
}
#else
static inline elf_i64 hash_value(elf_Value *v) {
	switch (v.tag) {
		case elf_tag_str: {
			ASSERT(v.x_str != 0);
			ASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case elf_tag_userobj:
		case elf_tag_tab: case elf_tag_closure: case elf_tag_sysobj:
		case elf_tag_int: case elf_tag_num: case elf_tag_function: {
			return hash64(v.x_ptr);
		}
		default: NO_CODE;
	}
	return 0;
}


bool value_equals(elf_Value *x, elf_Value *y) {
	if (x->tag != y->tag) {
		return 0;
	}
	switch (x->tag) {
		case elf_tag_str: {
			return elf_get_strings_eq(x->x_str,y->x_str);
		}
		case elf_tag_userobj:
		case elf_tag_sysobj: case elf_tag_int: case elf_tag_num:
		case elf_tag_tab: case elf_tag_closure: case elf_tag_function: {
			return x->x_int == y->x_int;
		}
		default: NO_CODE;
	}
	return 0;
}
#endif



static elf_Int slot2index(elf_Table *table, elf_Int slot) {
	return table->slots[slot].idx;
}


elf_Int elf_array_get_length(elf_Table *table) {
	return ARRAY_LENGTH(table->array);
}



//	elf_Value elf_tgets_any(elf_Table *tab, elf_String *key) {
//		return elf_table_get(tab,VALUE_STRING(key));
//	}

// todo: remove!
elf_Value elf_tgetx_any(elf_Table *tab, char const *key) {
	int length;
	elf_i64 hash;
	elf_Int slot;
	length=text_length(key);
	hash=hash_text(key);
	slot=elf_table_try_text(tab,key,length,hash);
	ASSERT(slot!=-2);
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_tag_nil,0};
}

elf_i64 elf_table_try(elf_Table *tab, elf_Value key) {
	ASSERT(tab != 0);
	elf_Table_Entry *slots = tab->slots;
	elf_i64 ntotal = tab->ntotal;
	elf_i64 hash = hash_value(&key);
	elf_i64 head = hash & (ntotal - 1);
	elf_i64 tail = head;
	// todo: try double hashing
	elf_i64 walk = 1;
	do {
		elf_Value value = slots[tail].key;
		if(value.tag == elf_tag_nil) return tail;
		if(value.tag != elf_tag_tomb){
			if(value_equals(&value,&key)){
				return tail;
			}
		}
		tail = (tail + walk) & (ntotal - 1);
	} while(head != tail);
	return -1;
}

// todo: remove!
elf_IndexInt elf_table_try_text(elf_Table *tab, const char *text, elf_i32 length, elf_HashInt hash) {
	elf_Table_Entry *slots = tab->slots;
	elf_i64 ntotal = tab->ntotal;
	elf_i64 head = hash % ntotal;
	elf_i64 tail = head;
	elf_i64 walk = 1; // rehash(hash)|1;
	do {
		elf_Value x = slots[tail].key;
		if (x.tag==elf_tag_nil) {
			return tail;
		}
		if (x.tag==elf_tag_str) {
			if (x.x_str->text==text) {
				return tail;
			}
			if ((x.x_str->hash==hash) && (x.x_str->length==length) && text_eq(x.x_str->text,text)) {
				return tail;
			}
		}
		tail = (tail + walk) % ntotal;
		// DEBUG_CODE( tab->ncollisions ++ );
	} while(head != tail);
	return -1;
}

elf_b32 elf_table_set(elf_Table *table, elf_Value k, elf_Value v) {
	elf_table_resize_maybe(table);
	elf_Int slot = elf_table_try(table,k);
	/* todo: instead return an error here */
	if (slot < 0) NO_CODE;
	elf_Table_Entry *entry = table->slots + slot;
	if (!slotiskey(table,slot)) {
		elf_Int i = ARRAY_GROW(table->array,1);
		table->array[i] = v;

		table->slots[slot].key = k;
		table->slots[slot].idx = i;
		table->nslots ++;
		return 0;
	} else {
		table->array[entry->idx] = v;
		return 1;
	}
}


elf_Value elf_table_get(elf_Table *tab, elf_Value k) {
	elf_Int slot = elf_table_try(tab,k);
	if (slot == -2) NO_CODE;
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_tag_nil,0};
}


elf_Int elf_table_get_or_add(elf_Table *table, elf_Value key) {
	elf_Int lot,idx;
	ASSERT(!IS_NIL_VALUE(key));
	elf_table_resize_maybe(table);
	lot=elf_table_try(table,key);
	ASSERT(lot>=0);
	if (!slotiskey(table,lot)) {
		idx=ARRAY_GROW(table->array,1);
		table->array[idx]=VALUE_NIL();
		table->slots[lot].key=key;
		table->slots[lot].idx=idx;
		table->nslots++;
	}
	return slot2index(table,lot);
}



void elf_table_alias(elf_Table *tab, elf_Value key, elf_Value alias) {
	elf_table_resize_maybe(tab);
	elf_Int key_slot = elf_table_try(tab,key);
	if (slotiskey(tab,key_slot)) {
		elf_Int alias_slot = elf_table_try(tab,alias);
		tab->slots[alias_slot].key = alias;
		tab->slots[alias_slot].idx = tab->slots[key_slot].idx;
	} else {
		// todo: is this an error?
	}
}


elf_b32 elf_table_contains(elf_Table *tab, elf_Value key) {
	return slotiskey(tab,elf_table_try(tab,key));
}


void elf_array_add(elf_Table *table, elf_Value v) {
	ARRAY_ADD(table->array,v);
}


// void elf_tadd_tab(elf_Table *table, elf_Table *thing) {
// 	ARRAY_ADD(table->array,VALUE_TABLE(thing));
// }


void elf_table_merge(elf_Table *tab, elf_Table *merger) {
	elf_Int i;
	for (i=0;i<merger->ntotal;++i) {
		elf_Table_Entry it = merger->slots[i];
		if (it.key.tag == elf_tag_nil) continue;
		elf_table_set(tab,it.key,merger->array[it.idx]);
	}
}


