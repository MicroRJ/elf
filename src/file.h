/*
** See Copyright Notice In elf.h
** file.h
** Parsing And Code Generation...
*/


typedef struct elToken {
	unsigned char 	type;
	elFileline 		line;
	/* whether this token was terminated by an
	end of line character */
	unsigned int 	 eol: 1;
	union {
		elInteger integer, /* @DEPRECATED */  i;
		elNumber   number, /* @DEPRECATED */  n;
		char 		 *string,	*text, /* @DEPRECATED */ *s;
	};
} elToken;


/* elf is likely to be removed and so is iff */
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


/*



	The following is all crap, I'll keep it here just because:

	Nodes are a minimal 'intermediate' language between
 	source code and bytecode...

 	Conceptually they are identical, the only difference
 	is the nodes allow for a greater degree of
 	expressibility, something that bytecode lacks.

 	For instance, there are a few pseudo instructions
 	which are either only understood by the parser or
 	code generator. One of them is the '(' {x} ')' operator,
 	which doesn't really do anything other than affect
 	the grouping of expressions. Other pseudo instructions,
 	must be converted into more basic instructions.


 	Another aspect is that nodes can freely reference other
 	nodes, in other words, nodes can take other nodes as
 	operands unlike bytecode can only take registers.


	For instance:

	Take this expression: '1 + 2 + 3 + 4'

	At a lower level, suppose '+' could only take registers,
	you'd represent it as:

	load register 0, constant 1
	load register 1, constant 2
	add register 0, register 0 and 1
	load register 1, constant 3
	add register 0, register 0 and 1
	load register 1, constant 4
	add register 0, register 0 and 1

	Whereas a node can represent it as: '(((1 + 2) + 3) + 4)'

	* Here I used '()' but it doesn't imply a group
	node, it's only for emphasis on the hierarchical
	structure.

	This means the parser just has to worry about generating
	the semantically correct nodes and let the emitter worry
	about the process of converting those nodes into code
	which is more straightforwards.

	That being said, nodes are still fairly low level and
	the parser has to still do some desugaring, because
	nodes, like bytecode, cannot represent every feature
	the language offers, and in my experience, sugar
	coating tends to simply things heavily.

	So in my opinion, though it not strictly necessary,
 	it makes for a very clean parser and emitter, and
 	it is not as redundant as having 3 different graphs
 	to do the same thing.
*/


/* todo: make these negative values and just use regular
register node... */
#define SPECIAL_REGISTER_INDEX 0 // #index
#define SPECIAL_REGISTER_VALUE 1 // #value
#define SPECIAL_REGISTER_ARRAY 2 // #array

#define NO_NODE (-1)

typedef int elNodeId;


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
_(SPECIAL_REGISTER) \
_(GLOBAL) _(LOCAL) _(CLOSURE_VALUE) _(FILE_VALUE) \
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


elNodeId elf_make_node_xyz(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y, elNodeId *z);
elNodeId elf_make_binary_node(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x, elNodeId y);
elNodeId elf_make_node_unary(elFileState *fs, elFileline, elNodeKi k, elNodeTy ty, elNodeId x);

elNodeId elf_make_nil_node(elFileState *fs, elFileline);
elNodeId elf_make_integer_node(elFileState *fs, elFileline, elInteger i);
elNodeId elf_make_number_node(elFileState *fs, elFileline, elNumber n);
elNodeId elf_make_string_node(elFileState *fs, elFileline, char *);

elNodeId elf_make_nullary_node(elFileState *fs, elFileline, elNodeKi k, elNodeTy t);
elNodeId elf_make_group_node(elFileState *fs, elFileline, elNodeId x);


