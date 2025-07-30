//
// See Copyright Notice In elf.h
//

#if defined(__EMSCRIPTEN__)
	#define ELF_API 	 EMSCRIPTEN_KEEPALIVE
	#define ELF_EXPORT EMSCRIPTEN_KEEPALIVE
#else
	#define ELF_EXPORT __declspec(dllexport)

	#if defined(BUILD_STATIC)
		#define ELF_API static
	#else
		#define ELF_API
	#endif
#endif
