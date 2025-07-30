//
// See Copyright Notice In elf.h
// elf.c
//
#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define KILOBYTES(x) ((x) << 10)
#define MEGABYTES(x) ((x) << 20)
#define GIGABYTES(x) ((x) << 30)

#define WITHIN(X,XMIN,XMAX) ((XMIN) <= (X) && (X) < (XMAX))
#define COUNTOF(X) (sizeof(X) / sizeof((X)[0]))

#define STATIC_ASSERT(x) typedef char _static_assert_[x ? 1 : -1]

#if defined(_DEBUG)
	#define CHECK_FORMAT(FORMAT,...) ((0)?(snprintf(0,0,FORMAT,##__VA_ARGS__),0):0)
#else
	#define CHECK_FORMAT(FORMAT,...) 0
#endif

/* so I think is a C feature only, and it allows you
to cast to explict types, so the user should get a warning
if the type isn't one of the given ones? */
#define UCAST(D,T) ( ((union { T _; }){D})._ )

#define XTEXT_(X) #X
#define XTEXT(X) XTEXT_(X)


#define XFUSE_(X,Y) X##Y
#define XFUSE(X,Y) XFUSE_(X,Y)

#if !defined(__cplusplus)
	#define XLITERAL(X) (X)
#else
	#define XLITERAL(X) X
#endif

#define FOR_RANGE(N,X,Y) for (int N = X; N < Y; N += 1)

// #if !defined(MAX_PATH)
//    #define MAX_PATH 256
// #endif

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

#endif