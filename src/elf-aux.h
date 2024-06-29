/*
** See Copyright Notice In elf.h
** elf-aux.h
** Auxiliary Functions
*/


void elf_debugger(char *message);


void elf_register_binding(elState *R, char *name, elBinding fn);
void elf_register_integer(elState *R, char *name, elInteger val);


elInteger elf_clocktime();
elNumber elf_timediffs(elInteger begin);
elNumber elf_timediffms(elInteger begin);


void elf_throw(elState *R, elByteId id, char *error);


int elf_fndfilebybyte(elModule *md, elByteId line);
void elf_getlinelocinfo(char *q, char *loc, int *linenum, char **lineloc);


void elf_tabmfld(elState *R, elTable *obj, char *name, elBinding b);






