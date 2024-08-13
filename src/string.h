/*
** See Copyright Notice In elf.h
** string.h
** String
*/


typedef struct elString {
	elObject    obj;
	elHashId   hash;
	/* Hear me out... do you even	use the length of
	the string that often, and when you do use it,
	you cache it somewhere, if you really want to
	compute the length of a string without using
	strlen (like when you're looking up a string),
	you can use the size of the object minus the
	size of the string header... */
	int     	length;
	union {
		char   text[1];
		/* TODO: DEPRECATED */
		// char   contents[1];
		char   string[1];
		char   c[1];
	};
} elString;


elString *elf_new_lstring(elState *R, elInteger length);
elString *elf_new_string(elState *R, char *contents);


int elf_libS_length(elState *R);
int elf_libS_match(elState *R);
int elf_libS_pop(elState *R);
int elf_libS_append(elState *R);
int elf_libS_append_char(elState *R);
int elf_libS_get_hash(elState *R);
int elf_libS_uppercase(elState *R);
int elf_libS_lowercase(elState *R);
int elf_libS_split_by_lines(elState *R);
int elf_libS_get_index(elState *R);
int elf_libS_find(elState *R);
int elf_libS_split(elState *R);


elGLOBAL elBinding elf_libS_[] = {
	{"length",elf_libS_length},
	{"match",elf_libS_match},
	{"uppercase",elf_libS_uppercase},
	{"lowercase",elf_libS_lowercase},
	{"__add",elf_libS_append},
	{"__add1",elf_libS_append},
	{"append",elf_libS_append},
	{"append_char",elf_libS_append_char},
	{"pop",elf_libS_pop},
	{"get_hash",elf_libS_get_hash},
	{"split_by_lines",elf_libS_split_by_lines},
	{"idx",elf_libS_get_index},
	{"find",elf_libS_find},
};



