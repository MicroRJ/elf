/*
** See Copyright Notice In elf.h
** elf-str.h
** String
*/


typedef struct elString {
	elObject  obj;
	elHashId hash;
	int     		length;
	union {
		char   string[1];
		char   c[1];
		char   contents[1];
	};
} elString;

elString *elf_new_lstring(elState *R, elInteger length);
elString *elf_new_string(elState *R, char *contents);

int elf_string_lib_length(elState *R);
int elf_string_lib_match(elState *R);
int elf_string_lib_pop(elState *R);
int elf_string_lib_append(elState *R);
int elf_string_lib_append_char(elState *R);
int elf_string_lib_get_hash(elState *R);
int elf_string_lib_uppercase(elState *R);
int elf_string_lib_lowercase(elState *R);

