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


#define elGC_MEM_THRESHOLD_MIN (elf_Int) MEGABYTES(1)
#define elGC_MEM_THRESHOLD_MAX (elf_Int) MEGABYTES(1024)

#define elGC_OBJ_THRESHOLD_MIN (elf_Int) ((1024)*2)
#define elGC_OBJ_THRESHOLD_MAX (elf_Int) ((1024)*8)


#define DEFAULT_STACK_SIZE 4096


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


typedef int Instr;
typedef char *Source;


typedef struct elf_Closure {
	elf_Node       obj;
	elf_Proto   proto;
	elf_Value  values[1];
} elf_Closure;

/* */
typedef struct elf_Module {
	elf_Table      *globals;
	elf_Table      *strings;
	elf_Num        *numbers;
	elf_Int       *integers;
	elf_File         *files;
	elf_Proto       *protos;
	Source           *lines;
	Bytecode         *bytes;
	Instr            nbytes;
} elf_Module;


typedef struct delaylist delaylist;
typedef struct delaylist {
	delaylist *n;
	Instr j;
} delaylist;


typedef struct elf_StackFrame elf_StackFrame;
typedef struct elf_StackFrame {
	elf_StackFrame    *caller;
	elf_Closure      *closure;
	elf_Value         *locals;
	int               nlocals;
	char                nargs;
	char                nrets;
	int                origin;
	delaylist    * delay_list;
	elf_Bool          logging;
} elf_StackFrame;


#define elf_GC_PHASE_MARK elf_GC_WHITE
#define elf_GC_PHASE_FREE elf_GC_BLACK


typedef struct elf_Collector {
	elf_Bool     paused;
	int          phase;
	elf_Int      memory_allocated;
	elf_Int      memory_threshold;
	elf_Node **new_objects;
	elf_Node **objects;
	/* this changes dynamically based on
	object min threshold, it tends to
	be around there... */
	elf_Int      object_trigger_threshold;
} elf_Collector;


#define FLAG_DEBUGGER         (1 << 0)
#define FLAG_DEBUGGER_ONCALL  (1 << 1)
#define FLAG_BYTETRACKING     (1 << 2)
#define FLAG_BYTELOGGING      (1 << 3)


typedef struct elf_State {
	elf_Module     *M;
	elf_Value      *stack;
	int             stack_max;
	elf_Value      *stack_ptr;

	elf_StackFrame  first_frame;
	elf_StackFrame *frame;
	int            nframe;
	int             flags;

	struct {
		elf_Table *integer;
		elf_Table *number;
		elf_Table *string;
		elf_Table *table;
	} metatables;
	struct {
		elf_Value oncall;
		elf_Value ongc;
	} hooks;
   /* the current instruction */
	Instr byte;
	elf_Collector collector;
} elf_State;


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
#include "src/parse.c"
#include "src/user.c"
#include "src/lib_math.h"
#include "src/lib_core.h"
#include "src/lib_math.c"
#include "src/lib_core.c"
#if defined(_WIN32)
#include "src/lib_win32.c"
#endif
#include "src/system.c"
#include "src/lib_table.c"
#include "src/lib_array.c"
#include "src/core.c"


#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
