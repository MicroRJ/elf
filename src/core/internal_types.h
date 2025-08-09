//
// See Copyright Notice In elf.h
//


#define NO_BYTE (-1)


typedef elf_String *strID;
typedef elf_Table  *tabID;


// todo: remove, rename to something else
typedef int Instr;


typedef char *Source;


// todo:
extern const char *tag2s[];


// todo: NaN tagging!
typedef struct elf_Value elf_Value;
struct elf_Value {
	elf_i32 tag;
	union {
		elf_i64         x_i64;
		struct {elf_i32 x_i32, y_i32; };

		elf_i64         x_int;
		elf_f64         x_num;
		void           *x_ptr;
		elf_Handle      x_sys;
		elf_Closure    *x_closure;
		elf_Object     *x_obj;
		elf_Table      *x_tab;
		elf_String     *x_str;
		elf_Function    x_proc;
	};
};


//
// functions are parsed, compiled, ultimately these
// remain, storing the necessary information for a
// function.
//
typedef struct elf_Proto elf_Proto;
struct elf_Proto {
	int      arity;
	int  ncaptures;
	int  stacksize;
	int   numbytes;
	int      bytes;
};

//
// keeps information about a file for introspection
//
typedef struct elf_File {
	int pos, end;
	elf_Proto   proto;
	// todo: do not keep this in memory, instead just
	// re-read the file?
	elf_String *contents;
	elf_String *name;
} elf_File;


typedef struct elf_Object elf_Object;
struct elf_Object {
	elf_i8      type;
	elf_i8     color;
	elf_i16     size;
	elf_Table  *meta;
};

typedef struct elf_Closure elf_Closure;
struct elf_Closure {
	elf_Object      obj;
	// todo: should be proto id or something instead?
	elf_Proto     proto;
	elf_Value  captures[];
};

// todo: we use 64 bits for all indices
typedef struct {
	elf_Value  key;
	index_t    idx;
} elf_Table_Entry;


typedef struct elf_Table elf_Table;
struct elf_Table {
	elf_Object            obj;
	index_t       ntotal;
	index_t       nslots;
	index_t    	 ndebug;
	union {
		elf_Table_Entry    *slots;
		elf_Table_Entry    *entries;
	};
	elf_Value  	       *array;
};

// todo: 64 bit strings?
typedef struct elf_String elf_String;
struct elf_String {
	elf_Object      obj;
	hash_t    hash;
	elf_i32 	    length;
	char        text[1];
};


typedef struct Stack_Frame Stack_Frame;
struct Stack_Frame {
	// todo: you could get this from the closure
	// at framebase-1
	int               bytes;
	int               bytec;

	short             nargs;
	short             nrets;
	int           nextinstr;
	int           framesize;
	elf_Value    *framebase;

	// todo: you could get this from the closure
	// at framebase-1
	elf_Value    *closureenv;
	char          closuresize;
};


// todo: make 32 bits
typedef struct elf_Bytec {
	short b_z,b_y,b_x,b_k;
} elf_Bytec;

typedef struct {
	elf_i32     address;
	elf_Bytec   bytecode;
	// todo: use different data structure,
	// objects could have been freed,
	elf_Value   operand_x;
	elf_Value   operand_y;
} elf_trail_entry;

// todo: remove!
#define elf_Module elf_State

typedef enum elf_GC_Ty {
	GC_OBJ = 0,
	GC_CLS,
	GC_STR,
	GC_TAB,
} elf_GC_Ty;


#define	GC_COLLECTABLE 0
#define	GC_NOCOLLECT   1
#define	GC_PHASE_MARK  GC_COLLECTABLE
#define	GC_PHASE_FREE  GC_NOCOLLECT
#define	GC_MEM_THRESHOLD_MIN ((elf_i64) MEGABYTES(  1))
#define	GC_MEM_THRESHOLD_MAX ((elf_i64) MEGABYTES(128))
#define	GC_OBJ_THRESHOLD_MIN ((elf_i64) ((512) * 1))
#define	GC_OBJ_THRESHOLD_MAX ((elf_i64) ((512) * 1))

//
// todo: proper object pooling
// todo: custom allocator
//
typedef struct GCState GCState;
struct GCState {
	elf_i32      phase;
	elf_GC_State state;
	elf_i64      memory_allocated;
	elf_i64      memory_threshold;

	elf_i32      num_objects;
	elf_i64      object_trigger_threshold;

	// todo: remove this!
	elf_Object **open_object_slots;
	elf_Object **close_object_slots;
};

typedef struct elf_State elf_State;
struct elf_State {

	struct {
		elf_Table      *globals;
		elf_f64        *numbers;
		elf_i64       *integers;
	};

	struct {
		elf_File         *files;
		elf_Proto       *protos;
		elf_u8           *track;
		// todo: make this better!
		char            **lines;
		elf_Bytec        *bytes;
		int              nbytes;
	};

	// todo: experiment with allocating types and tags
	// in separate buffers!
	// We also wouldn't need to increment or decrement
	// the tags stack, but we would either have to convert
	// stack_ptr to an integer, or make stack_ptr be an integer
	elf_Value        *stack;
	elf_Value        *stack_ptr;
	int               stack_max;

	union {  GCState  G, gc; };


	int          frame_stack_max;
	Stack_Frame *frame_stack;
	int          frame_index;
	Stack_Frame  frame;


	// todo: disable this
	elf_trail_entry *exec_trail;
	elf_i32 			  exec_trail_index;
	elf_i32 			  exec_trail_capacity; // power of two!
	int                      flags;
	int            disable_tracing;
	int trace_inner_loop_stack[16];
	int   trace_inner_loop_counter;
	int           trace_stop_instr;
	int          trace_start_instr;
	int           active_trace_pos;
	int           active_trace_len;
	elf_Bytec     *trace_buffer;
	elf_Table         *trace_table;
	elf_i32                   byte;

	struct {
		elf_Table *integer;
		elf_Table *number;
		elf_Table *string;
		elf_Table *table;
	} metatables;
};