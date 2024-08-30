/*
** See Copyright Notice Below.
** elf.h
** The λ elf language.
*/


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


/* todo: remove these from here?? */
#if !defined(__cplusplus)
	#define LITERAL(X) (X)
#else
	#define LITERAL(X) X
#endif


#define TO_TEXT_(X) #X
#define TO_TEXT(X) TO_TEXT_(X)

#define FUSE_(X,Y) X##Y
#define FUSE(X,Y) FUSE_(X,Y)

#if !defined(COUNTOF)
	#define COUNTOF(X) (sizeof(X)/sizeof(X[0]))
#endif


/* cast to union types */
#define UCAST(D,T) ( ((union { T _; }){D})._ )


/* todo: remove this from here */
#define NO_BYTE (-1)


typedef struct elf_Module 	elf_Module;
typedef struct elf_Shell 	elf_Shell;
typedef struct elf_Object 	elf_Object;
typedef struct elf_Table 	elf_Table;
typedef struct elf_String 	elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;


typedef long long int 	   elf_Int;
typedef signed int 		   elf_Bool;
typedef double 			   elf_Num;
typedef void 			     *elf_Handle;
typedef int 					elf_Error;
typedef int 				   elf_StackId;
typedef int 				   elf_SymbolId;
typedef unsigned int 		elf_Hash;


typedef int (* elf_CFunction)(elf_Shell *);


typedef struct elf_CBinding {
	char *name;
	elf_CFunction fn;
} elf_CBinding;



/* nlocals is the stack size required
for this function (includes arity),
nvalues is the number of closure values */
typedef struct elf_Function {
	unsigned char     arity;
	unsigned char   nvalues;
	unsigned char   nlocals;
	int 	           nbytes;
	int 	            bytes;
	int            **protos;
	int              parent;
	elf_String        *name;
	elf_String    *contents;
} elf_Function;


/* todo: convert this to an offset  */
typedef char *Source;


/* first object tag must be OBJ, all other
objects come after it, same order as object
types... */
#define ELF_TAG_LIST(_) _(NIL)_(NUM)_(INT)_(SYS)_(CFN)_(FLOAT2)_(OBJ)_(CLS)_(STR)_(TAB)


#define TAGENUM(NAME) FUSE(TAG_,NAME),
typedef enum { ELF_TAG_LIST(TAGENUM) } elValueTag;
#undef TAGENUM


typedef enum elf_GCColor {
	elf_GC_WHITE = 0, elf_GC_BLACK, elf_GC_RED, elf_GC_PINK, elf_GC_TRAP,
} elf_GCColor;


typedef enum elf_GCTy {
	GC_OBJ = 0, GC_CLS, GC_STR, GC_TAB
} elf_GCTy;


typedef struct elf_Object {
	unsigned  type: 4; // elf_GCTy
	unsigned color: 4; // elf_GCColor
	short     size;
	elf_Table  *metatable;
} elf_Object;


typedef struct elf_Value {
	elf_Int tag;
	union {
		union {
			elf_Int         x_int;
			elf_Num         x_num;
			void           *x_ptr;
			elf_Handle      x_sys;
			elf_Closure    *x_cls;
			elf_Object     *x_obj;
			elf_Table      *x_tab;
			elf_String     *x_str;
			elf_CFunction   x_cfn;
		};
		struct {   int x_i32,y_i32; };
		struct { float x_f32,y_f32; };
	};
} elf_Value;


typedef struct elf_String {
	elf_Object     obj;
	elf_Hash      hash;
	int     	   length;
	char       text[1];
} elf_String;


typedef struct elf_Entry {
	elf_Value key;
	elf_Int   idx;
} elf_Entry;


typedef struct elf_Table {
	elf_Object obj;
	elf_Int    ntotal;
	elf_Int    nslots;
	elf_Int    ncollisions;
	elf_Entry *slots;
	elf_Value *array;
} elf_Table;



elAPI void elf_begin(elf_Shell *R, elf_Module *M);
elAPI void elf_put_nil(elf_Shell *S);
elAPI void elf_put_integer(elf_Shell *S, elf_Int);
elAPI void elf_put_number(elf_Shell *S, elf_Num);
elAPI void elf_put_object(elf_Shell *S, elf_Object *);
elAPI void elf_put_string(elf_Shell *S, elf_String *);
elAPI void elf_put_handle(elf_Shell *S, elf_Handle);
elAPI void elf_new_table(elf_Shell *S, elf_Table *);
elAPI void elf_put_closure(elf_Shell *S, elf_Closure *);
elAPI void elf_put_cfunction(elf_Shell *S, elf_CFunction);


