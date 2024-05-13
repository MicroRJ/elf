/*
** See Copyright Notice In elf.h
** elf-tab.c
** Table
*/


elf_Table *elf_newtabmetatab(elf_State *R) {
	elf_Table *tab = elf_pushnewtab(R);
	elf_tabmfld(R,tab,"length",elf_tablength_);
	elf_tabmfld(R,tab,"tally",elf_tabtally_);
	elf_tabmfld(R,tab,"haskey",elf_tabhaskey_);
	elf_tabmfld(R,tab,"lookup",elf_tablookup_);
	elf_tabmfld(R,tab,"iter",elf_tabforeach_);
	elf_tabmfld(R,tab,"collisions",elf_tabcollisions_);
	elf_tabmfld(R,tab,"unload",elf_tabunload_);
	elf_tabmfld(R,tab,"add",elf_tabadd_);
	elf_tabmfld(R,tab,"idx",elf_tabidx_);
	elf_tabmfld(R,tab,"xrem",elf_tabxrem_);
	elf_tabmfld(R,tab,"alias",elf_tabalias_);
	return tab;
}


elf_Table *elf_newtablen(elf_State *R, elf_int ntotal) {
	elf_Table *table = elf_newobj(R,OBJ_TAB,sizeof(elf_Table));
	if (R) table->obj.metatable = R->metatab_tab;

	table->ntotal = ntotal;
	table->nslots = 0;
	table->slots = elf_clearalloc(lHEAP,ntotal*sizeof(elf_tabslot));
	return table;
}


elf_Table *elf_newtab(elf_State *R) {
	return elf_newtablen(R,4);
}


void elf_deltab(elf_Table *tab) {
	elf_delmem(lHEAP,tab->slots);
	elf_delvar(tab->array);
	tab->array = 0;
	tab->slots = 0;
}


/*
** 	Traverses the table until it finds a match
** or a nil slot for this particular key.
**
** 	If the result is -1 it means no nil slot
** nor match found, this is most likely an
** error as it means the table has reached
** peak capacity.
**
** 	Then you have to check whether the slot
** is nil, which means no match, use the
** result to modify the slot and value as desired.
**
*/
elf_int elf_tabhashin(elf_Table *tab, elf_Value k) {
	elf_ensure(tab != lnil);
	/* this particular function uses double hashing,
	which should allow us to get more resolution out
	of the hash value, the first hash computes the
	starting index, and the secondary hash computes
	the step by which we increment.
	Since the increment depends on the data, it
	should reduce clustering, and in practice it
	has proven to be drastically more efficient
	than linear probing.
	Of course, this is already well known... */
	elf_tabslot *slots = tab->slots;
	elf_int ntotal = tab->ntotal;
	elf_int hash = elf_tabhashval(k);
	elf_int head = hash % ntotal;
	elf_int tail = head;
	elf_hashint walk = elf_tabrehash(hash)|1;
	do {
		elf_Value x = slots[tail].k;
		if (x.tag == TAG_NIL) return tail;
		if (elf_tabvaleq(&x,&k)) return tail;
		tail = (tail+walk) % ntotal;
		LDODEBUG( tab->ncollisions ++ );
	} while(head != tail);
	return -1;
}


elf_int elf_tabslot2index(elf_Table *table, elf_int slot) {
	return table->slots[slot].i;
}


elf_Value elf_tabslot2value(elf_Table *table, elf_int slot) {
	return table->v[table->slots[slot].i];
}


elf_bool elf_tabslotiskey(elf_Table *table, elf_int slot) {
	return slot >= 0 && table->slots[slot].k.tag != TAG_NIL;
}


void elf_tabslotsetkeyval(elf_Table *table, elf_int slot, elf_Value k, elf_int i) {
	table->slots[slot].k = k;
	table->slots[slot].i = i;
}


void elf_tabcheck(elf_Table *table) {
	if (table->ntotal * 3 < table->nslots * 4) {
		// LDODEBUG( table->ncollisions = 0 );
		/* todo:
		Find a better strategy for incrementing
		the table size >> 1 << 2 */
		elf_Table newtable = * table;
		newtable.ntotal = table->ntotal << 2;
		if (newtable.ntotal < table->ntotal) elf_unreachable;
		newtable.slots = elf_clearalloc(lHEAP,newtable.ntotal * sizeof(elf_tabslot));

		for (int i = 0; i < table->ntotal; ++ i) {
			elf_tabslot slot = table->slots[i];
			if (slot.k.tag == TAG_NIL) continue;

			elf_int newslot = elf_tabhashin(&newtable,slot.k);

			if (newslot == -1) elf_unreachable;

			newtable.slots[newslot] = slot;
		}

		elf_delmem(lHEAP,table->slots);

		table->ntotal = newtable.ntotal;
		table->slots = newtable.slots;
	}
}


