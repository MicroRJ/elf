//
// See Copyright Notice In elf.h
//
//
// core table functions
//


// todo: remove!
#define slot2value(T,X) (T->array[T->slots[X].idx])
// todo: remove!
#define slotiskey(T,X) ((X >= 0) && (T->slots[X].key.tag != ELF_TNIL) && (T->slots[X].key.tag != ELF_TTOMB))


elf_rawapi
inline void elf_tableK_recycle(elf_Table *tab) {
	free(tab->slots);

	ARRAY_DELETE(tab->array);

	tab->array = 0;
	tab->slots = 0;
}


static inline void init_table(elf_State *S, elf_Table *table, elf_Index nentries) {
	table->ndebug = 0;
	table->slots  = calloc(1, nentries * sizeof(TEntry));
	table->ntotal = nentries;
	table->nslots = 0;
}


elf_rawapi
inline elf_Table *elf_alloc_table2(elf_State *S, elf_Index nentries) {
	elf_Table *table = elf_gc_alloc(S, GC_TAB, sizeof(elf_Table));
	table->obj.meta = S->metatables.table;

	init_table(S, table, nentries);
	return table;
}


elf_rawapi
inline elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R, 4);
}


static void check_resize(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 1;
		new_table.slots = calloc(1,new_table.ntotal * sizeof(TEntry));

		FOR_RANGE(i, 0, table->ntotal) {
			TEntry prev_entry = table->slots[i];

			if (prev_entry.key.tag != ELF_TNIL && prev_entry.key.tag != ELF_TTOMB) {

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


static inline hash_t hash_value(elf_Value *v) {
	if (v->tag == ELF_TSTRING) {
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
		if(x->tag==ELF_TSTRING){
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
		case ELF_TSTRING: {
			ASSERT(v.x_str != 0);
			ASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case ELF_TUSER:
		case ELF_TTABLE: case ELF_TCLOSURE: case ELF_THANDLE:
		case ELF_TINTEGER: case ELF_TNUMBER: case ELF_TFUNCTION: {
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
		case ELF_TSTRING: {
			return elf_get_strings_eq(x->x_str,y->x_str);
		}
		case ELF_TUSER:
		case ELF_THANDLE: case ELF_TINTEGER: case ELF_TNUMBER:
		case ELF_TTABLE: case ELF_TCLOSURE: case ELF_TFUNCTION: {
			return x->x_int == y->x_int;
		}
		default: NO_CODE;
	}
	return 0;
}
#endif


elf_rawapi
elf_Index elf_table_try_(elf_Table *tab, elf_Value key) {
	ASSERT(tab != 0);

	check_resize(tab);

	hash_t hash = hash_value(&key);

	TEntry *slots = tab->slots;
	elf_Index ntotal = tab->ntotal;

	elf_Index head,tail,walk;

	head = hash & (ntotal - 1);
	tail = head;
	// todo: try double hashing
	walk = 1;

	do {
		elf_Value value = slots[tail].key;
		if (value.tag == ELF_TNIL) return tail;
		if (value.tag != ELF_TTOMB) {
			if(value_equals(&value,&key)){
				return tail;
			}
		}
		tail = (tail + walk) & (ntotal - 1);
	} while(head != tail);

	return -1;
}

elf_rawapi
elf_Index tableset(elf_Table *table, elf_Value key, elf_Value value) {
	elf_Index index = -1;
	elf_Index slot = elf_table_try_(table, key);
	ASSERT(slot >= 0);

	TEntry *entry = table->slots + slot;
	if (!slotiskey(table,slot)) {
		index = darr_grow(table->array,1);
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
	elf_Integer slot = elf_table_try_(tab,k);
	if (slot == -2) NO_CODE;
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){ELF_TNIL,0};
}


static elf_Index elf_table_get_index_always_(elf_Table *table, elf_Value key) {

	ASSERT(!visnil(key));

	elf_Index slot = elf_table_try_(table, key);
	ASSERT(slot >= 0);

	elf_Index index = table->entries[slot].idx;

	// there's nothing here, so create use up the slot
	if (isdead(table->entries[slot].key)) {
		index = darr_grow(table->array, 1);
		table->entries[slot].key = key;
		table->entries[slot].idx = index;
		table->nslots++;

		vsetnil(&table->array[index]);
	}

	// todo: remove
	return index;
}


elf_rawapi
elf_Index arrayadd(elf_Table *table, elf_Value v) {
	elf_Index index = darr_grow(table->array, 1);
	table->array[index] = v;
	return index;
}


elf_pubapi
elf_Value elf_table_get(elf_State *inter) {
	elf_Value tab = inter->stack_ptr[-2];
	elf_Value key = inter->stack_ptr[-1];

	if (tab.tag != ELF_TTABLE) {
		elf_error(inter, NO_BYTE, "Not A Table!");
	}

	elf_Value val = elf_table_get_raw(vgettab(tab), key);

	inter->stack_ptr -= 1;
	return val;
}
















// void elf_tadd_tab(elf_Table *table, elf_Table *thing) {
// 	darr_add(table->array,VALUE_TABLE(thing));
// }



#if 0
typedef struct TABLE_BINARY_FILE {
	int num_keys;
} TABLE_BINARY_FILE;

void elf_table_export_binary(elf_Table *tab, FILE *io) {
	FOR_RANGE(i, 0, darr_l(tab->array)) {
		elf_Value value = tab->array[i];
		switch (value.tag) {
			// todo: compression!
			case ELF_TINTEGER: { fprintf(io, "%lli", value.x_i64); } break;
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
// elf_Index elf_table_try_text(elf_Table *tab, const char *text, elf_i32 length, hash_t hash) {
// 	check_resize(tab);

// 	TEntry *slots = tab->slots;
// 	elf_i64 ntotal = tab->ntotal;
// 	elf_i64 head = hash % ntotal;
// 	elf_i64 tail = head;
// 	elf_i64 walk = 1; // rehash(hash)|1;
// 	do {
// 		elf_Value x = slots[tail].key;
// 		if (x.tag==ELF_TNIL) {
// 			return tail;
// 		}
// 		if (x.tag==ELF_TSTRING) {
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
// 	elf_Integer slot;
// 	length=text_l(key);
// 	hash=hash_text(key);
// 	slot=elf_table_try_text(tab,key,length,hash);
// 	ASSERT(slot!=-2);
// 	if (slotiskey(tab,slot)) {
// 		return slot2value(tab,slot);
// 	}
// 	return (elf_Value){ELF_TNIL,0};
// }