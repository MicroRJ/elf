//
// See Copyright Notice Below.
//
#ifndef _elf_lang_
#define _elf_lang_


#if !defined(KILOBYTES)
#define KILOBYTES(x) ((x)*1024LLU)
#endif
#if !defined(MEGABYTES)
#define MEGABYTES(x) ((x)*1024LLU*1024LLU)
#endif
#if !defined(GIGABYTES)
#define GIGABYTES(x) ((x)*1024LLU*1024LLU*1024LLU)
#endif

#define elGC_MEM_THRESHOLD_MIN ((elf_i64) MEGABYTES(1))
#define elGC_MEM_THRESHOLD_MAX ((elf_i64) MEGABYTES(2))
#define elGC_OBJ_THRESHOLD_MIN ((elf_i64) ((512)*1))
#define elGC_OBJ_THRESHOLD_MAX ((elf_i64) ((512)*1))

#define STACK_MAX MEGABYTES(128)


#if defined(__EMSCRIPTEN__)
	#define elAPI 		EMSCRIPTEN_KEEPALIVE
	#define elEXPORT 	EMSCRIPTEN_KEEPALIVE
#else
	#define elEXPORT 	__declspec(dllexport)
	#define elIMPORT 	__declspec(dllimport)
	#if defined(BUILD_STATIC)
		#define elAPI static
	#else
		#define elAPI
	#endif
#endif


typedef struct elf_Module 	elf_Module;
typedef struct elf_State 	elf_State;
typedef struct elf_Object 	elf_Object;
typedef struct elf_Table 	elf_Table;
typedef struct elf_String 	elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;


typedef elf_Table   *elf_TableId;
typedef elf_String  *elf_strID;
typedef elf_Closure *elf_clsID;


// Todo: deprecated!
typedef signed int  elf_Bool;


// Todo: move to base or something
typedef signed   int  				 bool;
typedef signed   char           elf_i8;
typedef unsigned char           elf_u8;
typedef signed   short         elf_i16;
typedef unsigned short         elf_u16;
typedef signed   int           elf_i32;
typedef signed   int           elf_b32;
typedef unsigned int           elf_u32;
typedef   signed long long int elf_i64;
typedef unsigned long long int elf_u64;
typedef double 			       elf_f64;
typedef float  			       elf_f32;

typedef long long int 	   elf_Int;
typedef double 			   elf_Num;
typedef void 			     *elf_Handle;
typedef int 					elf_Error;
typedef int 				   elf_StackId;
typedef unsigned int 		elf_Hash;

typedef struct elf_bytecode elf_bytecode;

typedef int (* elf_Function)(elf_State *);

// Todo: remove this, this is silly...
typedef struct {
	char *name;
	elf_Function fn;
} NameFunctionPair;

typedef struct elf_Proto elf_Proto;
struct elf_Proto {
	int      arity;
	int  numvalues;
	int  stacksize;
	int   numbytes;
	int      bytes;
};

typedef struct elf_File {
	int pos,end;
	elf_Proto proto;
	elf_String *contents;
	elf_String *name;
} elf_File;


/* first object tag must be OBJ, all other
objects come after it, same order as object
types... */
typedef enum {
	elf_tag_nil = 0,
	elf_tag_tomb,
	elf_tag_num,
	elf_tag_int,
	elf_tag_sysobj,
	elf_tag_proc,
	elf_tag_userobj,
	elf_tag_closure,
	elf_tag_str,
	elf_tag_tab,
} elf_tag_enum;

// todo: remove this!
static char const *tag2s[] = {
	"nil",
	"tomb",
	"num",
	"int",
	"sysobj",
	"proc",
	"userobj",
	"closure",
	"str",
	"tab",
};


typedef enum elf_obj_enum {
	GC_OBJ = 0, GC_CLS, GC_STR, GC_TAB,
} elf_obj_enum;

char *obj2s[]={"obj","cls","str","tab"};

enum {
	GC_COLLECTABLE = 0,
	GC_NOCOLLECT,
};

