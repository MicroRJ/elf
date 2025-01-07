/*
** See Copyright Notice In elf.h
** parse.h
*/


typedef struct {
	unsigned char 	type;
	Source         line;
	unsigned int eol: 1;
	union {
		elf_i64 integer;
		elf_f64 number;
		char 	 *text;
	};
} tokenT;


//todo:remove "LEAVE"
//todo:remove "iff"
//todo:remove "lastly"
//todo:remove "let"
#define KEYWORDDEF(_) \
_(ELF      ,"elf"         ) \
_(DEFAULT  ,"default"     ) \
_(LOAD     ,"load"        ) \
_(NEW      ,"new"         ) \
_(FUN      ,"fun"         ) \
_(NIL      ,"nil"         ) \
_(TRUE     ,"true"        ) \
_(FALSE    ,"false"       ) \
_(ENUM     ,"enum"        ) \
_(TRY      ,"try"         ) \
_(CATCH    ,"catch"       ) \
_(FINALLY  ,"finally"     ) \
_(DO       ,"do"          ) \
_(WHILE    ,"while"       ) \
_(BREAK    ,"break"       ) \
_(CONTINUE ,"continue"    ) \
_(LET      ,"let"         ) \
_(FOR      ,"for"         ) \
_(DEFER    ,"defer"       ) \
_(LASTLY   ,"lastly"      ) \
_(RET      ,"ret"         ) \
_(LEAVE    ,"leave"       ) \
_(IF       ,"if"          ) \
_(IFF      ,"iff"         ) \
_(ELSE     ,"else"        ) \
_(ELIF     ,"elif"        ) \
_(THEN     ,"then"        ) \
/* end */

#define MACRODEF(_) \
_(LINE_NUMBER   , "line_number" ) \
_(LINE_CHAR     , "line_char"   ) \
_(FILE_NAME     , "file_name"   ) \
_(INT           , "int"         ) \
_(NUM           , "num"         ) \
_(LEVEL         , "level"       ) \
_(REGISTER      , "register"    ) \
_(INDEX         , "index"       ) \
_(VALUE         , "value"       ) \
_(ARRAY         , "array"       ) \
_(FIELD         , "field"       ) \
_(ENDOFFILE     , "eof"         ) \
/* end */


/* todo: why would !! and ?? have lower precedence
than relational operators, when !! and ?? work on
values */
#define OPERATORDEF(_) \
_(     POW, "**",12) \
_(     MUL,  "*",11) _( DIV,  "/", 11) _(MOD,"%",11) \
_(     ADD,  "+",10) _( SUB,  "-", 10) \
_(     SHR, ">>", 9) _( SHL, "<<",  9) \
_(      LT,  "<", 8) _(LTEQ, "<=",  8) \
_(      GT,  ">", 8) _(GTEQ, ">=",  8) \
_(      EQ, "==", 7) _( NEQ, "!=",  7) \
_( BIT_AND,  "&", 6) \
_(  BIT_OR,  "|", 5) \
_( BIT_XOR,  "^", 4) \
_( LOG_AND, "&&", 3) _( LOG_OR, "||", 2) \
_( NIL_AND, "!!", 3) _( NIL_OR, "??", 2) \
_(ELLIPSIS,"...", 1) _(DOT_DOT, "..", 1) \
/* end */


#define TOKENDEF(_)                 \
_(INTEGER            ,"integer")    \
_(NUMBER             , "number")    \
_(STRING             , "string")    \
_(LETTER             , "letter")    \
_(WORD               ,   "word")    \
_(QMARK              ,"?")          \
_(EXCLAMATION_MARK   ,"!")          \
_(ASSIGN             ,"=")          \
_(NIL_ASSIGN         ,"?=")         \
_(COLON              ,":")          \
_(SEMI_COLON         ,";")          \
_(COMMA              ,",")          \
_(DOT                ,".")          \
_(SQUARE_LEFT        ,"[")          \
_(SQUARE_RIGHT       ,"]")          \
_(CURLY_LEFT         ,"{")          \
_(CURLY_RIGHT        ,"}")          \
_(PAREN_LEFT         ,"(")          \
_(PAREN_RIGHT        ,")")          \
/* end */

