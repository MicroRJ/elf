/*
** See Copyright Notice In elf.h
** elf.c
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
   #include <emscripten.h>
   #include <unistd.h>
#endif


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4100)
#pragma warning(disable:4245)
#pragma warning(disable:4057)
#pragma warning(disable:4189)
#pragma warning(disable:4201)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4389)
#pragma warning(disable:4996)
#endif
/* both __clang__ and _MSC_VER can be defined
at the same time when using clang-cl */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
#pragma clang diagnostic ignored "-Wnon-literal-null-conversion"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wpointer-sign"
#pragma clang diagnostic ignored "-Wunused-function"
#endif


#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "stb/stb_sprintf.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb/stb_leakcheck.h"


#include "elf.h"

#include "src/help.h"
#include "src/error.h"
#include "src/debug.h"
#include "src/alloc.h"
#include "src/array.h"
#include "src/log.h"
#include "src/text.h"
#include "src/system.h"
#include "src/byte.h"

#include "src/parse.h"

#include "src/debug.c"
#include "src/log.c"
#include "src/array.c"
#include "src/alloc.c"
#include "src/text.c"

#include "src/closure.c"
#include "src/table.c"
#include "src/string.c"
#include "src/elf-aux.c"


#include "src/lexer.c"
#include "src/gen.c"
#include "src/tree.c"
#include "src/parse.c"
#include "src/user.c"
#include "src/lib_math.h"
#include "src/lib_core.h"
#include "src/lib_math.c"
#include "src/lib_core.c"
#include "src/lib_time.c"
#if defined(_WIN32)
#include "src/lib_win32.c"
#endif
#include "src/system.c"
#include "src/lib_table.c"
#include "src/lib_array.c"
#include "src/obj.c"
#include "src/core.c"


#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
