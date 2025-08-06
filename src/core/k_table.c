//
// See Copyright Notice In elf.h
//
//
// core table functions
//


// todo: remove!
#define slot2value(T,X) (T->array[T->slots[X].idx])
// todo: remove!
#define slotiskey(T,X) ((X >= 0) && (T->slots[X].key.tag != elf_tag_Nil) && (T->slots[X].key.tag != elf_tag_Tomb))

#define IS_DEAD_TAG(T) (((T) == elf_tag_Nil) || ((T) == elf_tag_Tomb))

static inline void elf_tableK_recycle(elf_Table *tab) {
	free(tab->slots);
	ARRAY_DELETE(tab->array);
	tab->array = 0;
	tab->slots = 0;
}

static inline void init_table(elf_State *S, elf_Table *table, elf_IndexInt nentries) {
	table->ndebug = 0;
	table->slots  = calloc(1, nentries * sizeof(elf_Table_Entry));
	table->ntotal = nentries;
	table->nslots = 0;
}

static inline elf_Table *elf_alloc_table2(elf_State *S, elf_IndexInt nentries) {
	elf_Table *table = elf_gc_alloc(S, GC_TAB, sizeof(elf_Table));
	table->obj.meta = S->metatables.table;

	init_table(S, table, nentries);
	return table;
}

elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R, 4);
}


static void check_resize(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 1;
		new_table.slots = calloc(1,new_table.ntotal * sizeof(elf_Table_Entry));

		FOR_RANGE(i, 0, table->ntotal) {
			elf_Table_Entry prev_entry = table->slots[i];

			if (prev_entry.key.tag != elf_tag_Nil && prev_entry.key.tag != elf_tag_Tomb) {

				elf_i64 prev_index = elf_table_try_(&new_table,prev_entry.key);
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
	if (v->tag == elf_tag_String) {
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
		if(x->tag==elf_tag_String){
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
		case elf_tag_String: {
			ASSERT(v.x_str != 0);
			ASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case elf_tag_UserObject:
		case elf_tag_Table: case elf_tag_Closure: case elf_tag_Handle:
		case elf_tag_Int: case elf_tag_Num: case elf_tag_Function: {
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
		case elf_tag_String: {
			return elf_get_strings_eq(x->x_str,y->x_str);
		}
		case elf_tag_UserObject:
		case elf_tag_Handle: case elf_tag_Int: case elf_tag_Num:
		case elf_tag_Table: case elf_tag_Closure: case elf_tag_Function: {
			return x->x_int == y->x_int;
		}
		default: NO_CODE;
	}
	return 0;
}
#endif




elf_Int elf_array_get_length(elf_Table *table) {
	return ARRAY_LENGTH(table->array);
}

// todo: also why does this return the slot as supposed to just
// the entry directly? what is this for?
elf_IndexInt elf_table_try_(elf_Table *tab, elf_Value key) {
	ASSERT(tab != 0);

	check_resize(tab);

	elf_HashInt hash = hash_value(&key);

	elf_Table_Entry *slots = tab->slots;
	elf_IndexInt ntotal = tab->ntotal;

	elf_IndexInt head,tail,walk;

	head = hash & (ntotal - 1);
	tail = head;
	// todo: try double hashing
	walk = 1;

	do {
		elf_Value value = slots[tail].key;
		if (value.tag == elf_tag_Nil) return tail;
		if (value.tag != elf_tag_Tomb) {
			if(value_equals(&value,&key)){
				return tail;
			}
		}
		tail = (tail + walk) & (ntotal - 1);
	} while(head != tail);

	return -1;
}

elf_IndexInt elf_raw_table_set(elf_Table *table, elf_Value key, elf_Value value) {
	elf_IndexInt index = -1;
	elf_IndexInt slot = elf_table_try_(table, key);
	ASSERT(slot >= 0);

	elf_Table_Entry *entry = table->slots + slot;
	if (!slotiskey(table,slot)) {
		index = ARRAY_GROW(table->array,1);
		table->slots[slot].key = key;
		table->slots[slot].idx = index;
		table->nslots ++;
	} else {
		index = entry->idx;
	}

	table->array[index] = value;
	return index;
}


elf_Value elf_table_get_raw(elf_Table *tab, elf_Value k) {
	elf_Int slot = elf_table_try_(tab,k);
	if (slot == -2) NO_CODE;
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_tag_Nil,0};
}

static elf_IndexInt elf_table_get_index_always_(elf_Table *table, elf_Value key) {

	ASSERT(!isnil(key));

	elf_IndexInt slot = elf_table_try_(table, key);
	ASSERT(slot >= 0);

	elf_IndexInt index = table->entries[slot].idx;

	// there's nothing here, so create use up the slot
	if (IS_DEAD_TAG(table->entries[slot].key.tag)) {
		index = ARRAY_GROW(table->array, 1);
		table->entries[slot].key = key;
		table->entries[slot].idx = index;
		table->nslots++;

		table->array[index] = VALUE_NIL();
	}

	// todo: remove
	return index;
}


// todo: inline version of this?
elf_IndexInt elf_array_add_k(elf_Table *table, elf_Value v) {
	elf_IndexInt index = ARRAY_GROW(table->array, 1);
	table->array[index] = v;
	return index;
}


// void elf_tadd_tab(elf_Table *table, elf_Table *thing) {
// 	ARRAY_ADD(table->array,VALUE_TABLE(thing));
// }



#if 0
typedef struct TABLE_BINARY_FILE {
	int num_keys;
} TABLE_BINARY_FILE;

void elf_table_export_binary(elf_Table *tab, FILE *io) {
	FOR_RANGE(i, 0, ARRAY_LENGTH(tab->array)) {
		elf_Value value = tab->array[i];
		switch (value.tag) {
			// todo: compression!
			case elf_tag_Int: { fprintf(io, "%lli", value.x_i64); } break;
			default: {
				elf_error();
			}
		}
	}
}
#endif


// todo:
// to be entirely removed


// todo: this path can be entirely removed because we only
// ever use it for the registry stuff which will get its own data structure
// elf_IndexInt elf_table_try_text(elf_Table *tab, const char *text, elf_i32 length, elf_HashInt hash) {
// 	check_resize(tab);

// 	elf_Table_Entry *slots = tab->slots;
// 	elf_i64 ntotal = tab->ntotal;
// 	elf_i64 head = hash % ntotal;
// 	elf_i64 tail = head;
// 	elf_i64 walk = 1; // rehash(hash)|1;
// 	do {
// 		elf_Value x = slots[tail].key;
// 		if (x.tag==elf_tag_Nil) {
// 			return tail;
// 		}
// 		if (x.tag==elf_tag_String) {
// 			if (x.x_str->text==text) {
// 				return tail;
// 			}
// 			if ((x.x_str->hash==hash) && (x.x_str->length==length) && text_eq(x.x_str->text,text)) {
// 				return tail;
// 			}
// 		}
// 		tail = (tail + walk) % ntotal;
// 		// DEBUG_CODE( tab->ncollisions ++ );
// 	} while(head != tail);
// 	return -1;
// }



// todo: remove!
// elf_Value elf_tgetx_any(elf_Table *tab, char const *key) {
// 	int length;
// 	elf_i64 hash;
// 	elf_Int slot;
// 	length=text_length(key);
// 	hash=hash_text(key);
// 	slot=elf_table_try_text(tab,key,length,hash);
// 	ASSERT(slot!=-2);
// 	if (slotiskey(tab,slot)) {
// 		return slot2value(tab,slot);
// 	}
// 	return (elf_Value){elf_tag_Nil,0};
// }