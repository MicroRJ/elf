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


int elf_find_file_info_by_byte(elModule *md, elByteId line);
void elf_get_line_location_info(char *q, char *loc, int *linenum, char **lineloc);
elf_lineid elf_get_line_for_byte(elModule *M, elByteId byte);


void elf_table_set_binding_field(elState *R, elTable *obj, char *name, elBinding b);






