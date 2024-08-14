/*
** See Copyright Notice In elf.h
** file.h
** Parsing And Code Generation...
*/


typedef struct elToken {
	unsigned char 	type;
	char *line;
	unsigned int eol: 1;
	union {
		elInteger integer;
		elNumber   number;
		char 		   *text;
	};
} elToken;


#define KWLIST(_) \
_(ELF,"elf") _(DEFAULT,"default") _(LOAD,"load") _(NEW,"new") _(FUN,"fun") \
_(NIL,"nil") _(TRUE,"true") _(FALSE,"false") \
_(ENUM,"enum") \
_(TRY,"try") _(CATCH,"catch") _(FINALLY,"finally") \
_(DO,"do") _(WHILE,"while") _(BREAK,"break") _(CONTINUE,"continue") \
_(LET,"let") _(FOR,"for") _(LASTLY,"lastly") _(LEAVE,"leave") \
_(IF,"if") _(IFF,"iff") _(ELSE,"else") _(ELIF,"elif") _(THEN,"then") \


#define MCLIST(_) \
_(LINE_NUMBER,"line_number") _(LINE_CHAR,"line_char") _(FILE_NAME,"file_name") \
_(INT,"int") _(NUM,"num") \
_(LEVEL,"level") _(REGISTER,"register") \
_(INDEX,"index") _(VALUE,"value") _(ARRAY,"array") _(FIELD,"field") _(ENDOFFILE,"eof")


/* todo: why would !! and ?? have lower precedence
than relational operators, when !! and ?? work on
values */
#define OPLIST(_) \
_(POW,"**",12) \
_(MUL,"*",11) _(DIV,"/",11) _(MOD,"%",11) \
_(ADD,"+",10) _(SUB,"-",10) \
_(SHR,">>",9) _(SHL,"<<",9) \
_(LT,"<", 8) _(LTEQ,"<=", 8) \
_(GT,">", 8) _(GTEQ,">=", 8) \
_(EQ, "==", 7) _(NEQ , "!=", 7) \
_(BIT_AND,"&",6) _(BIT_OR,"|",5) _(BIT_XOR,"^",4) \
_(LOG_AND,"&&",3) _(LOG_OR,"||",2) \
_(NIL_AND,"!!",3) _(NIL_OR,"??",2) \
_(ELLIPSIS,"...",1) _(DOT_DOT,"..",1)


#define TKLIST(_) \
_(INTEGER,"integer") _(NUMBER,"number") _(STRING,"string") _(LETTER,"letter") _(WORD,"word") \
_(QMARK,"?") _(EXCLAMATION_MARK,"!") \
_(ASSIGN,"=") _(NIL_ASSIGN,"?=") \
_(COLON,":") _(SEMI_COLON,";") \
_(COMMA,",") _(DOT,".") \
_(SQUARE_LEFT,"[") _(SQUARE_RIGHT,"]") _(CURLY_LEFT,"{") _(CURLY_RIGHT,"}") \
_(PAREN_LEFT,"(") _(PAREN_RIGHT,")") \


typedef enum elTokenType {
	TK_NONE = 0,

#define TKITEM(NAME,_) elFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) elFUSE(TK_,NAME),
#define MCITEM(NAME,_) elFUSE(TK_M_,NAME),

	KWLIST(TKITEM)
	MCLIST(MCITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)

#undef TKITEM
#undef MCITEM
#undef OPITEM
} elTokenType;


#define NO_NODE (-1)

#define SPECIAL_REGISTER_THIS   (     0) // #this
#define SPECIAL_REGISTER_INDEX  (0xff+1) // #index
#define SPECIAL_REGISTER_VALUE  (0xff+2) // #value
#define SPECIAL_REGISTER_ARRAY  (0xff+3) // #array


typedef int elNodeId;

/* this is silly */
typedef struct {
	elNodeId id;
} elNodeIdTypeGuard;


#define MAKE_NODE_ID(id) (elNodeIdTypeGuard){id}

typedef enum elNodeTy {
	NT_NON = 0,
	NT_ANY, NT_SYS,
	NT_NIL, NT_BOL, NT_INT, NT_NUM,
	NT_OBJ, NT_TAB, NT_FUN, NT_STR
} elNodeTy;


