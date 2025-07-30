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

#include "internal_utils.h"

#include "common.h"
#include "thread_alloc.c"

#include "sub_array.c"

// #include "sub_alloc.c"
#include "common.c"


#endif