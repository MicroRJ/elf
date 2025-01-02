/*
** See Copyright Notice In elf.h
** file.h
** Parsing And Code Generation...
*/


typedef struct FileToken {
	unsigned char 	type;
	Source line;
	unsigned int eol: 1;
	union {
		elf_Int integer;
		elf_Num   number;
		char 		   *text;
	};
} FileToken;


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


typedef struct elFileLoopState {
	Instr entry;
	Instr *false_jumps,*true_jumps;
	/* these can be directly accessed using #array, #index,
	and #value.
	#array and #index are guaranteed to be register nodes,
	and #value is an (index node), which translates to
	#array[#index] */
	TreeId array_register; // _register;
	TreeId index_register; // _register;
	TreeId value_register;
	/* todo: why do we need this, please
	remove? */
	union { TreeId x; };
} elFileLoopState;


#define BLOCK_LOOP 		0x01
#define BLOCK_ENDED  	0x02
#define BLOCK_DELAYED 	0x04


typedef struct FileBlock FileBlock;
typedef struct FileBlock {
	elf_Bool flags;
	int level;
	int xmemory;
	int nlocals;
	int xentity;
	int xnode;
	Instr entry;
	Instr jumpover;
	Instr *leavejumps;
	elFileLoopState loop;
} FileBlock;


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


typedef struct FileFunction FileFunction;
typedef struct FileFunction {
	FileFunction  *enclosing;
	char               *line;
	int              nlocals;
	int              xmemory;
	EntityId      *enclosure;
	EntityId        entities;
	BlockId      entry_block;
	Instr              bytes;
	Instr             nbytes;
	int               nyield;
	Instr       *yield_jumps;
} FileFunction;


typedef struct Parser {
	FileFunction function;
	elf_Module           *M;
	elf_State            *R;
	char        *filename;
	char        *filetext;
	char        *linechar;
	char        *thischar;
	int        linenumber;
	union {
		FileToken this_token,tk;
	};
	FileToken last_token,then_token;
	Tree 	          *nodes;
	TreeId          nnodes;
	FileEntity   *entities;
	EntityId     nentities;
	FileBlock      *blocks;
	BlockId        nblocks;
	int             nloops;
	FileFunction       *fn;
	int              flags;
	elf_Bool    debuggerflag;
} Parser;


static int find_local_entity(Parser *fs, int reg);


static int parser_begin(Parser *fs, char *name, char *text);
static void parser_end(Parser *fs);


static void begin_function(Parser *fs, FileFunction *fn, Source line);
static void close_function(Parser *fs);


static BlockId begin_block(Parser *fs, elf_Bool flags);
static void close_block(Parser *fs);
static FileBlock *get_loop_block(Parser *fs, elf_StackId with_value_register);


static void begin_delay_block(Parser *fs, Source line);
static void close_delay_block(Parser *fs, Source line);


static void emit_store(Parser *fs, Source line, TreeId x, TreeId y);

static int emit_eval(Parser *fs, int flags, int reg, int nreg, TreeId id);
static int emit_load(Parser *fs, TreeId id);
static int emit_preload(Parser *fs, TreeId id);


static int emit_branch_if_false(Parser *fs, BooleanJumps *js, TreeId id);
static int emit_branch_if_true(Parser *fs, BooleanJumps *js, TreeId id);
static int *emit_jump_if_true(Parser *fs, BooleanJumps *js, TreeId id);
static int *emit_jump_if_false(Parser *fs, BooleanJumps *js, TreeId id);


// JZ, JNZ
enum { L_IF  = 0, L_IFF = 1, };


static void begin_if(Parser *fs, Source line, BranchJumps *s, TreeId x, int z);
static void add_elif_clause(Parser *fs, Source line, BranchJumps *s, TreeId x);
static void add_else_clause(Parser *fs, Source line, BranchJumps *s);
static void add_then_clause(Parser *fs, Source line, BranchJumps *s);
static void close_if(Parser *fs, Source line, BranchJumps *s);

static void begin_range_loop(Parser *fs, Source line, TreeId x, TreeId lo, TreeId hi);
static void close_range_loop(Parser *fs, Source line);
static void begin_do_while_loop(Parser *fs, Source line);
static void close_do_while_loop(Parser *fs, Source line, TreeId x);
static void begin_while_loop(Parser *fs, TreeId x);
static void close_while_loop(Parser *fs);

static char *parser_get_name(Parser *fs);

static TreeId parse_table(Parser *fs);
static TreeId parse_unary(Parser *fs, BooleanJumps *expr, elf_Bool flags);
static TreeId parse_subexpr(Parser *fs, BooleanJumps *expr, int rank, int flags);
static TreeId parse_expr(Parser *fs, BooleanJumps *expr, elf_Bool flags);
static int parse_stat(Parser *fs);
static void parse_for_loop(Parser *fs);




