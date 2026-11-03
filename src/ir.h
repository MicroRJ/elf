//
// See Copyright Notice In elf.h
//

typedef struct IrNode IrNode;
typedef IrNode *Ir;
typedef struct IrEmitter IrEmitter;
typedef struct IrDefer IrDefer;
typedef struct IrLoopFrame IrLoopFrame;
typedef struct IrModule IrModule;
typedef struct IrCapture IrCapture;
typedef struct IrFunction IrFunction;



#define NO_SLOT (-1)

/*
IR kinds are grouped by behavior:

Sentinels
  IR_NONE is an invalid or uninitialized kind and is never emitted.
  IR_ERROR preserves an already reported lowering failure.

Literal values
  IR_NIL, IR_INTEGER, and IR_NUMBER produce their corresponding literal values.
  IR_ATOM produces a runtime string from an interned compiler atom.

Named, local, and callable values
  IR_NATIVE_FUNCTION refers to a native function in the target state's registry.
  IR_CAPTURE_REF refers to an IrCapture in the current function's capture table.
  IR_RECURSE produces the current callable for a recursive call.
  IR_FUNCTION_REF refers to an IrFunction and materializes a closure when used as a value.
  IR_LOAD reads an IR_LOCAL whose statement has already assigned its slot; it never allocates the slot itself.
  IR_GET_MEM produces that assigned stack-slot index and has the same allocation precondition as IR_LOAD.

Aggregate values and access
  IR_NEW_TAB and IR_NEW_ARR allocate empty collections.
  IR_INDEX reads an indexed value and may also be an IR_STORE destination.
  IR_FIELD reads a field and preserves its receiver as the implicit argument when called.
  IR_META_FIELD does the same for a meta field.
  IR_MEASURE produces the measured length of its operand.

Short-circuit expressions
  IR_AND and IR_OR evaluate lazily and produce normalized boolean zero or one.
  IR_NIL_OR produces its left operand unless it is nil, then evaluates its right operand.

Arithmetic, comparison, and bitwise expressions
  IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_POW, and IR_MOD perform their named arithmetic operations.
  IR_EQ, IR_NOT_EQ, IR_LESS_THAN, and IR_LESS_THAN_EQ compare two operands.
  IR_SHIFT_LEFT, IR_SHIFT_RIGHT, IR_BITWISE_AND, IR_BITWISE_OR, and IR_BITWISE_XOR operate on two integers.
  IR_BITWISE_NOT computes the complement of one integer.

Calls and structured expressions
  IR_CALL calls an expression with args; the backend selects direct or dynamic dispatch.
  IR_EXPR_BLOCK executes a statement sequence and then produces its final value.

Statements and storage
  IR_LOCAL represents stable local storage. Parameters receive incoming slots during function setup; a standalone IR_LOCAL
  statement adopts its initializer's result slot or allocates a fresh slot when emitted.
  IR_MULTI_LOCAL is a statement that evaluates one expression for N results and assigns those consecutive slots to N locals.
  IR_STORE writes to an allocated local, field, or index; a local destination must already have an assigned slot.
  IR_ARRAY_ADD appends one value to an array.
  IR_BLOCK executes statements with a scoped local-slot lifetime.
  IR_IF executes one of two statement branches according to a predicate.
  IR_RETURN returns zero or one value from the current function.

Low-level control flow
  IR_LABEL defines a symbolic bytecode position.
  IR_JUMP transfers control unconditionally to an IR_LABEL.
  IR_JUMP_IF_FALSE transfers control to an IR_LABEL when its predicate is false.
*/
#define IR_DEFINITIONS(X)                                   \
	X(IR_NONE                , "none")                       \
	X(IR_ERROR               , "error")                      \
	X(IR_NIL                 , "nil")                        \
	X(IR_INTEGER             , "integer")                    \
	X(IR_NUMBER              , "number")                     \
	X(IR_ATOM                , "atom")                       \
	X(IR_NATIVE_FUNCTION     , "native_function")            \
	X(IR_CAPTURE_REF         , "capture_ref")                \
	X(IR_RECURSE             , "recurse")                    \
	X(IR_FUNCTION_REF        , "function_ref")               \
	X(IR_LOAD                , "load")                       \
	X(IR_GET_MEM             , "get_mem")                    \
	X(IR_NEW_TAB             , "new_tab")                    \
	X(IR_NEW_ARR             , "new_arr")                    \
	X(IR_TAB_REF             , "tab_ref")                    \
	X(IR_ARR_REF             , "arr_ref")                    \
	X(IR_INDEX               , "index")                      \
	X(IR_FIELD               , "field")                      \
	X(IR_META_FIELD          , "meta_field")                 \
	X(IR_MEASURE             , "measure")                    \
	X(IR_AND                 , "and")                        \
	X(IR_OR                  , "or")                         \
	X(IR_NIL_OR              , "nil_or")                     \
	X(IR_ADD                 , "add")                        \
	X(IR_SUB                 , "sub")                        \
	X(IR_MUL                 , "mul")                        \
	X(IR_DIV                 , "div")                        \
	X(IR_POW                 , "pow")                        \
	X(IR_MOD                 , "mod")                        \
	X(IR_EQ                  , "eq")                         \
	X(IR_NOT_EQ              , "not_eq")                     \
	X(IR_LESS_THAN           , "less_than")                  \
	X(IR_LESS_THAN_EQ        , "less_than_eq")               \
	X(IR_SHIFT_LEFT          , "shift_left")                 \
	X(IR_SHIFT_RIGHT         , "shift_right")                \
	X(IR_BITWISE_AND         , "bitwise_and")                \
	X(IR_BITWISE_OR          , "bitwise_or")                 \
	X(IR_BITWISE_XOR         , "bitwise_xor")                \
	X(IR_BITWISE_NOT         , "bitwise_not")                \
	X(IR_TUPLE               , "tuple")                      \
	X(IR_CALL                , "call")                       \
	X(IR_EXPR_BLOCK          , "expr_block")                 \
	X(IR_LOCAL               , "local")                      \
	X(IR_MULTI_LOCAL         , "multi_local")                \
	X(IR_STORE               , "store")                      \
	X(IR_ARRAY_ADD           , "array_add")                  \
	X(IR_BLOCK               , "block_stat")                 \
	X(IR_IF                  , "if")                         \
	X(IR_RETURN              , "return")                     \
	X(IR_LABEL               , "label")                      \
	X(IR_JUMP                , "jump")                       \
	X(IR_JUMP_IF_FALSE       , "jump_if_false")              \