typedef enum tokenTy {
	TK_NONE = 0,

#define TKITEM(NAME,_) XFUSE(TK_,NAME),
#define OPITEM(NAME,_,__) XFUSE(TK_,NAME),
#define MCITEM(NAME,_) XFUSE(TK_M_,NAME),

	KEYWORDDEF(TKITEM)
	MACRODEF(MCITEM)
	TOKENDEF(TKITEM)
	OPERATORDEF(OPITEM)

#undef TKITEM
#undef MCITEM
#undef OPITEM
} tokenTy;

#include "tree.h"

typedef struct {
	int *t,*f;
} jumpS;


typedef struct BranchJumps {
	/* Conditional false jump instructions
	to be patched so that they jump to
	the next block or instruction */
	int *jz;
	/* list of exit jump instructions from
	each consecutive block to be patched */
	int *j;
} BranchJumps;

#define NO_SLOT (-1)
#define NO_JUMP (-0)
#define NO_LINE (0)


typedef int entityID;
typedef struct { entityID id; } entityID2;
#define ENTITY(X) XLITERAL(entityID2){X}

#define NO_ENTITY -1

#define ENTITY_REFERENCED (1 << 0)
#define ENTITY_CONSTANT   (1 << 1)
#define ENTITY_ASSIGNED   (1 << 2)
#define ENTITY_PARAMETER  (1 << 3)
#define ENTITY_FORLOOP    (1 << 4)

typedef enum {
	ENTITY_INVALID = 0, ENTITY_DIRECTORY, ENTITY_LOCAL, ENTITY_GLOBAL,
} entityKi;

typedef struct {
	entityKi kind;
	int    status;
	int     scope;
	treeID   tree;
	char    *name;
	Source   line;
} entityT;

typedef struct Block Block;
struct Block {
	treeID *body;
	treeID *defers;
	int     ended;
	int     has_ret;
};

typedef struct Loop Loop;
struct Loop {
	treeID *breaks;
	treeID *continues;
	treeID  name;
};


typedef struct Parser Parser;
struct Parser {
	elf_State              *R;
	char                *name;
	char                *text;
	char            *line_pos;
	char                 *pos;
	int              line_num;
	treeID          enc;
	treeID         *functions;
	entityT         *entities;
	entityID     entity_index;
	entityID  scope_stack[16];
	entityID      scope_index;
	entityID            scope;
	Source         src_stack[32];
	Source         src_index;
	Source               src;
	Block     block_stack[16];
	int           block_index;
	Block               block;
	Loop       loop_stack[16];
	int            loop_index;
	Loop                 loop;
	tokenT tok,tok_prev,tok_prox;
};

// todo:
// static FileBlock *get_loop_block(Parser *fs, elf_StackId with_value_register);
// static void begin_delay_block(Parser *fs, Source line);
// static void close_delay_block(Parser *fs, Source line);

static int emit_branch_if_false(Parser *fs, jumpS *js, treeID id);
static int emit_branch_if_true(Parser *fs, jumpS *js, treeID id);
static int *emit_jump_if_true(Parser *fs, jumpS *js, treeID id);
static int *emit_jump_if_false(Parser *fs, jumpS *js, treeID id);


static void begin_if(Parser *fs, Source line, BranchJumps *s, treeID x, int z);
static void add_elif_clause(Parser *fs, Source line, BranchJumps *s, treeID x);
static void add_else_clause(Parser *fs, Source line, BranchJumps *s);
static void add_then_clause(Parser *fs, Source line, BranchJumps *s);
static void close_if(Parser *fs, Source line, BranchJumps *s);

static void begin_range_loop(Parser *fs, Source line, treeID x, treeID lo, treeID hi);
static void close_range_loop(Parser *fs, Source line);
static void begin_do_while_loop(Parser *fs, Source line);
static void close_do_while_loop(Parser *fs, Source line, treeID x);
static void begin_while_loop(Parser *fs, treeID x);
static void close_while_loop(Parser *fs);