void elf_tabset(elf_Table *table, elf_Value k, elf_Value v) {
	elf_tabcheck(table);
	elf_int slot = elf_tabhashin(table,k);
	/* todo: instead return an error here */
	if (slot == -1) elf_unreachable;
	elf_tabslot *entry = table->slots + slot;
	if (!elf_tabslotiskey(table,slot)) {
		elf_int i = elf_varaddi(table->v,1);
		table->v[i] = v;

		table->slots[slot].k = k;
		table->slots[slot].i = i;
		table->nslots ++;
	} else {
		table->v[entry->i] = v;
	}
}


elf_Value elf_tablookup(elf_Table *tab, elf_Value k) {
	elf_int slot = elf_tabhashin(tab,k);
	if (elf_tabslotiskey(tab,slot)) {
		return elf_tabslot2value(tab,slot);
	}
	return (elf_Value){TAG_NIL,0};
}


elf_int elf_tabtake(elf_Table *table, elf_Value k) {
	elf_ensure((k.tag == TAG_INT || k.tag == TAG_NUM) || k.s != 0);

	elf_tabcheck(table);
	elf_int slot = elf_tabhashin(table,k);
	if (slot == -1) elf_unreachable;
	if (!elf_tabslotiskey(table,slot)) {
		elf_int i = elf_varaddi(table->v,1);
		table->v[i] = (elf_Value){TAG_NIL};

		table->slots[slot].k = k;
		table->slots[slot].i = i;
		table->nslots ++;
	}
	return elf_tabslot2index(table,slot);
}



void elf_tabalias(elf_State *S, elf_Table *tab, elf_Value key, elf_Value alias) {
	elf_tabcheck(tab);
	elf_int keyslot = elf_tabhashin(tab,key);
	if (elf_tabslotiskey(tab,keyslot)) {
		elf_int aliasslot = elf_tabhashin(tab,alias);
		tab->slots[aliasslot].k = alias;
		tab->slots[aliasslot].i = tab->slots[keyslot].i;
	} else elf_throw(S,NO_BYTE,"attempted to alias a key that was never added");
}


void elf_tabstralias(elf_State *S, elf_Table *tab, char *key, elf_Value alias) {
	return elf_tabalias(S,tab,elf_valstr(elf_newstr(S,key)),alias);
}


elf_Value elf_tabgetfld(elf_Table *tab, elf_String *key) {
	return elf_tablookup(tab,elf_valstr(key));
}


elf_num elf_tabgetnum(elf_Table *tab, elf_String *key) {
	elf_Value val = elf_tablookup(tab,elf_valstr(key));
	return elf_tonum(val);
}


elf_int elf_tabgetint(elf_Table *tab, elf_String *key) {
	elf_Value val = elf_tablookup(tab,elf_valstr(key));
	return elf_toint(val);
}


elf_String *elf_tabgetstr(elf_Table *tab, elf_String *key) {
	return elf_tablookup(tab,elf_valstr(key)).x_str;
}


elf_Table *elf_tabgettab(elf_Table *tab, elf_String *key) {
	return elf_tablookup(tab,elf_valstr(key)).x_tab;
}


elf_int elf_tabiadd(elf_Table *table, elf_Value v) {
	return elf_varaddi(table->v,1);
}


void elf_tabadd(elf_Table *table, elf_Value v) {
	elf_varadd(table->v,v);
}


void elf_tabsetstrfld(elf_Table *tab, elf_String *key, elf_String *val) {
	elf_tabset(tab,elf_valstr(key),elf_valstr(val));
}


void elf_tabsetintfld(elf_Table *tab, elf_String *key, elf_int val) {
	elf_tabset(tab,elf_valstr(key),elf_valint(val));
}


void elf_tabsetnumfld(elf_Table *tab, elf_String *key, elf_num val) {
	elf_tabset(tab,elf_valstr(key),elf_valnum(val));
}


void elf_tabsettabfld(elf_Table *tab, elf_String *key, elf_Table *val) {
	elf_tabset(tab,elf_valstr(key),elf_valtab(val));
}


/* metatable */


int elf_tablength_(elf_State *R) {
	elf_Table *tab = (elf_Table*) elf_getthis(R);
	elf_pushint(R,elf_varlen(tab->v));
	return 1;
}


int elf_tabtally_(elf_State *R) {
	elf_Table *tab = (elf_Table*) elf_getthis(R);
	elf_pushint(R,elf_varlen(tab->v));
	return 1;
}


int elf_tabhaskey_(elf_State *c) {
	elf_ensure(c->f->x == 1);
	elf_Table *table = (elf_Table*) elf_getthis(c);
	elf_Value k = elf_getany(c,0);
	elf_pushint(c,elf_tabslotiskey(table,elf_tabhashin(table,k)));
	return 1;
}


int elf_tablookup_(elf_State *c) {
	elf_ensure(c->f->x == 1);
	elf_Value k = elf_getany(c,0);
	elf_Table *table = (elf_Table*) c->f->obj;
	elf_pushany(c,elf_tablookup(table,k));
	return 1;
}


int elf_tabcollisions_(elf_State *c) {
	elf_Table *table = (elf_Table*) c->f->obj;
	elf_pushint(c,table->ncollisions);
	return 1;
}


