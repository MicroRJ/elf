//
// See Copyright Notice In elf.h
//




static inline Hash rehash(Hash hash) {
	return ((hash) + ((hash) >> 6) + ((hash) >> 19));
}




static inline Hash hash_textl(const char *text, int size) {
	Hash hash;
	for (hash=2166136261u; size; hash ^= *text++, hash *= 16777619, size --);
	return hash;
}




static inline Hash hash64(Int i) {
	Hash  hash = rehash(i);
	hash += hash << 16;
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 2;
	hash += hash >> 15;
	hash ^= hash << 10;
	return rehash(hash);
}

