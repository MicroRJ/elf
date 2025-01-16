/*
** See Copyright Notice In elf.h
** help.h
*/
//todo: rename to base

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


#if !defined(MEGABYTES)
   #define MEGABYTES(x) ((x)*1024LLU*1024LLU)
#endif
#if !defined(GIGABYTES)
   #define GIGABYTES(x) ((x)*1024LLU*1024LLU*1024LLU)
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


/* cast to union types, c feature */
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


#define ISOBJT(tag) ((tag)>=elf_tag_userobj)
#define INTORNUM(tag) (((tag)==elf_tag_num)||((tag)==elf_tag_int))
#define CAN_CALL(tag) (((tag)==elf_tag_closure)||((tag)==elf_tag_proc))
#define ISNILV(X) (((X).tag==elf_tag_nil)||(ISOBJT((X).tag)&&(X).x_obj==0))


#define VI2N(X) (((X).tag==elf_tag_int) ? (elf_Num) (X).x_int : (X).x_num)
#define VN2I(X) (((X).tag==elf_tag_num) ? (elf_Int) (X).x_num : (X).x_int)


#define POBJ(thing) ((elf_Node*)(thing))
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
#define VINT(thing) (XLITERAL(elf_Value){ elf_tag_int, {(elf_Int) UCAST(thing, elf_Int)} })
#define VSYS(thing) (XLITERAL(elf_Value){ elf_tag_sysobj, {(elf_Int) UCAST(thing, elf_Handle)} })
#define VTAB(thing) (XLITERAL(elf_Value){ elf_tag_tab, {(elf_Int) UCAST(thing, elf_Table *)} })
#define VOBJ(thing) (XLITERAL(elf_Value){ OBJ2V(thing->type), {(elf_Int) UCAST(thing, elf_Node *)} })
#define VSTR(thing) (XLITERAL(elf_Value){ elf_tag_str, {(elf_Int) UCAST(thing, elf_String *)} })
#define VCLS(thing) (XLITERAL(elf_Value){ elf_tag_closure, {(elf_Int) UCAST(thing, elf_Closure *)} })
#define VCFN(thing) (XLITERAL(elf_Value){ elf_tag_proc, {(elf_Int) UCAST(thing, elf_Function)} })



