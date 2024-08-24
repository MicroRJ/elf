/*
** See Copyright Notice Below.
** elf.h
** Basic types and forward declarations
*/


typedef struct elModule 	elModule;
typedef struct elState 		elState;
typedef struct elObject 	elObject;
typedef struct elTable 		elTable;
typedef struct elString 	elString;
typedef struct elClosure 	elClosure;
typedef struct elValue     elValue;


typedef long long int 	   elInteger;
typedef signed int 		   elBool;
typedef double 			   elNumber;
typedef void 			     *elHandle;
typedef void 			     *elAddr;
typedef int 					elError;
typedef unsigned int 	   elHashId;
typedef int 				   elRegId;
typedef int 				   elSymbolId;


typedef int (* elCFunction)(elState *);


typedef struct elCBinding {
	char *name;
	elCFunction fn;
} elCBinding;


typedef struct elFunction {
	short arity;
	short nvalues;
	short nlocals;
	elString     *name;
	elString *contents;
	int 	      nbytes;
	int 	       bytes;
	int       **protos;
	int         parent;
} elFunction;
