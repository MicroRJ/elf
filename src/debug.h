/*
** See Copyright Notice In elf.h
** debug.h
** Debug Stuff
*/


typedef struct SourceInfo {
	char const *fileName;
	int lineNumber;
	char const *func;
	char const *lineStart;
	char const *fileStart;
} SourceInfo;


#define DEBUG_HERE (SourceInfo){__FILE__,__LINE__,__func__}


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
	#define ASSERT_ALWAYS(xx) do { if (!(xx)) assertion_function(DEBUG_HERE,TO_TEXT(xx)); } while(0)
#endif


#if !defined(ASSERT)
	#if defined(_DEBUG)
		#define ASSERT(xx) ASSERT_ALWAYS(xx)
	#else
		#define ASSERT(xx)
	#endif
#endif


static void set_assertion_hook(int (*hook)(SourceInfo));
static void assertion_function(SourceInfo info, char const *message);

