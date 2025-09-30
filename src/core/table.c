//
// See Copyright Notice In elf.h
//
//


//
// table only things
//







static inline void _table_clearentries(Tab tab) {
	free(tab->entries);
	tab-> entries    = 0;
	tab->nentries    = 0;
	tab->fillcounter = 0;
}

//
//
//
//
//
//
//
//

static inline void _table_clear(Tab tab) {
	_table_clearentries(tab);

	free_heap_array(tab->array);
	tab->array = 0;
}

//
//
//
//
//
//
//
//

static inline void _table_init(elf_State *S, Tab table, Index nentries) {
	table->ndebug = 0;
	table->fillcounter = 0;
	// what's the point of always allocating this...
	table->entries = calloc(1, nentries * sizeof(*table->entries));
	table->nentries = nentries;
}

//
//
//
//
//
//
//
//

Tab newtable2(elf_State *S, Index nentries) {
	Tab table = gcalloc(S, GC_TAB, sizeof(*table));
	table->obj.meta = S->metatables.table;

	_table_init(S, table, nentries);
	return table;
}

//
//
//
//
//
//
//
//

Tab new_table(elf_State *R) {
	return newtable2(R, 4);
}

//
//
//
//
//
//
//
//

static inline Hash _table_hashvalue(V *v) {
	if (v->tag == ELF_TSTRING) {
		return strh(v->x_str);
	}
	return hash64(v->x_i64);
}

//
//
//
//
//
//
//
//

static inline bool _table_veq(V x, V y) {

	if (x.x_i64 == y.x_i64) {
		return true;
	}

	if (x.tag == y.tag) {
		if(x.tag == ELF_TSTRING) {
			return streq(as_string(x), as_string(y));
		}
	}


	return false;
}

//
//
//
//
//
//
//
//

#define FIND_ENTRY_FAILED ((Index)(-1))


//
// returning index adds redundant loads, will the compiler figure
// this out?
//
//	Treat as macro function? Force the compiler to inline?
//
// Return a find result instead?
//
// If I return whether the key was found or not, might be more obvious
// control flow wise.
//
//
//	Loading the value after will definitely cause a cache miss
// values are large, entries are larger ...
//
// Nothing that we can do about this...
//
static inline Index _table_findentry_internal(Entry *entries, Index nentries, Value key) {
	Index head,tail,walk;
	Hash hash;

	hash = _table_hashvalue(&key);
	head = hash & (nentries - 1);
	tail = head;

	// When does double hashing work best? For higher load factors?
	// Linear probing works best for smaller tables? Better cache?
	walk = 1;

	do {

		Value tabkey = entries[tail].key;

		if (tabkey.tag == ELF_TNIL) {
			return tail;
		}
		else if (tabkey.tag != ELF_TTOMB) {
			if (_table_veq(tabkey, key)) {
				return tail;
			}
		}

		tail = (tail + walk) & (nentries - 1);

	} while (head != tail);

	return FIND_ENTRY_FAILED;
}

//
//
//
//
//
//
//
//

static void _table_checkresize(Tab table) {
	// 0.75
	if (table->nentries * 3 < table->fillcounter * 4)
	{
		Table new_table = *table;
		new_table.nentries = table->nentries << 1;
		new_table.entries = calloc(1, new_table.nentries * sizeof(*new_table.entries));

		Index i;
		for(i=0; i<table->nentries; ++i)
		{
			IndexValue prev_entry = table->entries[i];

			if (iskey(prev_entry.key)) {

				Index new_entry_index = _table_findentry_internal(new_table.entries, new_table.nentries, prev_entry.key);
				ASSERT(new_entry_index != FIND_ENTRY_FAILED);

				new_table.entries[new_entry_index] = prev_entry;
			}
		}
		free(table->entries);

		table->ntotal = new_table.ntotal;
		table->entries = new_table.entries;
	}
}

//
//
//
//

static inline Index _table_tryresize(elf_State *S, Tab tab, V key) {
	ASSERT(tab != 0);
	_table_checkresize(tab);
	Index index = _table_findentry_internal(tab->entries, tab->nentries, key);
	if (index <= FIND_ENTRY_FAILED) {
		reporterrorf(S, -1, "'%s': internal error, _table_tryresize failed", tag2s[key.tag]);
	}
	return index;
}

