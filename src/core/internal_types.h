//
// See Copyright Notice In elf.h
//

#include "internal_shorternames.h"


enum { true = 1, false = 0 };


typedef int BCPos;
#define NO_BYTE (-1)

#include "bytecode.h"


typedef char *Source;


// todo:
extern const char *tag2s[];
extern const char *byte2s[];


// todo: NaN tagging!
typedef struct elf_Value elf_Value;
struct elf_Value {
	elf_Tag tag;
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
// if a function is variadic it just means it won't overwrite
// the additional arguments, so add '...' to your functions if
// you care about those!
//
typedef struct Proto Proto;
struct Proto {
	// and storage for other flags
	u8    variadic;
	u8       arity;
	u8   ncaptures;
	u16  stacksize;
	u16   numbytes;
	u32      bytes;
};


#if !defined(ELF_MAX_FILE_PATH)
#define ELF_MAX_FILE_PATH 256
#endif


//
// information about a compiled file
//
// todo: eventually will need at the proto level
//
typedef struct Proto_File Proto_File;
struct Proto_File {
	Proto_File *prox;
	Proto_File *prev;
	int         bytepos;
	int         byteend;
	Proto   proto;
	int         size;
	char        name[ELF_MAX_FILE_PATH];
	// this is null terminated!
	char        text[];
};


typedef struct elf_Object elf_Object;
struct elf_Object {
	elf_i8      type;
	elf_i8     color;
	elf_i16     size;
	elf_Table  *meta;
};

typedef struct elf_Closure elf_Closure;
struct elf_Closure {
	elf_Object obj;
	// todo: should be proto id or something instead?
	Proto      proto;
	elf_Value  captures[];
};

// todo: we use 64 bits for all indices
typedef struct {
	elf_Value  key;
	elf_Index  idx;
} TEntry;


typedef struct elf_Table elf_Table;
struct elf_Table {
	elf_Object         obj;
	elf_Index       ntotal;
	elf_Index       nslots;
	elf_Index    	 ndebug;
	union {
		TEntry    *slots;
		TEntry    *entries;
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
	// the start of this frame on the stack right on bellow of the function
	V            *framebase;

	u8                nargs;
	u8                nrets;

	//
	// todo: remove could be retrieved from the value above framebase
	u8                arity;
	u8             variadic;
	// the rest is only for interpreter frames
	//
	// todo: you could get this from the closure at framebase-1
	int               bytes;
	int               bytec;
	// only for interpreter because no other functions care about this.
	int           framesize;
	int           nextinstr;
	V            *reference;
	// todo: could get this from the closure at framebase-1
	V            *closureenv;
	char          closuresize;
};


typedef struct {
	elf_i32     address;
	Bytec   bytecode;
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
		Proto       *protos;
		//
		elf_u8           *track;
		// todo: compress this
		char            **lines;

		Proto_File      **files;
		// todo: I don't know how long we'll manage with just the one
		// buffer, especially with people loading stuff at runtime,
		// I think we'll do our own memory management here, and once
		// we run out of space we can either reallocate to get more, or
		// if too fragmented do a copy and compact
		Bytec        *bytebuf;
		int               bytecur;
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
	Bytec     *trace_buffer;
	elf_Table         *trace_table;
	elf_i32                   byte;

	struct {
		elf_Table *integer;
		elf_Table *number;
		elf_Table *string;
		elf_Table *table;
	} metatables;
};