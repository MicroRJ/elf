/*
** See Copyright Notice In elf.h
** elf-tab.c
** Table
*/


static elf_Int elf_hash_value(elf_Value v);
static elf_Hash elf_rehash(elf_Hash hash);
static elf_Hash elf_hash_text(const char *text);
static elf_Hash elf_hash_ptr(void *ptr);
static elf_Bool elf_value_eq(elf_Value *x, elf_Value *y);


elf_Table *elf_alloc_table2(elf_State *R, elf_Int ntotal) {
	elf_Table *table = elf_alloc_object(R,GC_TAB,sizeof(elf_Table));
	table->obj.meta = R->metatables.table;
	/* What if this is the first table... */
	// ASSERT(!! table->obj.meta);

	table->ntotal = ntotal;
	table->nslots = 0;
	table->slots = calloc_memory(GLOBAL_ALLOCATOR,ntotal*sizeof(elf_Entry));
	return table;
}


elf_Table *elf_alloc_table(elf_State *R) {
	return elf_alloc_table2(R,4);
}


static elf_Int slot2index(elf_Table *table, elf_Int slot) {
	return table->slots[slot].idx;
}


#define slot2value(T,X) (T->array[T->slots[X].idx])
#define slotiskey(T,X) ((X >= 0) && (T->slots[X].key.tag != elf_TAG_NIL))


void elf_free_table_contents(elf_Table *tab) {
	dealloc_memory(GLOBAL_ALLOCATOR,tab->slots);
	ARRAY_DELETE(tab->array);
	tab->array = 0;
	tab->slots = 0;
}


elf_Int elf_get_table_length(elf_Table *table) {
	return ARRAY_LENGTH(table->array);
}



elf_Value elf_tgets_any(elf_Table *tab, elf_String *key) {
	return elf_tget_any(tab,VSTR(key));
}


elf_Value elf_tgetx_any(elf_Table *tab, char const *key) {
	int length;
	elf_Hash hash;
	elf_Int slot;
	length=text_length(key);
	hash=elf_hash_text(key);
	slot=elf_ttryx(tab,key,length,hash);
	ASSERT(slot!=-2);
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_TAG_NIL,0};
}


/* uses 'double hashing', which aims to get more 'resolution'
out of the hash value. First hash computes the starting index,
and the secondary hash computes the step by which we increment.
since the increment depends on the data, it should reduce
clustering. */
elf_Int elf_ttry(elf_Table *tab, elf_Value key) {
	ASSERT(tab != 0);
	elf_Entry *slots = tab->slots;
	elf_Int ntotal = tab->ntotal;
	elf_Int hash = elf_hash_value(key);
	elf_Int head = hash % ntotal;
	elf_Int tail = head;
	elf_Hash walk = elf_rehash(hash)|1;
	elf_Value value;
	do {
		value=slots[tail].key;
		if ((value.tag==elf_TAG_NIL)||(elf_value_eq(&value,&key))){
			return tail;
		}
		tail = (tail+walk) % ntotal;
		// DEBUG_CODE( tab->ncollisions ++ );
	} while(head != tail);
	return -1;
}


