//
// See Copyright Notice In elf.h
//





static int marktablereachable(elf_State *S, Tab tab) {
	Index i, tally=0;
	for(i=0; i<tab->nentries; ++i) {
		if (is_ref(tab->entries[i].key)) {
			tally += markreachable(S, as_ref(tab->entries[i].key));
		}
	}
	for(i=0; i<heap_array_length(tab->array); ++i) {
		if (is_ref(tab->array[i])) {
			tally += markreachable(S, as_ref(tab->array[i]));
		}
	}
	return tally;
}



static int markreachable(elf_State *S, GCRef ref) {
	ASSERT(ref != 0);

	int counter = 0;

	// mark reachable internal, which doesn't do this check,
	// instead internal macro which does the check!
	if (~ref->status & NODE_REACHABLE) {
		ref->status |= NODE_REACHABLE;

		counter = 1;

		if (ref->meta) {
			counter += markreachable(S, (Ref) ref->meta);
		}

		if (ref->type == GC_CLS) {
			Closure closure = (Closure) ref;

			for (int i=0; i<closure->proto.ncaptures; ++i)
			{
				if (is_ref(closure->captures[i])) {
					counter += markreachable(S, as_ref(closure->captures[i]));
				}
			}
		}
		else if (ref->type == GC_TAB) {
			counter += marktablereachable(S, (Tab) ref);
		}
	}
	return counter;
}





static int marktablereadonly(elf_State *S, Tab tab) {
	Index i, tally=0;
	for(i=0; i<tab->nentries; ++i) {
		if (is_ref(tab->entries[i].key)) {
			tally += markreadonly(S, as_ref(tab->entries[i].key));
		}
	}
	for(i=0; i<heap_array_length(tab->array); ++i) {
		if (is_ref(tab->array[i])) {
			tally += markreadonly(S, as_ref(tab->array[i]));
		}
	}
	return tally;
}




static int markreadonly(elf_State *S, Ref ref) {
	ASSERT(ref);

	int counter = 0;
	if (~ref->status & NODE_READONLY) {
		ref->status |= NODE_READONLY;

		counter += 1;
		ref->status |= NODE_READONLY;
		if (ref->type == GC_TAB) {
			counter += marktablereadonly(S, (Tab) ref);
		}
	}
	return counter;
}

