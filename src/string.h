/*
** See Copyright Notice In elf.h
** string.h
** String
*/


/* length can probably go away or be much smaller,
we only really use length for smaller strings, same
for hash... */
typedef struct elString {
	elObject    obj;
	elHashId   hash;
	int     	length;
	union {
		char   text[1];
		char   string[1];
	};
} elString;


elAPI elString *elf_new_lstring(elState *R, elInteger length);
elAPI elString *elf_new_string(elState *R, const char *text);

elAPI int elf_str_get_length(elString *);
elAPI elHashId elf_str_get_hash(elString *);
elAPI char *elf_str_get_text(elString *);


elBool elf_string_eq(elString *x, elString *y);


elAPI int elf_slib_length(elState *R);
elAPI int elf_slib_match(elState *R);
elAPI int elf_slib_pop(elState *R);
elAPI int elf_slib_append(elState *R);
elAPI int elf_slib_append_char(elState *R);
elAPI int elf_slib_get_hash(elState *R);
elAPI int elf_slib_uppercase(elState *R);
elAPI int elf_slib_lowercase(elState *R);
elAPI int elf_slib_split_by_lines(elState *R);
elAPI int elf_slib_get_index(elState *R);
elAPI int elf_slib_find(elState *R);
elAPI int elf_slib_split(elState *R);