// INDEX: {x}[{y}]
// FIELD: {x}.{y}
// METAFIELD: {x}:{y}
// CALL: {x}({y})
// RANGE_INDEX: [{x}...{y}]
// RANGE: {x}...{y}
// GROUP: ({x})
/* Nodes AND through GTEQ come in pairs,
use ^ to get the counter instruction.
AND should be an even number so that this
works... */
#define NODELIST(_) \
_(NOP)\
_(AND) _(OR)\
_(NIL_AND) _(NIL_OR)\
_(EQ) _(NEQ)\
_(BIT_SHL) _(BIT_SHR)\
_(ADD) _(SUB) _(MUL) _(DIV)\
_(LT) _(GT) _(LTEQ) _(GTEQ)\
_(INDEX) _(FIELD)\
_(BIT_AND) _(BIT_OR) _(BIT_XOR)\
_(MOD) _(POW)\
_(TYPEGUARD)\
_(LOAD)\
_(CLOSURE) _(STRING) _(TABLE) \
_(INTEGER) _(NUMBER) _(NIL) \
_(GLOBAL) _(LOCAL) _(CLSVAL) _(FILE_VALUE) \
_(MULTI)\
_(METAFIELD)\
_(CALL)\
_(RANGE_INDEX)\
_(RANGE)\
_(GROUP)\
_(REGION)


#define NODE_ENUM(NAME) NODE_##NAME,
typedef enum elNodeKi {
	NODE_NONE = 0,
	NODELIST(NODE_ENUM)
} elNodeKi;
#undef NODE_ENUM


elGLOBAL char *node2s[] = {
	"NONE",
#define NODE_ENUM(NAME) #NAME,
	NODELIST(NODE_ENUM)
#undef NODE_ENUM
};


/* todo: make this more compact! */
typedef struct elNode {
	union { elNodeKi kind, ki, k; };
	union { elNodeTy type, ty, t; };
	elFileline line;
	/* todo: eventually remove this */
	int level;

	union {
		struct { elNodeId x,y,*z; };
		union {
			char      *s;
			elInteger  i;
			elNumber   n;
		} lit;
	};
} elNode;


elNodeId elf_fnodexyz(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y, elNodeId *z);
elNodeId elf_nodexy(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y);
elNodeId elf_nodex(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x);

elNodeId elf_nnil(elFileState *fs, elFileline);
elNodeId elf_node_integer(elFileState *fs, elFileline, elInteger i);
elNodeId elf_nnumber(elFileState *fs, elFileline, elNumber n);
elNodeId elf_node_string(elFileState *fs, elFileline, char *);

elNodeId elf_make_nullary_node(elFileState *fs, elFileline, elNodeKi k, elNodeTy t);
elNodeId elf_make_group_node(elFileState *fs, elFileline, elNodeId x);


