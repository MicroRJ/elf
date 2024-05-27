/*
** See Copyright Notice In elf.h
** elf-tys.h
** Type Definitions
*/


typedef long long int elInteger;
typedef signed int elBool;
typedef double elNumber;
typedef void *elHandle;
typedef void *elAddr;
typedef int (* elBinding)(elState *);


typedef unsigned int elf_hashint;
typedef int elf_localid;
typedef int elf_byteid;
typedef int elf_globalid;
/* todo: eventually convert this to an offset */
typedef char *elf_lineid;


/*
** Function Prototype
*/
typedef struct elProto {
	short x,y;
	short zcache;
	short zstack;
	int nbytes;
	int bytes;
} elProto;



