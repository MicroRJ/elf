//
// See Copyright Notice In elf.h
//


#if defined(PLATFORM_WEB)
	#include "em/platform_em.c"
#elif defined(PLATFORM_DESKTOP)
	#include "win32/platform_win32.c"
#else
	#include "sys_stub.c"
#endif