int elf_tabadd_(elf_State *R) {
	elf_ensure(R->call->x >= 1);
	elf_Table *tab = (elf_Table *) R->call->obj;
	elf_tabadd(tab,elf_getany(R,0));
	return 0;
}


int elf_tabidx_(elf_State *R) {
	elf_ensure(R->call->nx >= 1);
	elf_Table *tab = (elf_Table *) elf_getthis(R);
	elf_int len = elf_varlen(tab->array);
	if (len != 0) {
		elf_int idx = elf_getint(R,0) % len;
		elf_pushany(R,tab->array[idx]);
	} else elf_pushnil(R);
	return 1;
}


int elf_tabxrem_(elf_State *R) {
	elf_ensure(R->call->x >= 1);
	elf_Table *tab = (elf_Table *)elf_getthis(R);
	elf_int min = elf_vardec(tab->array);
	elf_int idx = elf_getint(R,0);
	if (idx != min) {
		tab->array[idx] = tab->array[min];
	}
	return 0;
}


int elf_tabalias_(elf_State *R) {
	elf_checkargs(R,":alias",2,"(key of any, alias of any) -> none, adds a new entry to the table (alias) that points to where (key) points");
	elf_Table *tab = (elf_Table *) elf_getthis(R);
	elf_tabalias(R,tab,elf_getany(R,0),elf_getany(R,1));
	return 0;
}



int elf_tabforeach_(elf_State *R) {
	elf_ensure(R->frame->x == 1);
	elf_Table *table = (elf_Table *) R->frame->obj;
	elf_checkcl(R,0);
	elf_localid k = elf_pushmany(R,1);
	elf_localid v = elf_pushmany(R,1);
	for (int i = 0; i < table->ntotal; ++ i) {
		elf_tabslot slot = table->slots[i];
		if (slot.k.tag != TAG_NIL) {
			R->stk[k] = slot.k;
			R->stk[v] = table->v[slot.i];
			/* call the iterator function with two arguments */
			/* todo: should yield boolean to signal whether to
			stop or not */
			elf_callex(R,R->frame->obj,0,0,2,0);
		}
	}
	return 0;
}


void ftabs(FILE *io, int level) {
	while (level --) fprintf(io,"\t");
}
void elf_tabunload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	for (int i = 0; i < tab->ntotal; ++ i) {
		elf_tabslot slot = tab->slots[i];
		if (slot.k.tag != TAG_NIL) {
			if (nitems ++ != 0) fprintf(io,",");
			elf_valfpf(io,slot.k,ltrue);
			fprintf(io," = ");
			elf_Value v = tab->v[slot.i];
			if (v.tag == TAG_TAB) {
				elf_tabunload(io,v.t,level+1);
			} else {
				elf_valfpf(io,v,ltrue);
			}
		}
	}
	fprintf(io,"}");
}


int elf_tabunload_(elf_State *R) {
	elf_Handle io = elf_getsys(R,0);
	elf_tabunload(io,(elf_Table*)elf_getthis(R),0);
	return 0;
}


/* Some of the hash functions and comments
were borrowed from the great Sean Barrett */
elf_hashint elf_tabrehash(elf_hashint hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}


#if 1
// FNV-1a
elf_hashint elf_tabhashstr (char *bytes) {
	elf_hashint hash = 2166136261u;
	while (*bytes) {
		hash ^= *bytes++;
		hash *= 16777619;
	}
	return hash;
}
#else
elf_hashint elf_tabhashstr(char *bytes) {
	elf_hashint hash = 0;
	while (*bytes) {
		hash = (hash << 7) + (hash >> 25) + *bytes++;
	}
	return hash + (hash >> 16);
}
#endif


elf_hashint elf_tabhashptr(Ptr *p) {
   // typically lacking in low bits and high bits
	elf_hashint hash = elf_tabrehash((elf_hashint)(elf_int)p);
	hash += hash << 16;

   // pearson's shuffle
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return elf_tabrehash(hash);
}


elf_bool elf_tabvaleq(elf_Value *x, elf_Value *y) {
	if (x->tag != y->tag) {
		return lfalse;
	}
	switch (x->tag) {
		case TAG_STR: {
			return elf_streq(x->s,y->s);
		}
		case TAG_SYS: case TAG_INT: case TAG_NUM:
		case TAG_TAB: case TAG_CLS: case TAG_BID: {
			return x->i == y->i;
		}
		default: elf_unreachable;
	}
	return lfalse;
}


elf_int elf_tabhashval(elf_Value v) {
	switch (v.tag) {
		case TAG_STR: {
			elf_ensure(v.x_str != lnil);
			return v.x_str->hash;
		}
		case TAG_TAB: case TAG_CLS: case TAG_SYS:
		case TAG_INT: case TAG_NUM: case TAG_BID: {
			return elf_tabhashptr(v.p);
		}
		default: elf_unreachable;
	}
	return lfalse;
}

