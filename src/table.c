/*
** See Copyright Notice In elf.h
** elf-tab.c
** Table
*/

#if 1
static inline elf_i64 _vhash(elf_Value *v) {
	if(v->tag==elf_tag_str){
		return v->x_str->hash;
	}
	return v->x_i64;
}

static inline bool _veq(elf_Value *x, elf_Value *y) {
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
static inline elf_i64 _vhash(elf_Value *v) {
	switch (v.tag) {
		case elf_tag_str: {
			ASSERT(v.x_str != 0);
			ASSERT(v.x_str->hash != 0);
			return v.x_str->hash;
		}
		case elf_tag_userobj:
		case elf_tag_tab: case elf_tag_closure: case elf_tag_sysobj:
		case elf_tag_int: case elf_tag_num: case elf_tag_proc: {
			return elf_hash_ptr(v.x_ptr);
		}
		default: NO_CODE;
	}
	return 0;
}


bool _veq(elf_Value *x, elf_Value *y) {
	if (x->tag != y->tag) {
		return 0;
	}
	switch (x->tag) {
		case elf_tag_str: {
			return elf_get_strings_eq(x->x_str,y->x_str);
		}
		case elf_tag_userobj:
		case elf_tag_sysobj: case elf_tag_int: case elf_tag_num:
		case elf_tag_tab: case elf_tag_closure: case elf_tag_proc: {
			return x->x_int == y->x_int;
		}
		default: NO_CODE;
	}
	return 0;
}
#endif

static elf_Hash elf_rehash(elf_Hash hash);
static elf_Hash elf_hash_text(const char *text);
static elf_Hash elf_hash_ptr(void *ptr);
static elf_Bool _veq(elf_Value *x, elf_Value *y);

static elf_Int slot2index(elf_Table *table, elf_Int slot) {
	return table->slots[slot].idx;
}


#define slot2value(T,X) (T->array[T->slots[X].idx])
#define slotiskey(T,X) ((X >= 0) && (T->slots[X].key.tag != elf_tag_nil))

elf_Int elf_get_array_tally(elf_Table *table) {
	return ARRAY_LENGTH(table->array);
}



elf_Value elf_tgets_any(elf_Table *tab, elf_String *key) {
	return elf_table_get(tab,VSTR(key));
}


elf_Value elf_tgetx_any(elf_Table *tab, char const *key) {
	int length;
	elf_Hash hash;
	elf_Int slot;
	length=text_length(key);
	hash=elf_hash_text(key);
	slot=elf_table_try_text(tab,key,length,hash);
	ASSERT(slot!=-2);
	if (slotiskey(tab,slot)) {
		return slot2value(tab,slot);
	}
	return (elf_Value){elf_tag_nil,0};
}


/* uses 'double hashing', which aims to get more 'resolution'
out of the hash value. First hash computes the starting index,
and the secondary hash computes the step by which we increment.
since the increment depends on the data, it should reduce
clustering. */
elf_i64 elf_table_try(elf_Table *tab, elf_Value key) {
	ASSERT(tab != 0);
	elf_Entry *slots = tab->slots;
	elf_i64 ntotal = tab->ntotal;
	elf_i64 hash = _vhash(&key);
	elf_i64 head = hash % ntotal;
	elf_i64 tail = head;
	elf_i64 walk = 1; // elf_rehash(hash)|1;
	elf_Value value;
	do {
		value=slots[tail].key;
		if ((value.tag==elf_tag_nil)||(_veq(&value,&key))){
			return tail;
		}
		tail = (tail+walk) % ntotal;
	} while(head != tail);
	return -1;
}


elf_i64 elf_table_try_text(elf_Table *tab, const char *text, elf_i64 length, elf_Hash hash) {
	elf_Entry *slots = tab->slots;
	elf_i64 ntotal = tab->ntotal;
	elf_i64 head = hash % ntotal;
	elf_i64 tail = head;
	elf_Hash walk = elf_rehash(hash)|1;
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

elf_Bool elf_table_set(elf_Table *table, elf_Value k, elf_Value v) {
	_check_table(table);
	elf_Int slot = elf_table_try(table,k);
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
	ASSERT(!ISNILV(key));
	_check_table(table);
	lot=elf_table_try(table,key);
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
	_check_table(tab);
	elf_Int key_slot = elf_table_try(tab,key);
	if (slotiskey(tab,key_slot)) {
		elf_Int alias_slot = elf_table_try(tab,alias);
		tab->slots[alias_slot].key = alias;
		tab->slots[alias_slot].idx = tab->slots[key_slot].idx;
	} else elf_error(S,NO_BYTE,"attempted to alias a key that was never added");
}


elf_Bool elf_table_contains(elf_Table *tab, elf_Value key) {
	return slotiskey(tab,elf_table_try(tab,key));
}


void elf_array_add(elf_Table *table, elf_Value v) {
	ARRAY_ADD(table->array,v);
}


// void elf_tadd_tab(elf_Table *table, elf_Table *thing) {
// 	ARRAY_ADD(table->array,VTAB(thing));
// }


void elf_merge_tables(elf_Table *tab, elf_Table *merger) {
	elf_Int i;
	for (i=0;i<merger->ntotal;++i) {
		elf_Entry it = merger->slots[i];
		if (it.key.tag == elf_tag_nil) continue;
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

