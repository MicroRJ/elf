/*
** See Copyright Notice In elf.h
** elf.c
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
   #include <emscripten.h>
   #include <unistd.h>
#endif


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4100)
#pragma warning(disable:4245)
#pragma warning(disable:4057)
#pragma warning(disable:4189)
#pragma warning(disable:4201)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4389)
#pragma warning(disable:4996)
#endif
/* both __clang__ and _MSC_VER can be defined
at the same time when using clang-cl */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
#pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wpointer-sign"
#pragma clang diagnostic ignored "-Wunused-function"
#endif


#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "stb/stb_sprintf.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb/stb_leakcheck.h"

#include "elf.h"

enum{true=1,false=0};

#define STATIC_ASSERT(x) typedef char _static_assert_[x ? 1 : -1]


STATIC_ASSERT(sizeof(elf_Int)==sizeof(elf_i64));
STATIC_ASSERT(sizeof(elf_Num)==sizeof(elf_f64));

STATIC_ASSERT(sizeof(elf_i64)==8);
STATIC_ASSERT(sizeof(elf_f64)==8);


#if defined(__EMSCRIPTEN__)
   #define THREAD static
   #define GLOBAL static
#else
   #define THREAD static __declspec(thread)
   #define GLOBAL static
#endif

#define INTERNAL static

#if !defined(MAX)
   #define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#if !defined(MIN)
   #define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif


#if !defined(WITHIN)
   #define WITHIN(X,XMIN,XMAX) ((XMIN) <= (X) && (X) < (XMAX))
#endif


#if !defined(MAX_PATH)
   #define MAX_PATH 0xff
#endif


#if defined(_DEBUG)
	#define CHECK_FORMAT(FORMAT,...) ((0)?(snprintf(0,0,FORMAT,##__VA_ARGS__),0):0)
#else
	#define CHECK_FORMAT(FORMAT,...) 0
#endif


#if !defined(__cplusplus)
	#define XLITERAL(X) (X)
#else
	#define XLITERAL(X) X
#endif


/* so I think is a C feature only, and it allows you
to cast to explict types, so the user should get a warning
if the type isn't one of the given ones? */
#define UCAST(D,T) ( ((union { T _; }){D})._ )


#define XTEXT_(X) #X
#define XTEXT(X) XTEXT_(X)


#define XFUSE_(X,Y) X##Y
#define XFUSE(X,Y) XFUSE_(X,Y)


#if !defined(COUNTOF)
	#define COUNTOF(X) (sizeof(X)/sizeof((X)[0]))
#endif


/* call elf debugger when reached */
#if !defined(NO_CODE)
	#define NO_CODE elf_debugger(__FILE__" ["XTEXT(__LINE__)"]: internal error: unexpected code branch")
#endif


#define NO_BYTE (-1)

// #define CHUNKSIZE 1024
// #define CHUNKCATE(x,y) ((x+y-1)/y*y)

#define FLYTRAP 0x55555555


typedef struct Debug_Source {
	char const *fileName;
	int lineNumber;
	char const *func;
} Debug_Source;

#define DBG_SOURCE (Debug_Source){__FILE__,__LINE__,__func__}

#if !defined(ASSERT_ALWAYS)
#define ASSERT_ALWAYS(xx) \
do { \
	if (!(xx)){ \
		printf("%s[%i] %s(): '%s' triggered assertion\n",__FILE__,__LINE__,__func__,#xx); \
		__debugbreak(); \
	}; \
} while(0)
#endif

#if !defined(ASSERT)
	#if defined(_DEBUG)
		#define ASSERT(xx) ASSERT_ALWAYS(xx)
	#else
		#define ASSERT(xx)
	#endif
#endif


#define ISOBJT(tag) ((tag)>=elf_tag_userobj)
#define INTORNUM(tag) (((tag)==elf_tag_num)||((tag)==elf_tag_int))
#define ISNILV(X) (((X).tag==elf_tag_nil)||(ISOBJT((X).tag)&&(X).x_obj==0))


#define VI2N(X) (((X).tag==elf_tag_int) ? (elf_Num) (X).x_int : (X).x_num)
#define VN2I(X) (((X).tag==elf_tag_num) ? (elf_Int) (X).x_num : (X).x_int)


#define POBJ(thing) ((elf_Object*)(thing))
#define OBJ_COLOR(thing) (POBJ(thing)->color)


#define OBJ2V(ty) (elf_tag_userobj+ty)


#define GET_FRAME(S) ((S)->frame)
#define GET_LOCAL(S,X) (GET_FRAME(S)->locals[X])


#define GET_TOP(S)   ((S)->stack_ptr)
#define SET_TOP(S,X) (GET_TOP(S) = UCAST(X, elf_Value *))


static void _debug_stack_push(elf_State *S, elf_Value v);

#if defined(_DEBUG)
#define PUSHV(S,X) _debug_stack_push(S,X)
#else
#define PUSHV(S,X) (* GET_TOP(S) ++ = (X))
#endif


#define VNIL() (XLITERAL(elf_Value){elf_tag_nil})
#define VNUM(thing) (XLITERAL(elf_Value){ elf_tag_num, ((union { elf_Num _; float __; elf_Int I; }){thing}).I })
#define VALUE_INTEGER(thing) (XLITERAL(elf_Value){ elf_tag_int, {(elf_Int) UCAST(thing, elf_Int)} })
#define VALUE_TABLE(thing) (XLITERAL(elf_Value){ elf_tag_tab, {(elf_Int) UCAST(thing, elf_Table *)} })
#define VOBJ(thing) (XLITERAL(elf_Value){ OBJ2V(thing->type), {(elf_Int) UCAST(thing, elf_Object *)} })
#define VALUE_STRING(thing) (XLITERAL(elf_Value){ elf_tag_str, {(elf_Int) UCAST(thing, elf_String *)} })
#define VCLS(thing) (XLITERAL(elf_Value){ elf_tag_closure, {(elf_Int) UCAST(thing, elf_Closure *)} })
#define VALUE_FUNCTION(thing) (XLITERAL(elf_Value){ elf_tag_proc, {(elf_Int) UCAST(thing, elf_Function)} })
// todo: remove this, this is unnecessary
#define VSYS(thing) (XLITERAL(elf_Value){ elf_tag_sysobj, {(elf_Int) UCAST(thing, elf_Handle)} })

#include "src/alloc.h"
#include "src/array.c"
#include "src/log.h"
#include "src/text.h"
#include "src/system.h"
#include "src/byte.h"

#include "src/parse.h"

#include "src/log.c"
#include "src/alloc.c"
#include "src/text.c"

#include "src/table.c"
#include "src/string.c"
#include "src/elf-aux.c"


#include "src/lexer.c"
#include "src/bytecode_gen.c"
#include "src/tree.c"
#include "src/parse.c"
#include "src/user.c"
#include "src/lib_core.h"
#include "src/lib_math.c"
#include "src/lib_core.c"
#include "src/lib_time.c"
#if defined(_WIN32)
#include "src/lib_win32.c"
#endif
#include "src/system.c"
#include "src/lib_table.c"
#include "src/lib_array.c"
#include "src/lib_random.c"
#include "src/objects.c"
#include "src/core.c"


#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
