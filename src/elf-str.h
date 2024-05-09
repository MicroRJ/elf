/*
** See Copyright Notice In elf.h
** elf-str.h
** String
*/


typedef struct elf_String {
	elf_Object  obj;
	elf_hashint hash;
	int     		length;
	union {
		char   string[1];
		char   c[1];
	};
} elf_String;


elf_String *elf_newstrlen(elf_State *R, elf_int length);
elf_String *elf_newstr(elf_State *R, char *contents);
elf_Table *elf_newstrmetatab(elf_State *R);


int elfstr_length_(elf_State *R);
int elfstr_match_(elf_State *R);
int elfstr_append_(elf_State *R);
int elfstr_gethash_(elf_State *R);
int elfstr_touppercase_(elf_State *R);
int elfstr_tolowercase_(elf_State *R);

