/*
** See Copyright Notice In elf.h
** elf-sys.h
** System Tools
*/


elAPI void *sys_valloc(elInteger length);


elAPI void sys_consolelog(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
elAPI elBool sys_debugger();

elAPI elInteger sys_clockhz();
elAPI elInteger sys_clocktime();

elAPI int sys_getmyname(int length, char *buffer);
elAPI int sys_getmypid();

elAPI int sys_getworkdir(int length, char *buffer);
elAPI int sys_changeworkdir(char *buffer);

elAPI elHandle sys_loadlib(char const *name);
elAPI void *sys_libfn(elHandle lib, char const *name);

elAPI elError sys_load_file_text(elAllocator *allocator, void **lppOut, char const *fileName);
elAPI elError sys_savefilebytes(char const *buffer, elInteger length, char const *fileName);

elAPI int sys_getlasterror();
elAPI void sys_geterrormsg(int error, char *buff, int len);
