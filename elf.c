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

#define elGC_MEM_THRESHOLD_MIN (elf_Int) MEGABYTES(1)
#define elGC_MEM_THRESHOLD_MAX (elf_Int) MEGABYTES(1024)

#define elGC_OBJ_THRESHOLD_MIN (elf_Int) ((1024)*2)
#define elGC_OBJ_THRESHOLD_MAX (elf_Int) ((2048)*4)


#define elDEFAULT_STACK_SIZE 4096


#if defined(__EMSCRIPTEN__)
   #define THREAD static
   #define GLOBAL static
#else
   #define THREAD static __declspec(thread)
   #define GLOBAL static
#endif


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


#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "stb/stb_sprintf.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb/stb_leakcheck.h"


#include "elf.h"

#include "src/error.h"
#include "src/debug.h"
#include "src/alloc.h"
#include "src/array.h"
#include "src/log.h"
#include "src/text.h"
#include "src/system.h"
#include "src/byte.h"


typedef int Instr;


typedef struct elf_Closure {
   elf_Object       obj;
   elf_Shell     *state;
   elf_Function   proto;
   elf_Value  values[1];
} elf_Closure;



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
   char                nregs;
   int                origin;
   delaylist    * delay_list;
   elf_Bool            logging;
} elf_StackFrame;


#define elf_GC_PHASE_MARK elf_GC_WHITE
#define elf_GC_PHASE_FREE elf_GC_BLACK


typedef struct elf_Collector {
   elf_Bool       paused;
   int          phase;
   elf_Int      memory_allocated;
   elf_Int      memory_threshold;
   elf_Object **new_objects;
   elf_Object **objects;
   /* this changes dynamically based on
   object min threshold, it tends to
   be around there... */
   elf_Int      object_trigger_threshold;
} elf_Collector;


/* symbols are mapped at load time, so the code generator
references globals by index... */
typedef struct elf_Module {
   elf_Table      *globals;
   int              *track;
   elf_Bytecode     *bytes;
   Instr            nbytes;
   Source           *lines;
   elf_Function     *files;
   elf_Table      *strings;
   elf_Num        *numbers;
   elf_Int       *integers;
   elf_Function *functions;
} elf_Module;


#define FLAG_DEBUGGER         (1 << 0)
#define FLAG_DEBUGGER_ONCALL  (1 << 1)
#define FLAG_BYTETRACKING     (1 << 2)
#define FLAG_BYTELOGGING      (1 << 3)


typedef struct elf_Shell {
   elf_Module     *M;
   elf_Value      *stack;
   int             stack_max;
   elf_Value      *stack_ptr;
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
   union { elf_Collector collector, memory; };
} elf_Shell;


#include "src/node.h"
#include "src/file.h"
#include "src/help.h"

#include "src/debug.c"
#include "src/log.c"
#include "src/array.c"
#include "src/elf-mem.c"
#include "src/text.c"

#include "src/closure.c"
#include "src/table.c"
#include "src/string.c"
#include "src/elf-aux.c"


#include "src/lexer.c"
#include "src/node.c"
#include "src/emit.c"
#include "src/file.c"
#include "src/user.c"
#include "src/libcore.c"
#include "src/libtable.c"
#include "src/runtime.c"
#include "src/system.c"


#if 0
#include "src/elf-cli.c"

int main(int n, char **c) {
   (void) n;
   elf_cliopts cli = {0};
   if (elf_loadcliopts(&cli,n,c)) return 0;

   elf_Module M = {0};
   elf_Shell R = {0};
   elf_begin(&R,&M);
   if (cli.logging) R.bytelogging = 1;

   elf_StackFrame frame = {0};
   frame.base = R.top;
   R.frame = &frame;

   if (cli.filename != 0) {
      elf_String *filename = elf_new_string(&R,cli.filename);
      /* todo: remove this?? */
      filename->obj.color = elf_GC_PINK;
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
