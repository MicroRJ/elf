//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_TABLE_H
#define ELF_CORE_TABLE_H

static inline b32 entry_is_key(Entry entry)
{
	return entry.key.type != ELF_VALUE_TYPE_NIL && entry.key.type != ENTRY_TOMB_TYPE;
}

static inline elf_Value entry_key_value(Entry entry)
{
	return entry.key;
}

#endif
