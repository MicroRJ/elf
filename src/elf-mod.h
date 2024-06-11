/*
** See Copyright Notice In elf.h
** elf-mod.h
** Module
*/


typedef struct elFileInfo {
	char *name;
	elByteId bytes;
	elByteId nbytes;
	int **protos;
	/* todo: eventually remove these */
	char *pathondisk;
	elf_lineid lines;
	int nlines;
} elFileInfo;


/*
** Symbols
** 	elf_Bytecode and globals can be added dynamically and
** safely, in fact, multiple files will reference the
** same global by name, no matter the order in which
** they were loaded, or the means, runtime/compiletime.
** This is because we use a symbol table that maps a
** name at compile time to an index in the global values.
** Even if a file is loaded at runtime, the compilation
** process finds the global symbol and maps it to the
** target index. Lookups are effectively done at compile
** time.
**
*/
typedef struct elModule {
	union { elTable *g, *globals; };

	elProto *p;
	elNumber *kn;
	elInteger *ki;
	int *track;
	elf_Bytecode *bytes;
	elByteId nbytes;
	char **lines;
	elFileInfo *files;
} elModule;


elSymbolID elf_get_global_symbol(elModule *md, elString *name);
elSymbolID lang_addglobal(elModule *md, elString *name, elValue v);
elSymbolID elf_add_proto(elModule *md, elProto p);

/*
	elModule\r: runtime is stored here
for garbage collection.
	elModule\gc: all objects to be automatically
managed, or garbage collected, are listed here.
By default all objects are added here, you
can however remove them from this array.

elModule\gf: buffer for functions definitions,
essentially a type table, anonymous functions
are also added here.

elModule\g: global symbol table which
maps names to values, indexed
at runtime by index.

elModule\bytes: buffer for bytes, all the bytes
are stored here, functions index into this
buffer.

*/