elAPI elf_Int elf_get_integer(elf_Shell *R, int arg);
elAPI elf_Num elf_get_number(elf_Shell *R, int arg);
elAPI elf_String *elf_get_string(elf_Shell *R, int arg);
elAPI char *elf_get_text(elf_Shell *R, int arg);
elAPI elf_Object *elf_get_object(elf_Shell *R, int arg);
elAPI elf_Table *elf_get_table(elf_Shell *R, int arg);
elAPI elf_Handle elf_get_handle(elf_Shell *R, int arg);
elAPI elf_Closure *elf_get_closure(elf_Shell *R, int arg);


elAPI elf_String *elf_new_string(elf_Shell *, const char *text);
elAPI elf_Object *elf_put_new_object(elf_Shell *, elf_Int size);
elAPI elf_Table *elf_put_new_table(elf_Shell *);
elAPI elf_String *elf_new_string2(elf_Shell *, elf_Int length);


/* todo: move to table */
elAPI void elf_tsetx_bindings(elf_Shell *, elf_Table *, elf_CBinding *list, int num);

elAPI void elf_gset_bindings(elf_Shell *, elf_CBinding *list, int num);


elAPI void elf_gsetx_cfn(elf_Shell *R, char *name, elf_CFunction thing);
elAPI void elf_gsetx_int(elf_Shell *R, char *name, elf_Int thing);
elAPI void elf_gsetx_tab(elf_Shell *R, char *name, elf_Table *thing);


elAPI elf_String *elf_alloc_string2(elf_Shell *R, elf_Int length);
elAPI elf_String *elf_alloc_string(elf_Shell *R, const char *text);
elAPI int elf_str_get_length(elf_String *);
elAPI elf_Hash elf_str_get_hash(elf_String *);
elAPI char *elf_str_get_text(elf_String *);
elf_Bool elf_string_eq(elf_String *x, elf_String *y);



#include "src/string.h"
#include "src/table.h"




elAPI void elf_check_args(elf_Shell *R, char *fnname, int n, char *usage);
elAPI elf_Object *elf_get_this(elf_Shell *S);
elAPI elf_Value elf_get_arg(elf_Shell *S, int X);
elAPI elValueTag elf_get_tag(elf_Shell *S, int x);
elAPI int elf_get_num_args(elf_Shell *S);


elAPI int elf_load_file(elf_Shell *, elf_String *name, int nargs, int nregs);
elAPI int elf_call_function(elf_Shell *R, int nargs, int nregs);
elAPI int elf_run(elf_Shell *);

/* todo: why are these public */
elf_Int elf_trigger_collection_cycle(elf_Shell *R);
elf_Int elf_mark_object(elf_Object *obj);

void *elf_alloc_object(elf_Shell *R, elf_GCTy type, elf_Int length);


elAPI void elf_debugger(char *message);
elAPI void elf_fail(elf_Shell *R, int instr, const char *error);



elAPI elf_SymbolId elf_get_global_symbol(elf_Module *M, elf_String *name);
elAPI elf_SymbolId elf_gset(elf_Module *M, elf_String *name, elf_Value v);
elAPI elf_SymbolId elf_add_function(elf_Module *M, elf_Function p);

elAPI int elf_get_instr_file(elf_Module *M, int instr);
elAPI char *elf_get_instr_line(elf_Module *M, int instr);
elAPI void elf_get_line_source_info(char *q, char *loc, int *linenum, char **lineloc);


#define elNUM(thing) (LITERAL(elf_Value){ TAG_NUM, ((union { elf_Num _; float __; elf_Int I; }){thing}).I })
#define elINT(thing) (LITERAL(elf_Value){ TAG_INT, {(elf_Int) UCAST(thing, elf_Int)} })
#define elSYS(thing) (LITERAL(elf_Value){ TAG_SYS, {(elf_Int) UCAST(thing, elf_Handle)} })
#define elTAB(thing) (LITERAL(elf_Value){ TAG_TAB, {(elf_Int) UCAST(thing, elf_Table *)} })
#define elOBJ(thing) (LITERAL(elf_Value){ OBJ2V(thing->type), {(elf_Int) UCAST(thing, elf_Object *)} })
#define elSTR(thing) (LITERAL(elf_Value){ TAG_STR, {(elf_Int) UCAST(thing, elf_String *)} })
#define elCLS(thing) (LITERAL(elf_Value){ TAG_CLS, {(elf_Int) UCAST(thing, elf_Closure *)} })
#define elCFN(thing) (LITERAL(elf_Value){ TAG_CFN, {(elf_Int) UCAST(thing, elf_CFunction)} })
#define elNIL() (LITERAL(elf_Value){TAG_NIL})


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

