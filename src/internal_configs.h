//
// See Copyright Notice In elf.h
//

#if defined(__EMSCRIPTEN__)
   #define THREAD static
   #define GLOBAL static
#else
   #define THREAD static __declspec(thread)
   #define GLOBAL static
#endif

#if !defined(INTERNAL)
	#define INTERNAL static
#endif