elNodeId elf_make_table_node(elFileState *fs, elFileline, elNodeId *z);
elNodeId elf_make_closure_node(elFileState *fs, elFileline, elNodeId x, elNodeId *z);
elNodeId elf_make_load_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_local_node(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_make_special_register_node(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_make_global_value_node(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_closure_node(elFileState *fs, elFileline line, elNodeId i);
elNodeId elf_make_type_guard_node(elFileState *fs, elFileline line, elNodeId x, elNodeTy y);
elNodeId elf_make_metafield_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_make_field_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_make_index_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_make_ranged_index_node(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elNodeId elf_make_call_node(elFileState *fs, elFileline line, elNodeId x, elNodeId *z);


elValueTag elf_nodettotag(elNodeTy ty) {
	switch (ty) {
		case NT_SYS: return TAG_SYS;
		case NT_NUM: return TAG_NUM;
		case NT_INT: return TAG_INT;
		default: elNOCODE;
	}
	return TAG_NIL;
}



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
	elBool      flags;
	char        *name;
	elFileline   line;
	union { elRegId local, slot; };
	int         level;
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


typedef struct elFileFnState elFileFnState;
typedef struct elFileFnState {
	elFileFnState *enclosing;
	elFileline line;
	/* maximum number of local register used concurrently
	at any point for this function */
	elRegId nlocals;
	elRegId xmemory;
	/* array of entities from enclosing function,
	use for closure values... */
	elEntityId *enclosure;
	/* this is needed to emit instructions
	relative to the current function we're
	loading, there's always an active function,
	even at file level */
	/* index to first entity within entity list in file. */
	elEntityId entities;
	elBlockId entry_block;
	// elBlockId block;
	elByteId bytes;
	int nyield;
	/* todo: deprecated */
	/* list of yield jumps to be patched */
	elByteId *yj;
} elFileFnState;


#define NO_SLOT (-1)
#define NO_BYTE (-1)
#define NO_JUMP (-0)
#define NO_LINE (-0)


typedef struct elFileState {
	elModule *M;
	elState  *R;
	/*
	these probably came from GCStrings,
	they should remain alive... I think. */
	char     *filename;
	char     *contents;
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
	elNode *nodes;
	elNodeId nnodes;
	elFileEntity *entities;
	elEntityId nentities;
	elFileBlock *blocks;
	union { elBlockId nblocks, level; };
	/* the current function */
	elFileFnState *fn;
	elByteId bytes;
	int flags;
	elBool debuggerflag;
	/* How many loops are we in */
	int nloops;
	/* The semantics of a leave instruction
	change within a default expression,
	so if this isn't set to '-1' it means
	that a leave instruction should instead
	be a store to this register */
	elRegId default_register;
} elFileState;

char *elf_get_file_name(elFileState *fs);

elNodeId elf_file_load_expr(elFileState *fs, elFileExpr *expr, elBool flags);
elNodeId elf_file_load_unary_expr(elFileState *fs, elFileExpr *expr, elBool flags);
void elf_load_file_stat(elFileState *fs);

void elf_emitter_emit_store(elFileState *fs, elFileline line, elNodeId x, elNodeId y);
elFileBlock *elf_emitter_get_loop_block(elFileState *fs, elRegId with_value_register);
elRegId elf_emitter_local_load(elFileState *fs, elFileline line, elBool reload, elRegId x, elRegId y, elNodeId id);
elRegId elf_emitter_localize(elFileState *fs, elFileline line, elNodeId id);
elRegId elf_emitter_relocalize(elFileState *fs, elFileline line, elRegId target_register, elNodeIdTypeGuard id);
void elf_emitter_enter_delayed_block(elFileState *fs, elFileline line);
void elf_emitter_leave_delayed_block(elFileState *fs, elFileline line);
elByteId elf_branch_if_false(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id);
elByteId elf_branch_if_true(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id);
elByteId *elf_emit_jump_if_true(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id);
elByteId *elf_emit_jump_if_false(elFileState *fs, elFileExpr *js, elRegId x, elNodeId id);

// JZ
// JNZ
enum { L_IF  = 0, L_IFF = 1, };

void elf_emitter_begin_if(elFileState *fs, elFileline line, elSelectState *s, elNodeId x, int z);
void elf_emitter_add_elif_clause(elFileState *fs, elFileline line, elSelectState *s, elNodeId x);
void elf_emitter_add_else_clause(elFileState *fs, elFileline line, elSelectState *s);
void elf_emitter_add_then_clause(elFileState *fs, elFileline line, elSelectState *s);
void elf_emitter_close_if(elFileState *fs, elFileline line, elSelectState *s);
void elf_emitter_begin_ranged_loop(elFileState *fs, elFileline line, elNodeId x, elNodeId lo, elNodeId hi);
void elf_emitter_close_ranged_loop(elFileState *fs, elFileline line);
void elf_emitter_begin_do_while_loop(elFileState *fs, elFileline line);
void elf_emitter_close_do_while_loop(elFileState *fs, elFileline line, elNodeId x);
void elf_emitter_begin_while_loop(elFileState *fs, elFileline line, elNodeId x);
void elf_emitter_close_while_loop(elFileState *fs, elFileline line);
elBlockId elf_emitter_begin_block(elFileState *fs, elBool flags);
void elf_emitter_close_block(elFileState *fs);

