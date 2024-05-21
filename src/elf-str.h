/*
** See Copyright Notice In elf.h
** elf-str.h
** String
*/


typedef struct elString {
	elObject  obj;
	elf_hashint hash;
	int     		length;
	union {
		char   string[1];
		char   c[1];
	};
} elString;


elString *elf_newstrlen(elState *R, elInteger length);
elString *elf_newstr(elState *R, char *contents);
elTable *elf_newstrmetatab(elState *R);


int elfstr_length_(elState *R);
int elfstr_match_(elState *R);
int elfstr_append_(elState *R);
int elfstr_gethash_(elState *R);
int elfstr_touppercase_(elState *R);
int elfstr_tolowercase_(elState *R);