typedef struct elf_Value elf_Value;
struct elf_Value {
	elf_i32 tag;
	union {
		elf_i64       x_i64;
		union{elf_i32 x_i32,y_i32;};

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

// todo: remove color, and size
typedef struct elf_Object elf_Object;
struct elf_Object {
	elf_i8      type;
	elf_i8     color;
	elf_i16     size;
	elf_Table  *meta;
};

typedef struct elf_String elf_String;
struct elf_String {
	elf_Object     obj;
	elf_Hash      hash;
	int     	   length;
	char       text[1];
};

// note: this thing is so large, we could make the
// index be 32 bits, and allow the user to store
// additional data in the remaining 32 bits.
typedef struct {
	elf_Value key;
	elf_i64   idx;
} elf_table_entry;

// todo: table templates?
typedef struct elf_Table elf_Table;
struct elf_Table {
	elf_Object            obj;
	elf_i64            ntotal;
	elf_i64            nslots;
	elf_i64    	       ndebug;
	elf_table_entry    *slots;
	elf_Value  	       *array;
};


typedef struct elf_Closure elf_Closure;
struct elf_Closure {
	elf_Object    obj;
	elf_Proto   proto;
	elf_Value  values[1];
};

typedef struct elf_Module elf_Module;
struct elf_Module {
	elf_Table      *globals;
	elf_Table      *strings;
	elf_f64        *numbers;
	elf_i64       *integers;
	elf_File         *files;
	elf_Proto       *protos;
	elf_u8           *track;
	char            **lines;
	elf_bytecode     *bytes;
	int              nbytes;
};

typedef struct elf_Stack_Frame elf_Stack_Frame;
struct elf_Stack_Frame {
	elf_Closure      *closure;
	elf_Value         *locals;
	int           bytecounter;
	int               nlocals;
	char                nargs;
	char                nrets;
};

typedef struct elf_Collector elf_Collector;
struct elf_Collector{
	int          phase;
	bool         paused;
	elf_i64      memory_allocated;
	elf_i64      memory_threshold;

	elf_i32      num_objects;
	elf_i64      object_trigger_threshold;
	elf_Object **open_object_slots;
	elf_Object **close_object_slots;
};

// todo: make 32 bits
struct elf_bytecode {
	short z,y,x,k;
};

typedef struct {
	elf_i32      address;
	elf_bytecode bytecode;
	// todo: use different data structure,
	// objects could have been freed,
	elf_Value   operand_x;
	elf_Value   operand_y;
} elf_trail_entry;

typedef struct elf_State elf_State;
struct elf_State {
	elf_Module         *M;
	elf_Collector       G;
	elf_Value      *stack;
	elf_Value  *stack_ptr;
	int         stack_max;

