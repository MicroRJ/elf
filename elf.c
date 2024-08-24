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


/* Configuration Macros (mostly temporary)
*/

#define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(1)
#define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(1024)

#define elGC_OBJ_THRESHOLD_MIN (elInteger) ((1024)*2)
#define elGC_OBJ_THRESHOLD_MAX (elInteger) ((2048)*4)


#define elDEFAULT_STACK_SIZE 4096


#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
	#include "stb/stb_sprintf.h"
#endif
#ifndef STB_LEAKCHECK_IMPLEMENTATION
#define STB_LEAKCHECK_IMPLEMENTATION
	#include "stb/stb_leakcheck.h"
#endif

#include "elf.h"


/* few utility macros */
#if !defined(MAX)
	#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#if !defined(MIN)
	#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#if !defined(WITHIN)
	#define WITHIN(X,XMIN,XMAX) ((XMIN) <= (X) && (X) < (XMAX))
#endif

#if !defined(MEGABYTES)
	#define MEGABYTES(x) ((x)*1024LLU*1024LLU)
#endif
#if !defined(GIGABYTES)
	#define GIGABYTES(x) ((x)*1024LLU*1024LLU*1024LLU)
#endif
#if !defined(MAX_PATH)
	#define MAX_PATH 0xff
#endif


#include "src/error.h"
#include "src/debug.h"
#include "src/alloc.h"
#include "src/array.h"
#include "src/log.h"
#include "src/text.h"
#include "src/system.h"
#include "src/byte.h"


typedef int Instr;


/* symbols are mapped at load time, so the code generator
references globals by index... */
typedef struct elModule {
	elTable *globals;
	int *track;
	Bytecode *bytes;
	Instr nbytes;
	char **lines;
	elFunction *files;
	elTable *strings;
	elNumber *numbers;
	elInteger *integers;
	elFunction *functions;
} elModule;


typedef struct elf_delaylist elf_delaylist;
typedef struct elf_delaylist {
	elf_delaylist *n;
	Instr j;
} elf_delaylist;


typedef struct elStackFrame elStackFrame;
typedef struct elStackFrame {
	elStackFrame   *caller;
	elClosure      *closure;
	elValue        *locals;
	int            nlocals;
	char			     nargs;
	char			     nregs;
	int             origin;
	elf_delaylist * delay_list;
	elBool 			 logging;
} elStackFrame;


typedef enum elGCColor elGCPhase;
#define elGC_PHASE_HOLD GC_WHITE
#define elGC_PHASE_FREE GC_BLACK


typedef struct elCollector {
	elBool     paused;
	elGCPhase  phase;
	elInteger  memory_allocated;
	elInteger  memory_threshold;
	elObject **new_objects;
	elObject **objects;
	/* this changes dynamically based on
	object min threshold, it tends to
	be around there... */
	elInteger  object_trigger_threshold;
} elCollector;


#define FLAG_DEBUGGER 			(1 << 0)
#define FLAG_DEBUGGER_ONCALL 	(1 << 1)
#define FLAG_BYTETRACKING 		(1 << 2)
#define FLAG_BYTELOGGING 		(1 << 3)


typedef struct elState {
	elModule *M;
	elValue  *stack;
	int 		 stack_max;
	elValue  *stack_ptr;

	elStackFrame *frame;
	int          nframe;
	int           flags;

	struct {
		elTable *integer;
		elTable *number;
		elTable *string;
		elTable *table;
	} metatables;
	struct {
		elValue oncall;
		elValue ongc;
	} hooks;
	/* todo: remove */
	struct {
		elString *x,*y,*z,*w;
		elString *width,*height;
		elString *__add,*__sub,*__mul,*__div;
		elString *__add1,*__sub1,*__mul1,*__div1;
		elString *__getfield,*__setfield;
		elString *__hash;
	} cache;
	/* the current instruction */
	Instr byte;
	union { elCollector collector, memory; };
} elState;


#include "src/node.h"
#include "src/file.h"

#include "src/debug.c"
#include "src/log.c"
#include "src/array.c"
#include "src/elf-mem.c"
#include "src/text.c"

#include "src/table.c"
#include "src/string.c"
#include "src/elf-aux.c"


#include "src/elf-obj.c"
// #include "src/elf-chr.c"
#include "src/elf-node.c"
#include "src/lexer.c"
#include "src/emit.c"
#include "src/file.c"
#include "src/node.c"
/* todo: remove this */
#include "src/elf-api.c"
#include "src/elf-lib.c"
#include "src/runtime.c"
#include "src/system.c"


#if 0
#include "src/elf-cli.c"

int main(int n, char **c) {
	(void) n;
	elf_cliopts cli = {0};
	if (elf_loadcliopts(&cli,n,c)) return 0;

	elModule M = {0};
	elState R = {0};
	elf_begin(&R,&M);
	if (cli.logging) R.bytelogging = 1;

	elStackFrame frame = {0};
	frame.base = R.top;
	R.frame = &frame;

	if (cli.filename != 0) {
		elString *filename = elf_put_new_string(&R,cli.filename);
		/* todo: remove this?? */
		filename->obj.color = GC_PINK;
		FileState fs = {0};
		elf_parse_file_fs(&R,&fs,filename,0,0);
	}
	if (cli.dump) {
		FILE *dumpf = stdout;
		if (strcmp(cli.dumpfilename,"stdout")) {
			dumpf = fopen(elf_tpf("%s.module.ignore",cli.dumpfilename),"wb");
		}
		if (dumpf == 0) {
			printf("error: could open specified dump file for writting");
		} else {
			lang_dumpmodule(&M,dumpf);
			if (dumpf != stdout) fclose(dumpf);
		}
	}
	sys_console_print(LOG_KINFO,"exited");
	return 0;
}

#endif


#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
