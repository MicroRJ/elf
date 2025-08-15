//
// See Copyright Notice In elf.h
//

#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H


#include <stdlib.h>
#include <stdio.h>



#if !defined(NO_CODE)
	#define NO_CODE __debugbreak();
#endif

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