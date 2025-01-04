/*
** See Copyright Notice In elf.h
** file.h
** Parsing And Code Generation...
*/


typedef struct tokenT {
	unsigned char 	type;
	Source line;
	unsigned int eol: 1;
	union {
		elf_Int integer;
		elf_Num   number;
		char 		   *text;
	};
} tokenT;


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


typedef enum elf_TokenType {
	TK_NONE = 0,

#define TKITEM(NAME,_) XFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) XFUSE(TK_,NAME),
#define MCITEM(NAME,_) XFUSE(TK_M_,NAME),

	KWLIST(TKITEM)
	MCLIST(MCITEM)
	TKLIST(TKITEM)
	OPLIST(OPITEM)

#undef TKITEM
#undef MCITEM
#undef OPITEM
} elf_TokenType;



#define EXPR_LHS     				0x01
#define EXPR_ALLOW_POSTFIX     	0x02


/* Entity: high-level data structure used for
lexical scoping, binds a name to some
value or compile time thing. */

#define NO_ENTITY -1

typedef int EntityId;
typedef int BlockId;


typedef struct { EntityId id; } EntityIdGuard;

#define ENTITY(X) XLITERAL(EntityIdGuard){X}


#define ENTITY_REFERENCED (1 << 0)
#define ENTITY_CONSTANT   (1 << 1)
#define ENTITY_ASSIGNED   (1 << 2)
#define ENTITY_PARAMETER  (1 << 3)
#define ENTITY_FORLOOP    (1 << 4)


typedef enum EntityKi {
	ENTITY_INVALID = 0, ENTITY_DIRECTORY, ENTITY_LOCAL, ENTITY_GLOBAL,
} EntityKi;


typedef struct FileEntity {
	EntityKi kind;
	char    *name;
	char    *line;
	int     level;
	/* depends on the entity type */
	int      args;
	int     flags;
} FileEntity;


#define BLOCK_LOOP 		0x01
#define BLOCK_ENDED  	0x02
#define BLOCK_DELAYED 	0x04


// we'll come back to this later....
typedef struct ParseLoopState {
	Instr entry;
	Instr *false_jumps,*true_jumps;
	/* these can be directly accessed using #array, #index, and #value.
	#array and #index are guaranteed to be register nodes,
	and #value is an (index node), which translates to
	#array[#index] */
	IR_Id array_register; // _register;
	IR_Id index_register; // _register;
	IR_Id value_register;
	/* todo: why do we need this, please remove? */
	union { IR_Id x; };
} ParseLoopState;

// so i guess this just stores additional
// data for the parser to do some form
// of semantic analysis...
// todo: if we only end up using this for loops,
// then use a loop stack instead and remove this
typedef struct FileBlock FileBlock;
struct FileBlock {
	IR_Id            entry;
	int              flags;
	int              level;
	IR_Id         jumpover;
	ParseLoopState    loop;
	IR_Id      *leavejumps;
};


typedef struct BooleanJumps {
	Instr *t,*f;
} BooleanJumps;


typedef struct BranchJumps {
	/* Conditional false jump instructions
	to be patched so that they jump to
	the next block or instruction */
	Instr *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	Instr *j;
} BranchJumps;


#define NO_SLOT (-1)
#define NO_JUMP (-0)
#define NO_LINE (0)

typedef elf_i32 IR_FuncId;

typedef struct IR_Function IR_Function;

typedef elf_i32 IR_LabelId;

typedef struct IR_Label IR_Label;
struct IR_Label {
	IR_Id src,end;
	bool mark;
	char *name;
};

/* so now a function is made up of basic blocks, a basic
block represents a single piece of continuous code, ever
control flow instruction's destination is a basic block
index */
struct IR_Function {
	IR_FuncId     enclosing;
	Source             line;
	EntityId     *enclosure;
	IR_Id      *yield_jumps;
	IR_Label        *labels;
	int               arity;
	int               nrets;
};

#include "tree.h"

typedef struct IR_Module IR_Module;
struct Parser {
	elf_State      *R;
	/* Todo: remove this from here */
	BC_Module      *M;

	IR_Node          *ir;
	int         ir_index;
	IR_Function   *funcs;
	IR_FuncId       func;
	IR_Label      *label;
	IR_Id           prev;
	treeID     enclosing;
	// we need to get rid of this from here

	FileEntity   *entities;
	EntityId     nentities;

	EntityId      scope_stack[16];
	EntityId      scope_index;
	EntityId      scope;

	// this could actually be used for scoping
	// like we had before, removing the need
	// for the scope stack...
	FileBlock     block_stack[16];
	int           block_index;
	FileBlock     block;


	char        *filename;
	char        *filetext;
	char        *linechar;
	char        *thischar;
	int        linenumber;
	union {
		tokenT tok,tk;
	};
	tokenT tok_prev,tok_prox;

	// Now this is in the IR module...
	// IR_Node 	  *nodes;
	// IR_Id          nnodes;


	int             nloops;
	int              flags;
	elf_Bool    debuggerflag;
};


static int find_local_entity(Parser *fs, int reg);


static int begin_parser(Parser *fs, char *name, char *text);
static void end_parser(Parser *fs);

static IR_FuncId begin_function(Parser *parser, Source line);
static void close_function(Parser *parser);

/* this syntactic block */
static void parser_begin_block(Parser *parser, int flags);
static void parser_close_block(Parser *parser);

// todo:
// static FileBlock *get_loop_block(Parser *fs, elf_StackId with_value_register);
// static void begin_delay_block(Parser *fs, Source line);
// static void close_delay_block(Parser *fs, Source line);

static int emit_branch_if_false(Parser *fs, BooleanJumps *js, IR_Id id);
static int emit_branch_if_true(Parser *fs, BooleanJumps *js, IR_Id id);
static int *emit_jump_if_true(Parser *fs, BooleanJumps *js, IR_Id id);
static int *emit_jump_if_false(Parser *fs, BooleanJumps *js, IR_Id id);


// JZ, JNZ
enum { L_IF  = 0, L_IFF = 1, };


static void begin_if(Parser *fs, Source line, BranchJumps *s, IR_Id x, int z);
static void add_elif_clause(Parser *fs, Source line, BranchJumps *s, IR_Id x);
static void add_else_clause(Parser *fs, Source line, BranchJumps *s);
static void add_then_clause(Parser *fs, Source line, BranchJumps *s);
static void close_if(Parser *fs, Source line, BranchJumps *s);

static void begin_range_loop(Parser *fs, Source line, IR_Id x, IR_Id lo, IR_Id hi);
static void close_range_loop(Parser *fs, Source line);
static void begin_do_while_loop(Parser *fs, Source line);
static void close_do_while_loop(Parser *fs, Source line, IR_Id x);
static void begin_while_loop(Parser *fs, IR_Id x);
static void close_while_loop(Parser *fs);

static char *parser_get_name(Parser *fs);

static IR_Id parse_table(Parser *fs);
static IR_Id parse_unary(Parser *fs, BooleanJumps *expr, elf_Bool flags);
static IR_Id parse_subexpr(Parser *fs, BooleanJumps *expr, int rank, int flags);
static IR_Id parse_expr(Parser *fs, BooleanJumps *expr, elf_Bool flags);
static int parse_stat(Parser *fs);
static void parse_for_loop(Parser *fs);




