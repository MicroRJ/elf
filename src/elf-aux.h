/*
** See Copyright Notice In elf.h
** elf-aux.h
** Auxiliary Functions
*/


void elf_debugger(char *message);


void elf_register_binding(elState *R, char *name, elCFunction fn);
void elf_register_integer(elState *R, char *name, elInteger val);


elInteger elf_clocktime();
elNumber elf_timediffs(elInteger begin);
elNumber elf_timediffms(elInteger begin);


void elf_Rthrow(elState *R, elByteId id, char *error);


int elf_get_file_for_byte(elModule *md, elByteId line);
void elf_get_line_location_info(char *q, char *loc, int *linenum, char **lineloc);
elFileline elf_get_line_for_byte(elModule *M, elByteId byte);







