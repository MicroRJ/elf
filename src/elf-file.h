/*
** See Copyright Notice In elf.h
** elf-file.h
** Parsing Structures
*/


#define NO_ENTITY (lentityid){-1}

#define NOTANENTITY 0x01


typedef struct { int x; } lentityid;


/* for scoping, binds a name to
some local value such as a label,
enum or local variable. */
typedef struct elf_fileentry {
	char    *name;
	elf_lineid  line;
	elf_localid slot;
	elBool enm;
	/* the level in which this name was
	declared, for scoping */
	int     level;
	/* todo: we could just put the node here */
} elf_fileentry;



typedef struct elf_usingstack elf_usingstack;
typedef struct elf_usingstack {
	elf_usingstack *enclosing;
	char *name;
} elf_usingstack;



typedef struct elFileFnState elFileFnState;
typedef struct elFileFnState {
	elFileFnState *enclosing;
	elf_lineid line;
	/* for the basic register allocation system, where we have
	an infinite number of register, but we still want to keep
	the number of registers at a minimum, we resort to using
	two counters, nlocals and xmemory */
	/* maximum number of local register used concurrently
	at any point for this function */
	elf_localid nlocals;
	/* the memory state, in other words, the current number
	of local registers that are being used at this point. */
	elf_localid xmemory;
	/* index to first entity within entity list in file. */
	int entities;
	/* array of entities from enclosing function
	that are to be cached */
	lentityid *captures;
	/* this is needed to emit instructions
	relative to the current function we're
	parsing, there's always an active function,
	even at file level */
	elf_byteid bytes;
	elf_fileblock entry;
	elf_fileblock *block;
	elf_usingstack *usingstack;
	int nloops;
	int nblocks;
	int nyield;
	/* todo: deprecated */
	/* list of yield jumps to be patched */
	elf_byteid *yj;
} elFileFnState;


typedef struct elFileState {

	union { elModule  *M,*md; };
	union { elState *R,*rt; };

	char *filename;
	char *linechar;
	char *contents;
	char *thischar;
	int linenumber;
	elToken lasttk,tk,thentk;
	/* buffer for nodes */
	elNode *nodes;
	elNodeID nnodes;
	/* the current level, level is incremented
	per level, block or statement or whenever
	it makes sense, represents a visibility
	layer, 0 is for file. */
	int level;
	/* -- hierarchical list of local tags for the current
	- function stack, each function points to a base local
	- tag defined here by index.
	-- remaining locals are file locals, at file level. */
	elf_fileentry *entities;
	int nentities;

	elf_fileblock entry;
	/* hierarchical list of loading functions,
	each allocated in C stack by caller function */
	elFileFnState *fn;
	elf_byteid bytes;
	int flags;
	elBool debuggerflag;
} elFileState;


elNodeID elf_fsloadexpr(elFileState *fs);
elNodeID elf_fsloadunary(elFileState *fs);
void elf_fsloadstat(elFileState *fs);
