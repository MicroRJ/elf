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


#if !defined(ASSERT_ALWAYS)
	#define ASSERT_ALWAYS(xx) do { if (!(xx)) assertion_function(DBG_SOURCE,XTEXT(xx)); } while(0)
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