	elf_i32 frame_stack_max;
	elf_Stack_Frame *frame_stack;
	int              frame_index;
	elf_Stack_Frame  frame;

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
	elf_bytecode     *trace_buffer;
	elf_Table         *trace_table;
	struct {
		elf_Table *integer;
		elf_Table *number;
		elf_Table *string;
		elf_Table *table;
	} metatables;
	elf_i32 byte;
};


#define elf_GC_PHASE_MARK GC_COLLECTABLE
#define elf_GC_PHASE_FREE GC_NOCOLLECT
#define FLAG_DEBUGGER         (1 << 0)
#define FLAG_BYTELOGGING      (1 << 1)
#define FLAG_TRACING          (1 << 2)

typedef int Instr;
typedef char *Source;


elAPI void elf_init(elf_State *S, elf_Module *M);
elf_Value *elf_get_stack(elf_State *S);
elf_Value *elf_get_stack_ptr(elf_State *S);

/* allocate and adds the object to stack (prevents it from getting GC'd) */
elAPI elf_Closure *elf_new_closure(elf_State *, elf_Proto proto);
elAPI elf_String *elf_new_string2(elf_State *, elf_i32 length);
elAPI elf_String *elf_new_string(elf_State *, const char *text);
elAPI elf_Object *elf_new_object(elf_State *, elf_i32 size);
elAPI elf_Table *elf_new_table(elf_State *);


/* add objects to stack */
elAPI void elf_add_this(elf_State *S);
elAPI void elf_add_any(elf_State *S, elf_Value value);
elAPI void elf_add_nil(elf_State *S);
elAPI void elf_add_int(elf_State *S, elf_Int);
elAPI void elf_add_num(elf_State *S, elf_Num);
elAPI void elf_add_object(elf_State *S, elf_Object *);
elAPI void elf_push_string(elf_State *S, elf_String *);
elAPI void elf_add_sys(elf_State *S, elf_Handle);
elAPI void elf_add_table(elf_State *S, elf_Table *);
elAPI void elf_add_closure(elf_State *S, elf_Closure *);
elAPI void elf_add_proc(elf_State *S, elf_Function);


/* getting arguments from stack */
elAPI elf_Object  *elf_get_this(elf_State *S);

elAPI elf_Value elf_get_arg(elf_State *S, int arg);
elAPI elf_Int elf_get_int(elf_State *S, int arg);
elAPI elf_Num elf_get_num(elf_State *S, int arg);
elAPI elf_String *elf_get_string(elf_State *S, int arg);
elAPI char *elf_get_text(elf_State *S, int arg);
elAPI elf_Object *elf_get_object(elf_State *S, int arg);
elAPI elf_Table *elf_get_table(elf_State *S, int arg);
elAPI elf_Handle elf_get_sysobj(elf_State *S, int arg);
elAPI elf_Closure *elf_get_cls(elf_State *S, int arg);

elAPI int elf_get_num_args(elf_State *S);
elAPI elf_tag_enum elf_get_tag(elf_State *S, int x);
elAPI void elf_check_args(elf_State *S, char *func, int nargs, char *usage);

/* allocating objects */
elAPI elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto);
elAPI elf_String *elf_alloc_string2(elf_State *S, elf_i32 length);
elAPI elf_String *elf_alloc_string(elf_State *S, const char *text);
elAPI elf_Table *elf_alloc_table2(elf_State *, elf_Int length);
elAPI elf_Table *elf_alloc_table(elf_State *);

/* strings */
elAPI int         elf_get_string_length(elf_String *);
elAPI elf_Hash    elf_get_string_hash(elf_String *);
elAPI bool    elf_get_strings_eq(elf_String *x, elf_String *y);

elf_Int elf_table_try(elf_Table *tab, elf_Value key);
elf_Int elf_table_try_text(elf_Table *tab, const char *text, elf_Int length, elf_Hash hash);
elf_Int elf_table_get_or_add(elf_Table *table, elf_Value key);
elf_Value elf_table_get(elf_Table *tab, elf_Value key);
elf_Int elf_get_array_tally(elf_Table *table);
void elf_array_add(elf_Table *table, elf_Value thing);
bool elf_table_set(elf_Table *table, elf_Value k, elf_Value v);

elAPI void resize_table(elf_Table *table);
elAPI void elf_table_alias(elf_State *S, elf_Table *tab, elf_Value key, elf_Value alias);
elAPI void elf_merge_tables(elf_Table *tab, elf_Table *merger);


elAPI void *elf_alloc_object(elf_State *S, elf_obj_enum type, elf_Int length);
elAPI int elf_exec_file(elf_State *, elf_String *name, int nargs, int nrets);
elAPI int elf_call(elf_State *S, int nargs, int nrets);

/* todo: why are these public */
elf_Int _gc_cycle(elf_State *S);
elf_Int elf_mark_object(elf_Object *obj);

/* Todo: why are we exposing any of this */

/* get clock time (unknown frequency) */
elf_Int elf_get_clock_time();
/* time difference in seconds */
elf_Num elf_time_diff_s(elf_Int begin);
/* time difference in milliseconds */
elf_Num elf_time_diff_ms(elf_Int begin);
elAPI void elf_debugger(char *message);
elAPI void elf_error(elf_State *S, int instr, const char *error);
int elf_add_const_int(elf_State *S, elf_Int i);
int elf_add_const_num(elf_State *S, elf_Num i);
int elf_add_proto(elf_State *S);
int elf_get_global_slot(elf_Module *M, elf_String *name);
int elf_set_global(elf_Module *M, elf_String *name, elf_Value value);

elAPI int elf_query_file_for_instr(elf_Module *M, int instr);
elAPI char *elf_get_instr_line(elf_Module *M, int instr);
elAPI void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc);



#define ELF_FUNCTION(NAME) int NAME(elf_State *S)

#endif
/*
** Copyright (C) 2023-2024 Dayan Rodriguez
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
*/

