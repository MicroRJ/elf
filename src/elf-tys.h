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
typedef int elRegId;
typedef int elByteId;
typedef int elSymbolId;
/* todo: eventually convert this to an offset */
typedef char *elFileLine;


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



