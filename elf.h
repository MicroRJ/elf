/*
** See Copyright Notice Below.
** elf.h
e*/

#ifndef _elf_lang_
#define _elf_lang_

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


typedef elf_Table   *elf_tabID;
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



typedef int (* elf_Function)(elf_State *);

typedef struct elf_CBinding {
	char *name;
	elf_Function fn;
} elf_CBinding;

typedef struct elf_Proto {
	int               arity;
	int             nvalues;
	int             nlocals;
	int              nbytes;
	int               bytes;
} elf_Proto;

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
	elf_tag_nil=0,
	elf_tag_tomb,
	elf_tag_num,
	elf_tag_int,
	elf_tag_sysobj,
	elf_tag_proc,
	elf_tag_float2,
	elf_tag_userobj,
	elf_tag_closure,
	elf_tag_str,
	elf_tag_tab,
} elf_tagenum;

static char const *tag2s[] = {
	"nil",
	"tomb",
	"num",
	"int",
	"sysobj",
	"proc",
	"float2",
	"userobj",
	"closure",
	"str",
	"tab",
};


typedef enum elf_GCTy {
	GC_OBJ=0,GC_CLS,GC_STR,GC_TAB,
} elf_GCTy;

enum {
	elf_GC_WHITE=0,
	elf_GC_BLACK,
	elf_GC_RED,
	elf_GC_PINK,
	elf_GC_TRAP,
};

struct elf_Object {
	elf_i8      type;
	elf_i8     color;
	elf_i16     size;
	elf_Table  *meta;
};

/* Todo: figure out what do actually do with this,
it's going to get padded to 128 - bits anyways...
this gave me the idea tho, why not go with it?
Maybe every value is vector type? */
typedef struct elf_Value {
	elf_i32 tag;
	union {
		union {
			elf_Int         x_int;
			elf_Num         x_num;
			void           *x_ptr;
			elf_Handle      x_sys;
			elf_Closure    *x_cls;
			elf_Object       *x_obj;
			elf_Table      *x_tab;
			elf_String     *x_str;
			elf_Function    x_fun;
		};
		struct { elf_i32 x_i32, y_i32, z_i32; };
		struct { elf_f32 x_f32, y_f32, z_f32; };
	};
} elf_Value;


typedef struct elf_String {
	elf_Object       obj;
	elf_Hash      hash;
	int     	   length;
	char       text[1];
} elf_String;


typedef struct elf_Entry {
	elf_Value key;
	elf_Int   idx;
} elf_Entry;

typedef struct elf_Table {
	elf_Object      obj;
	elf_Int    ntotal;
	elf_Int    nslots;
	elf_Int    ndebug;
	elf_Entry  *slots;
	elf_Value  *array;
} elf_Table;


elAPI void elf_init(elf_State *S, elf_Module *M);

/* allocate and adds the object to stack (prevents it from getting GC'd) */
elAPI elf_Closure *elf_new_closure(elf_State *, elf_Proto fn);
elAPI elf_String *elf_new_string2(elf_State *, elf_Int length);
elAPI elf_String *elf_new_string(elf_State *, const char *text);
elAPI elf_Object *elf_new_object(elf_State *, elf_Int size);
elAPI elf_Table *elf_new_table(elf_State *);


/* add objects to stack */
elAPI void elf_push_this(elf_State *S);
elAPI void elf_push(elf_State *S, elf_Value value);
elAPI void elf_push_nil(elf_State *S);
elAPI void elf_push_integer(elf_State *S, elf_Int);
elAPI void elf_push_number(elf_State *S, elf_Num);
elAPI void elf_add_obj(elf_State *S, elf_Object *);
elAPI void elf_push_string(elf_State *S, elf_String *);
elAPI void elf_add_sys(elf_State *S, elf_Handle);
elAPI void elf_push_table(elf_State *S, elf_Table *);
elAPI void elf_push_closure(elf_State *S, elf_Closure *);
elAPI void elf_push_proc(elf_State *S, elf_Function);


/* getting arguments from stack */
elAPI elf_Object  *elf_get_this(elf_State *S);

