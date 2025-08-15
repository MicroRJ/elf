//
// See Copyright Notice In elf.h
//
//
// core table functions
//


// todo: remove!
#define slot2value(T,X) (T->array[T->entries[X].idx])
// todo: remove!
#define slotiskey(T,X) ((X >= 0) && (T->entries[X].key.tag != ELF_TNIL) && (T->entries[X].key.tag != ELF_TTOMB))



inline void recycletable(Table tab) {
	free(tab->entries);

	ARRAY_DELETE(tab->array);

	tab->array = 0;
	tab->entries = 0;
}


static inline void init_table(elf_State *S, Table table, elf_Index nentries) {
	table->ndebug = 0;
	table->slots  = calloc(1, nentries * sizeof(TEntry));
	table->ntotal = nentries;
	table->nslots = 0;
}



elf_rawapi
Table newtable2(elf_State *S, Index nentries) {
	Table table = elf_gc_alloc(S, GC_TAB, sizeof(elf_Table));
	table->obj.meta = S->metatables.table;

	init_table(S, table, nentries);
	return table;
}



elf_rawapi
Table newtable(elf_State *R) {
	return newtable2(R, 4);
}



static void check_resize(Table table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		elf_Table new_table = *table;
		new_table.ntotal = table->ntotal << 1;
		new_table.entries = calloc(1,new_table.ntotal * sizeof(TEntry));

		FOR_RANGE(i, 0, table->ntotal) {
			TEntry prev_entry = table->entries[i];

			if (!isdead(prev_entry.key)) {

				Index new_entry_index = tabletry(&new_table,prev_entry.key);
				ASSERT(new_entry_index >= 0);

				new_table.entries[new_entry_index] = prev_entry;
			}
		}
		free(table->entries);

		table->ntotal = new_table.ntotal;
		table->entries = new_table.entries;
	}
}



static inline Hash hash_value(V *v) {
	if (v->tag == ELF_TSTRING) {
		return elf_get_string_hash(v->x_str);
	}
	return hash64(v->x_i64);
}



static inline bool value_equals(V *x, V *y) {
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



// todo: try double hashing
elf_rawapi
Index tabletry(Table tab, V key) {
	ASSERT(tab != 0);

	check_resize(tab);

	Hash hash = hash_value(&key);
	Index ntotal = tab->ntotal;
	Index head,tail,walk;
	head = hash & (ntotal - 1);
	tail = head;
	walk = 1;

	do {
		V value = tab->entries[tail].key;
		if (value.tag == ELF_TNIL) return tail;
		if (value.tag != ELF_TTOMB) {
			if (value_equals(&value,&key)) {
				return tail;
			}
		}
		tail = (tail + walk) & (ntotal - 1);
	} while (head != tail);

	return -1;
}



elf_rawapi
Index tableset(Table table, V key, V value) {
	Index index = -1;
	Index slot = tabletry(table, key);
	ASSERT(slot >= 0);

	TEntry *entry = table->slots + slot;
	if (!slotiskey(table,slot)) {
		index = darr_grow(table->array, 1);
		table->slots[slot].key = key;
		table->slots[slot].idx = index;
		table->nslots ++;
	} else {
		index = entry->idx;
	}

	table->array[index] = value;
	return index;
}



V elf_table_get_raw(Table tab, V k) {
	Index slot = tabletry(tab,k);
	if (slot == -2) NO_CODE;
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (V){ELF_TNIL,0};
}



static Index elf_table_get_index_always_(Table table, elf_Value key) {

	ASSERT(!visnil(key));

	Index slot = tabletry(table, key);
	ASSERT(slot >= 0);

	Index index = table->entries[slot].idx;

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
Index arrayadd(Table table, V v) {
	Index index = darr_grow(table->array, 1);
	table->array[index] = v;
	return index;
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












// void elf_tadd_tab(Table table, Table thing) {
// 	darr_add(table->array,VALUE_TABLE(thing));
// }



#if 0
typedef struct TABLE_BINARY_FILE {
	int num_keys;
} TABLE_BINARY_FILE;

void elf_table_export_binary(Table tab, FILE *io) {
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
// elf_Index elf_table_try_text(Table tab, const char *text, elf_i32 length, elf_Hash hash) {
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
// elf_Value elf_tgetx_any(Table tab, char const *key) {
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