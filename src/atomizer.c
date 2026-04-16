//
// See Copyright Notice In elf.h
//

//
// todo: we're not even using this...
// We need a dedicated string map...
//
static elf_String *elf_atomize(elf_State *inter, char *text, int length) {
	elf_String *result = 0;
	elf_Table *registry = R->strings;
	elf_Hash hash = hash_text(text);

	if (length < 64 && registry != 0) {
		elf_Integer slot = elf_table_try_text(registry,text,length,hash);
		ASSERT(slot != -1);
		IndexValue entry = registry->slots[slot];
		if (entry.key.tag != ELF_VALUE_TYPE_NIL) {
			elf_Value target = registry->array[registry->slots[slot].idx];
			string = target.x_str;
		} else {
			string = new_empty_string(R,length);
			copy_memory(string->text,text,length);
			string->hash = hash;

			elf_Integer i = dynamic_array_allocate(registry->array,1);
			registry->array[i]=VALUE_STRING(string);
			registry->slots[slot].key=VALUE_STRING(string);
			registry->slots[slot].idx=i;
			registry->nslots ++;
		}
	} else {
		string = new_empty_string(R,length);
		copy_memory(string->text,text,length);
		string->hash = hash;
	}
}