elAPI elf_Value    elf_get_arg(elf_State *S, int arg);
elAPI elf_Int      elf_get_int(elf_State *S, int arg);
elAPI elf_Num      elf_get_num(elf_State *S, int arg);
elAPI elf_String  *elf_get_string(elf_State *S, int arg);
elAPI char        *elf_get_text(elf_State *S, int arg);
elAPI elf_Object    *elf_get_obj(elf_State *S, int arg);
elAPI elf_Table   *elf_get_table(elf_State *S, int arg);
elAPI elf_Handle   elf_get_sysobj(elf_State *S, int arg);
elAPI elf_Closure *elf_get_cls(elf_State *S, int arg);

elAPI int elf_get_num_args(elf_State *S);
elAPI elf_tagenum elf_get_tag(elf_State *S, int x);
elAPI void elf_check_args(elf_State *S, char *func, int nargs, char *usage);

/* allocating objects */
elAPI elf_Closure *elf_alloc_closure(elf_State *S, elf_Proto proto);
elAPI elf_String *elf_alloc_string2(elf_State *S, elf_Int length);
elAPI elf_String *elf_alloc_string(elf_State *S, const char *text);
elAPI elf_Table *elf_alloc_table2(elf_State *, elf_Int length);
elAPI elf_Table *elf_alloc_table(elf_State *);

/* strings */
elAPI int         elf_get_string_length(elf_String *);
elAPI elf_Hash    elf_get_string_hash(elf_String *);
elAPI bool    elf_get_strings_eq(elf_String *x, elf_String *y);

#include "src\table.h"

/* todo: deprecate! */
elAPI void elf_tsetx_bindings(elf_State *S, elf_Table *tab, elf_CBinding *list, int num);
elAPI elf_Value   elf_tgetx_any(elf_Table *tab, char const *key/* , or = nil */);
elAPI elf_Num     elf_tgetx_num(elf_Table *tab, char const *key, elf_Num     or);
elAPI elf_Int     elf_tgetx_int(elf_Table *tab, char const *key, elf_Int     or);
elAPI elf_String *elf_tgetx_str(elf_Table *tab, char const *key, elf_String *or);
elAPI elf_Table  *elf_tgetx_tab(elf_Table *tab, char const *key, elf_Table  *or);
elAPI char const *elf_tgetx_txt(elf_Table *tab, char const *key, char const *or);
elAPI elf_Value   elf_tgets_any(elf_Table *tab, elf_String *key);
elAPI elf_Num     elf_tgets_num(elf_Table *tab, elf_String *key);
elAPI elf_Int     elf_tgets_int(elf_Table *tab, elf_String *key);
elAPI elf_String *elf_tgets_str(elf_Table *tab, elf_String *key);
elAPI elf_Table  *elf_tgets_tab(elf_Table *tab, elf_String *key);
elAPI elf_Int    elf_tgetsor_int(elf_Table *tab, elf_String *key, elf_Int or);


/* table set using string field */
elAPI void elf_tsets_num(elf_Table *tab, elf_String *key, elf_Num val);
elAPI void elf_tsets_int(elf_Table *tab, elf_String *key, elf_Int val);
elAPI void elf_tsets_str(elf_Table *tab, elf_String *key, elf_String *val);
elAPI void elf_tsets_tab(elf_Table *tab, elf_String *key, elf_Table *val);

elAPI void elf_check_table(elf_Table *table);
elAPI void elf_table_alias(elf_State *S, elf_Table *tab, elf_Value key, elf_Value alias);
elAPI void elf_merge_tables(elf_Table *tab, elf_Table *merger);


elAPI void *elf_alloc_object(elf_State *S, elf_GCTy type, elf_Int length);
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
elAPI void elf_fail_(elf_State *S, int instr, const char *error);
#define elf_fail(R,instr,error) elf_fail_(R,instr,error)

int elf_add_const_int(elf_State *S, elf_Int i);
int elf_add_const_num(elf_State *S, elf_Num i);
int elf_add_proto(elf_State *S);
int elf_get_global(elf_Module *M, elf_String *name);
int elf_set_global(elf_Module *M, elf_String *name, elf_Value value);

// elAPI elf_SymbolId elf_add_proto(elf_Module *M, elf_Proto fn);
// elAPI elf_SymbolId elf_get_global(elf_Module *M, elf_String *name);
// elAPI elf_SymbolId elf_set_global(elf_Module *M, elf_String *name, elf_Value v);
elAPI int elf_get_instr_file(elf_Module *M, int instr);
elAPI char *elf_get_instr_line(elf_Module *M, int instr);
elAPI void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc);



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