/* end */

typedef enum
{

#define EXPAND(ENUM, NAME) ENUM,
	IR_DEFINITIONS(EXPAND)
#undef EXPAND

	IR_COUNT_,
}
IrKind;

static inline const char *ir_kind_name(IrKind kind)
{
	static const char *names[] =
	{
#define EXPAND(ENUM, NAME) #ENUM,
		IR_DEFINITIONS(EXPAND)
#undef EXPAND
	};

	if ((u32)kind >= IR_COUNT_) {
		return "IR_UNKNOWN";
	}
	return names[kind];
}

struct IrDefer
{
	IrDefer *previous;
	Ir       ir;
};

struct IrLoopFrame
{
	IrLoopFrame *parent;
	Ir           continue_label;
	Ir           break_label;
	IrDefer     *defer_start;
};

// Note, this is the state needed to to lower one IrFunction ... but we also use it for allocating IrNodes ...
// which is a bit odd ...
struct IrEmitter
{
	IrModule    *module;
	IrFunction  *function;
	Com_Source  *source;
	struct Scope *active_scope;

	IrDefer     *defer;
	IrDefer     *defer_scope_start;
	IrLoopFrame *loop;
	// Todo, when revisiting this again, reuse the ir stack for the entire process
	// not just per function ...
	u32          ir_stack_size;
	u32          ir_stack_index;
	Ir          *ir_stack;
};

typedef struct
{
	Ir *items;
	u32 count;
}
IrArray;

typedef struct
{
	IrEmitter *emitter;
	u32        start;
	u32        count;
}
IrQueue;

struct IrCapture
{
	Ir local;
	Ir target;
};

struct IrFunction
{
	IrFunction *next;
	IrFunction *parent;
	Ast         ast;
	Com_Source *source;
	SourceSite  site;
	b32         variadic;
	Ir          body;
	IrArray     params;
	IrCapture  *caps;
	u32         ncaps;
	u32         xcaps;
	IrFunction **funcs;
	u32          nfuncs;
	u32          xfuncs;
	elf_Prototype *proto;
};

struct IrModule
{
	Arena       *arena;
	IrFunction *first_function;
	IrFunction  *last_function;
	u32         function_count;
};

struct IrNode
{
	IrKind     kind;
	SourceSite site;
	TypeRule   type_rule;

	union
	{
		Com_Status status;
		u32        native_function_index;
		u32        capture;

		struct
		{
			b32 defined;
			u32 position;
		}
		ir_label;

		Atom *atom;
		i64   ir_int;
		f64   ir_num;
		elf_Table *ir_tab;
		elf_Array *ir_arr;

		Ir ir_unary;
		IrArray ir_tuple;

		struct
		{
			Ir x;
			Ir y;
		}
		ir_binary;

		struct
		{
			Ir      expr;
			IrArray args;
		}
		ir_call;

		u32 func;

		struct
		{
			Ir        pred;
			Ir true_clause;
			Ir else_clause;
		}
		ir_if;

		struct
		{
			Ir expr;
		}
		ir_return;

		struct
		{
			IrArray stats;
		}
		ir_block;

		struct
		{
			IrArray stats;
			Ir       value;
		}
		ir_expr_block;

		struct
		{
			Ir pred;
			Ir label;
		}
		ir_jump_if_false;

		struct
		{
			Ir label;
		}
		ir_jump;

		struct
		{
			Ir expr;
		}
		ir_continue;

		struct
		{
			Ir expr;
		}
		ir_break;

		struct
		{
			IrFunction *owner;
			Ir          expr;
			i32         slot;
		}
		ir_local;

		struct
		{
			Ir        expr;
			IrArray locals;
		}
		ir_multi_local;

		struct
		{
			Ir local;
		}
		ir_load;
	};
};


static IrNode ir_error_sentinel;
static IrNode ir_oom_sentinel;

#define ERROR_IR     (&ir_error_sentinel)
#define ERROR_OOM_IR (&ir_oom_sentinel)

static inline Ir ir_from_status(Com_Status status)
{
	switch (status)
	{
		case COM_STATUS_REPORTED: return ERROR_IR;
		case COM_STATUS_OOM:      return ERROR_OOM_IR;
		default: ASSERT(0);       return ERROR_IR;
	}
}

static inline b32 ir_is_error(Ir ir)
{
	return ir && ir->kind == IR_ERROR;
}

static inline Com_Status ir_status(Ir ir)
{
	ASSERT(ir_is_error(ir));
	ASSERT(ir->status != COM_STATUS_OK);
	return ir->status;
}