elf_Int elf_ttryx(elf_Table *tab, const char *text, elf_Int length, elf_Hash hash) {
	elf_Entry *slots = tab->slots;
	elf_Int ntotal = tab->ntotal;
	elf_Int head = hash % ntotal;
	elf_Int tail = head;
	elf_Hash walk = elf_rehash(hash)|1;
	do {
		elf_Value x = slots[tail].key;
		if (x.tag==elf_TAG_NIL) {
			return tail;
		}
		if (x.tag==elf_TAG_STR) {
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


void elf_check_table(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		// DEBUG_CODE( table->ncollisions = 0 );
		/* todo: better strat */
		elf_Table new_table = *table;
		new_table.ntotal=table->ntotal << 2;
		if (new_table.ntotal<table->ntotal) NO_CODE;
		new_table.slots=calloc_memory(GLOBAL_ALLOCATOR,new_table.ntotal*sizeof(elf_Entry));

		elf_Entry old_slot;
		elf_Int new_slot;
		FOR_RANGE(i,0,table->ntotal) {
			old_slot=table->slots[i];
			if (old_slot.key.tag==elf_TAG_NIL) continue;

			new_slot=elf_ttry(&new_table,old_slot.key);
			ASSERT(new_slot>=0);
			new_table.slots[new_slot]=old_slot;
		}
		dealloc_memory(GLOBAL_ALLOCATOR,table->slots);

		table->ntotal=new_table.ntotal;
		table->slots=new_table.slots;
	}
}


elf_Bool elf_table_set(elf_Table *table, elf_Value k, elf_Value v) {
	elf_check_table(table);
	elf_Int slot = elf_ttry(table,k);
	/* todo: instead return an error here */
	if (slot < 0) NO_CODE;
	elf_Entry *entry = table->slots + slot;
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


elf_Value elf_tget_any(elf_Table *tab, elf_Value k) {
	elf_Int slot = elf_ttry(tab,k);
	if (slot == -2) NO_CODE;
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_TAG_NIL,0};
}


elf_Int elf_tget_ornew(elf_Table *table, elf_Value key) {
	elf_Int lot,idx;
	ASSERT(!ISNILV(key));
	elf_check_table(table);
	lot=elf_ttry(table,key);
	ASSERT(lot>=0);
	if (!slotiskey(table,lot)) {
		idx=ARRAY_GROW(table->array,1);
		table->array[idx]=VNIL();
		table->slots[lot].key=key;
		table->slots[lot].idx=idx;
		table->nslots++;
	}
	return slot2index(table,lot);
}



void elf_table_alias(elf_State *S, elf_Table *tab, elf_Value key, elf_Value alias) {
	elf_check_table(tab);
	elf_Int key_slot = elf_ttry(tab,key);
	if (slotiskey(tab,key_slot)) {
		elf_Int alias_slot = elf_ttry(tab,alias);
		tab->slots[alias_slot].key = alias;
		tab->slots[alias_slot].idx = tab->slots[key_slot].idx;
	} else elf_fail(S,NO_BYTE,"attempted to alias a key that was never added");
}


elf_Bool elf_table_contains(elf_Table *tab, elf_Value key) {
	return slotiskey(tab,elf_ttry(tab,key));
}


void elf_tadd(elf_Table *table, elf_Value v) {
	ARRAY_ADD(table->array,v);
}


void elf_tadd_tab(elf_Table *table, elf_Table *thing) {
	ARRAY_ADD(table->array,VTAB(thing));
}


void elf_merge_tables(elf_Table *tab, elf_Table *merger) {
	elf_Int i;
	for (i=0;i<merger->ntotal;++i) {
		elf_Entry it = merger->slots[i];
		if (it.key.tag == elf_TAG_NIL) continue;
		elf_table_set(tab,it.key,merger->array[it.idx]);
	}
}


/* some of the hash functions and comments
were borrowed from the great Sean Barrett (stb),
will return later... */
elf_Hash elf_rehash(elf_Hash hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}


elf_Hash elf_hash_text(const char *text) {
	elf_Hash hash;
	for (hash=2166136261u; *text; hash^=*text++, hash*=16777619);
	return hash;
}


elf_Hash elf_hash_ptr(void *p) {
   // typically lacking in low bits and high bits
	elf_Hash hash = elf_rehash((elf_Hash)(elf_Int)p);
	hash += hash << 16;
   // pearson's shuffle
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return elf_rehash(hash);
}


elf_Int elf_hash_value(elf_Value v) {
	switch (v.tag) {
		case elf_TAG_STR: {
			ASSERT(v.x_str != 0);
			ASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case elf_TAG_OBJ:
		case elf_TAG_TAB: case elf_TAG_CLS: case elf_TAG_SYS:
		case elf_TAG_INT: case elf_TAG_NUM: case elf_TAG_CFN: {
			return elf_hash_ptr(v.x_ptr);
		}
		default: NO_CODE;
	}
	return 0;
}


elf_Bool elf_value_eq(elf_Value *x, elf_Value *y) {
	if (x->tag != y->tag) {
		return 0;
	}
	switch (x->tag) {
		case elf_TAG_STR: {
			return elf_get_strings_eq(x->x_str,y->x_str);
		}
		case elf_TAG_OBJ:
		case elf_TAG_SYS: case elf_TAG_INT: case elf_TAG_NUM:
		case elf_TAG_TAB: case elf_TAG_CLS: case elf_TAG_CFN: {
			return x->x_int == y->x_int;
		}
		default: NO_CODE;
	}
	return 0;
}


