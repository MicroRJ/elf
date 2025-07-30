//
// See Copyright Notice In elf.h
//
// <3 stb
//

static inline elf_HashInt rehash(elf_HashInt hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}


static inline elf_HashInt hash_text(const char *text) {
	elf_HashInt hash;
	for (hash=2166136261u; *text; hash ^= *text++, hash *= 16777619);
	return hash;
}

static inline elf_HashInt hash64(elf_i64 i) {
	elf_HashInt hash = rehash(i);
	hash += hash << 16;
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return rehash(hash);
}
