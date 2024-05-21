/*
** See Copyright Notice In elf.h
** elf-sys.h
** System Tools
*/


elf_api void *sys_valloc(elInteger length);


elf_api void sys_consolelog(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
elf_api elBool sys_debugger();

elf_api elInteger sys_clockhz();
elf_api elInteger sys_clocktime();

elf_api int sys_getmyname(int length, char *buffer);
elf_api int sys_getmypid();

elf_api int sys_pwd(int length, char *buffer);
elf_api int sys_setpwd(char *buffer);

elf_api elHandle sys_loadlib(char const *name);
elf_api void *sys_libfn(elHandle lib, char const *name);

elf_api Error sys_loadfilebytes(Alloc *allocator, void **lppOut, char const *fileName);
elf_api Error sys_savefilebytes(char const *buffer, elInteger length, char const *fileName);

elf_api int sys_getlasterror();
elf_api void sys_geterrormsg(int error, char *buff, int len);
