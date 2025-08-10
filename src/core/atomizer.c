//
// See Copyright Notice In elf.h
//

static elf_String *elf_atomize(elf_State *inter, char *text, int length) {
	elf_String *result = 0;
	elf_Table *registry = R->strings;
	hash_t hash = hash_text(text);

	if (length < 64 && registry != 0) {
		elf_Integer slot = elf_table_try_text(registry,text,length,hash);
		ASSERT(slot != -1);
		elf_Table_Entry entry = registry->slots[slot];
		if (entry.key.tag != ELF_TNIL) {
			elf_Value target = registry->array[registry->slots[slot].idx];
			string = target.x_str;
		} else {
			string = elf_alloc_string2(R,length);
			copy_memory(string->text,text,length);
			string->hash = hash;

			elf_Integer i = darr_grow(registry->array,1);
			registry->array[i]=VALUE_STRING(string);
			registry->slots[slot].key=VALUE_STRING(string);
			registry->slots[slot].idx=i;
			registry->nslots ++;
		}
	} else {
		string = elf_alloc_string2(R,length);
		copy_memory(string->text,text,length);
		string->hash = hash;
	}
}