elNodeId elf_node_newtable(elFileState *fs, elFileline, elNodeId *z);
elNodeId elf_make_closure_node(elFileState *fs, elFileline, elNodeId x, elNodeId *z);
elNodeId elf_node_store(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_node_local(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_fnodeglobal(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_nclosevalue(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_ntypeguard(elFileState *fs, elFileline line, elNodeId x, elNodeTy y);
elNodeId elf_node_metafield(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_node_field(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_node_index(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_make_ranged_index_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_node_call(elFileState *fs, elFileline line, elNodeId x, elNodeId *z);
elNodeId elf_ncallmetafield(elFileState *fs, elFileline line, elNodeId x, elNodeId *z, char *name);
elNodeId elf_make_node_less_than(elFileState *fs, elFileline line, elNodeId x, elNodeId y);



#define EXPR_LHS     				0x01
#define EXPR_ALLOW_POSTFIX     	0x02


/* Entity: high-level data structure used for
lexical scoping, binds a name to some
value or compile time thing. */

#define NO_ENTITY -1

typedef int elEntityId;
typedef int elBlockId;


typedef struct {
	elEntityId id;
} elEntityIdTypeGuard;


#define ENTITY_ID(X) elLITERAL(elEntityIdTypeGuard){X}


#define ENTITY_REFERENCED (1 << 0)
#define ENTITY_CONSTANT   (1 << 1)
#define ENTITY_ASSIGNED   (1 << 2)
#define ENTITY_PARAMETER  (1 << 3)
#define ENTITY_FORLOOP    (1 << 4)



typedef enum elEntityKind {
	ENTITY_INVALID = 0,
	ENTITY_DIRECTORY,
	ENTITY_LOCAL,
	ENTITY_GLOBAL,
} elEntityKind;


typedef struct elFileEntity {
	elEntityKind kind;
	int   flags;
	char  *name;
	char  *line;
	int     reg;
	int   level;
} elFileEntity;


/* todo: remove this */
#define LOAD_RELOAD 		0x1
#define LOAD_ALLOCATE 	0x2

typedef struct elFileLoopState {
	elByteId entry;
	elByteId *false_jumps,*true_jumps;
	/* these can be directly accessed using #array, #index,
	and #value.
	#array and #index are guaranteed to be register nodes,
	and #value is an (index node), which translates to
	#array[#index] */
	elNodeId array_register; // _register;
	elNodeId index_register; // _register;
	elNodeId value_register;
	/* todo: why do we need this, please
	remove? */
	union { elNodeId x; };
} elFileLoopState;


#define BLOCK_LOOP 		0x01
#define BLOCK_ENDED  	0x02
#define BLOCK_DELAYED 	0x04


typedef struct elFileBlock elFileBlock;
typedef struct elFileBlock {
	elBool flags;
	int level;
	int xmemory;
	int nlocals;
	int xentity;
	int xnode;
	elByteId entry;
	elByteId jumpover;
	elByteId *leavejumps;
	elFileLoopState loop;
} elFileBlock;



typedef struct elFileExpr {
	int kind;
	union {
		struct {
			elByteId *t,*f;
			int x,y,*z;
		};
		elNumber number;
		elInteger integer;
		char *string;
	};
} elFileExpr;


/* Contains list of jumps generated
by a select statement, such as if else */
typedef struct elSelectState {
	/* Conditional false jump instructions
	to be patched so that they jump to
	the next block or instruction */
	elByteId *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	elByteId *j;
} elSelectState;



#define NO_SLOT (-1)
#define NO_BYTE (-1)
#define NO_JUMP (-0)
#define NO_LINE (-0)


typedef struct elFileFnState elFileFnState;
typedef struct elFileFnState {
	elFileFnState *enclosing;
	char               *line;
	int              nlocals;
	int              xmemory;
	elEntityId    *enclosure;
	elEntityId      entities;
	elBlockId    entry_block;
	elByteId           bytes;
	elByteId          nbytes;
	int               nyield;
	elByteId             *yj;
} elFileFnState;


typedef struct elFileState {
	elFileFnState state;
	elModule         *M;
	elState          *R;
	char     *filename;
	char     *filetext;
	char     *linechar;
	char     *thischar;
	int     linenumber;
	union {
		struct {
			elToken lasttk,tk,thentk;
		};
		struct {
			elToken last_token,this_token,then_token;
		};
	};
	elNode 	       *nodes;
	elNodeId        nnodes;
	elFileEntity *entities;
	elEntityId   nentities;
	elFileBlock    *blocks;
	elBlockId      nblocks;
	int             nloops;
	elFileFnState      *fn;
	int              flags;
	elBool    debuggerflag;
	int   default_register;
} elFileState;


void elf_begin_function(elFileState *fs, elFileFnState *fn, char *line);
char *elf_fget_name(elFileState *fs);

elNodeId elf_parse_unary(elFileState *fs, elFileExpr *expr, elBool flags);
elNodeId elf_parse_expr(elFileState *fs, elFileExpr *expr, elBool flags);
int elf_parse_stat(elFileState *fs);
void elf_fforloop(elFileState *fs);

void elf_emitstore(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elFileBlock *elf_get_loop_block(elFileState *fs, elRegId with_value_register);

int elf_emittereval(elFileState *fs, int flags, int reg, int nreg, elNodeId id);
int elf_emitter_load(elFileState *fs, elNodeId id);

void elf_begin_lastly_block(elFileState *fs, elFileline line);
void elf_close_lastly_block(elFileState *fs, elFileline line);
int elf_emitbranchiffalse(elFileState *fs, elFileExpr *js, elNodeId id);
int elf_emitbranchiftrue(elFileState *fs, elFileExpr *js, elNodeId id);
int *elf_emitjumpiftrue(elFileState *fs, elFileExpr *js, elNodeId id);
int *elf_emitjumpiffalse(elFileState *fs, elFileExpr *js, elNodeId id);

// JZ
// JNZ
enum { L_IF  = 0, L_IFF = 1, };

void elf_fbeginif(elFileState *fs, elFileline line, elSelectState *s, elNodeId x, int z);
void elf_fifaddelifclause(elFileState *fs, elFileline line, elSelectState *s, elNodeId x);
void elf_fifaddelseclause(elFileState *fs, elFileline line, elSelectState *s);
void elf_fifaddthenclause(elFileState *fs, elFileline line, elSelectState *s);
void elf_fcloseif(elFileState *fs, elFileline line, elSelectState *s);
void elf_fbeginrangeloop(elFileState *fs, elFileline line, elNodeId x, elNodeId lo, elNodeId hi);
void elf_fcloserangeloop(elFileState *fs, elFileline line);
void elf_fbegindowhileloop(elFileState *fs, elFileline line);
void elf_fclosedowhileloop(elFileState *fs, elFileline line, elNodeId x);
void elf_emit_begin_while_loop(elFileState *fs, elNodeId x);
void elf_emit_close_while_loop(elFileState *fs);
elBlockId elf_begin_block(elFileState *fs, elBool flags);
void elf_close_block(elFileState *fs);

