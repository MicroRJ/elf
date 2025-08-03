//
// See Copyright Notice In elf.h
//


#if defined(PLATFORM_WEB)
	#include "sys_em.c"
#elif defined(PLATFORM_DESKTOP)
	#include "sys_win32.c"
#else
	#include "sys_stub.c"
#endif


