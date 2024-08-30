/*
** See Copyright Notice In elf.h
** debug.h
*/


// #define CHUNKSIZE 1024
// #define CHUNKCATE(x,y) ((x+y-1)/y*y)


#define FLYTRAP 0x55555555


typedef struct DBGSource {
	char const *fileName;
	int lineNumber;
	char const *func;
} DBGSource;


#define DBG_SOURCE (DBGSource){__FILE__,__LINE__,__func__}


#if defined(_DEBUG)
	#define DEBUG_CODE(xx) do { xx; } while(0)
#else
	#define DEBUG_CODE(xx)
#endif


#if !defined(NO_CODE)
	#define NO_CODE elf_debugger(__FILE__" ["TO_TEXT(__LINE__)"]: internal error: unexpected code branch")
#endif


#if defined(_DEBUG)
	#define CHECK_FORMAT(FORMAT,...) ((0)?(snprintf(0,0,FORMAT,##__VA_ARGS__),0):0)
#else
	#define CHECK_FORMAT(FORMAT,...) 0
#endif



#if !defined(ASSERT_ALWAYS)
	#define ASSERT_ALWAYS(xx) do { if (!(xx)) assertion_function(DBG_SOURCE,TO_TEXT(xx)); } while(0)
#endif


#if !defined(ASSERT)
	#if defined(_DEBUG)
		#define ASSERT(xx) ASSERT_ALWAYS(xx)
	#else
		#define ASSERT(xx)
	#endif
#endif


static void set_assertion_hook(int (*fn)(DBGSource));
static void assertion_function(DBGSource info, char const *message);

