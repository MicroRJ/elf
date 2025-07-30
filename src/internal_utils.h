//
// See Copyright Notice In elf.h
// elf.c
//
#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H

// todo: remove this file!


/* so I think is a C feature only, and it allows you
to cast to explict types, so the user should get a warning
if the type isn't one of the given ones? */

// todo: @deprecated!
#define UCAST(D,T) ( ((union { T _; }){ D })._ )

// todo: @deprecated!
#define FOR_RANGE(N,X,Y) for (int N = X; N < Y; N += 1)


#if !defined(NO_CODE)
	#define NO_CODE __debugbreak();
#endif


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


#define ASSERT(xx) ASSERT_ALWAYS(xx)
//#if !defined(ASSERT)
//	#if defined(_DEBUG)
//		#define ASSERT(xx) ASSERT_ALWAYS(xx)
//	#else
//		#define ASSERT(xx)
//	#endif
//#endif

#endif