//
//
//
//

static inline bool _table_contains(elf_State *S, Tab tab, V key) {
	Index s = _table_tryresize(S, tab, key);
	return s >= 0 && !isdead(tab->entries[s].key);
}

//
//
//
//


static inline void tablealias(elf_State *S, Tab tab, V key, V alias) {
	// todo: no need to resize!
	Index s = _table_tryresize(S, tab,key);

	if (s >= 0 && !isdead(tab->entries[s].key)) {
		Index c = _table_tryresize(S, tab, alias);
		tab->entries[c].key = alias;
		tab->entries[c].index = tab->entries[s].index;
		tab->fillcounter += 1;
	}
}

//
//
//
//

Value _table_getornil(elf_State *S, Tab tab, Value key) {
	Index index = _table_findentry_internal(tab->entries, tab->nentries, key);
	Value value = NIL_VALUE;

	if (index != FIND_ENTRY_FAILED) {

		IndexValue *entry = & tab->entries[index];

		if (iskey(entry->key)) {
			value = tab->array[entry->idx];
		}

	}
	return value;
}

//
//
//
//

static Index _table_getalways(elf_State *S, Tab table, Value key) {
	Index index = _table_tryresize(S, table, key);
	ASSERT(index != FIND_ENTRY_FAILED);

	Entry *entry = & table->entries[index];

	// there's nothing here, so use up the slot
	if (isdead(entry->key)) {

		index = heap_array_grow(table->array, 1);
		to_nil(&table->array[index]);

		entry->key = key;
		entry->index = index;
		table->fillcounter ++;
	}
	else {
		index = entry->index;
	}

	return index;
}

//
//
//
//
//
//
//
//

Index _table_bindtoindex(elf_State *S, Tab table, Value key, Index index) {
	checkwrite(S, (Ref) table);

	Index entry_index = _table_tryresize(S, table, key);
	ASSERT(entry_index != FIND_ENTRY_FAILED);

	Entry *entry = & table->entries[entry_index];

	if (isdead(entry->key)) {
		entry->key = key;
		entry->index = index;
		table->fillcounter ++;
	}
	else {
		index = entry->index;
	}

	return index;
}

//
//
//
//

Index tableset(elf_State *S, Tab table, Val key, Val value) {
	checkwrite(S, (Ref) table);

	Index index = _table_tryresize(S, table, key);
	ASSERT(index != FIND_ENTRY_FAILED);

	Entry *entry = table->entries + index;

	if (isdead(entry->key)) {
		index = heap_array_grow(table->array, 1);
		entry->key = key;
		entry->idx = index;
		table->fillcounter ++;
	}
	else {
		index = entry->idx;
	}

	table->array[index] = value;
	return index;
}

//
//
//
//
//
//
//
//

Index _table_arraylen(Tab tab) {
	return heap_array_length(tab->array);
}

//
//
//
//
//
//
//
//

Index _table_arrayadd(elf_State *S, Tab tab, V v) {
	checkwrite(S, (Ref) tab);
	Index index = heap_array_grow(tab->array, 1);
	tab->array[index] = v;
	return index;
}

//
//
//
//

Value _table_arrayget(elf_State *S, Tab tab, Index i) {
	Index l = heap_array_length(tab->array);

	if (i < 0 || i >= l) {
		reporterror(S, -1, "index out of bounds");
	}
	return tab->array[i];
}

//
//
//
//
//
//
//
//

static inline void _table_freeinternalmemory(Tab tab) {
	_table_clear(tab);
}

//
//
//
//
//
//
//
//

static void _table_markfieldreadonly(elf_State *S, Tab tab, Value field) {
	Index entryindex = _table_findentry_internal(tab->entries, tab->nentries, field);
	if (entryindex < 0) {
		reporterrorf(S, -1, "no such field!");
	}
	Value *key = & tab->entries[entryindex].key;
	key->status |= VALUE_READONLY;
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



