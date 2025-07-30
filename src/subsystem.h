//
// See Copyright Notice In elf.h
//
#ifndef SUBSYSTEM_H
#define SUBSYSTEM_H

//
// Numerous functions and definitions that depend on
// each other and create a sort of underlying system.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


#include "system.h"


#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "stb_sprintf.h"


// #define STB_LEAKCHECK_IMPLEMENTATION
// #include "stb_leakcheck.h"


typedef struct Debug_Source {
	char const *fileName;
	int lineNumber;
	char const *func;
} Debug_Source;

#define DBG_SOURCE (Debug_Source){__FILE__,__LINE__,__func__}

#include "internal_utils.h"
#include "internal_configs.h"

// #include "sub_alloc.h"
#include "thread_alloc.c"
#include "sub_text.h"

#include "sub_array.c"

#include "sub_log.c"
// #include "sub_alloc.c"
#include "sub_text.c"
#include "sub_profile.c"


#endif