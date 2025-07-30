//
// See Copyright Notice In elf.h
//


// common macro definitions for build configuration
#if defined(__EMSCRIPTEN__)
   #define THREAD static
   #define global static
#else
   #define THREAD static __declspec(thread)
   #define global static
#endif

#if !defined(INTERNAL)
	#define INTERNAL static
#endif


// not too necessary
#if !defined(__cplusplus)
	#define XLITERAL(X) (X)
#else
	#define XLITERAL(X) X
#endif


// todo: do this properly!
#if defined(_DEBUG)
	#define CHECK_FORMAT(FORMAT,...) ((0)?(snprintf(0,0,FORMAT,##__VA_ARGS__),0):0)
#else
	#define CHECK_FORMAT(FORMAT,...) 0
#endif


// bothers me
#define __FUNC__ __func__


// common utility macros and functions

#define XTEXT_(X) #X
#define XTEXT(X) XTEXT_(X)

#define XFUSE_(X,Y) X##Y
#define XFUSE(X,Y) XFUSE_(X,Y)



#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define KILOBYTES(x) ((x) << 10)
#define MEGABYTES(x) ((x) << 20)
#define GIGABYTES(x) ((x) << 30)
#define WITHIN(X,XMIN,XMAX) ((XMIN) <= (X) && (X) < (XMAX))
#define COUNTOF(X) (sizeof(X) / sizeof((X)[0]))
#define STATIC_ASSERT(x) typedef char _static_assert_[x ? 1 : -1]


static char *get_name_from_file_path(const char *p);
static void get_source_info(char *source, char *cursor, int *line_number, char **line_start);
static elf_Bool is_eol_chr(char x);
static elf_Bool is_digit_chr(char x);
static elf_Bool is_lowercase_chr(char x);
static elf_Bool is_uppercase_chr(char x);
static elf_Bool is_letter_chr(char x);
static elf_Bool is_letter_or_digit_chr(char x);
static char chr_to_uppercase(char x);
static char chr_to_lowercase(char x);
static int text_length(char const *text);
static elf_Bool text_eql(char const *x, char const *y, int n);
static elf_Bool text_eq(char const *x, char const *y);
static char *copy_text2(int length, char const *string);
static elf_Bool match_entire_text_noclause(char *pattern, char *text);
static elf_Bool match_entire_text(char *pattern, char *text);
static char *match_text_single_clause_ex(char *p, char *s);
static char *thread_format_v(char const *format, va_list v);


#define elf_tpf(format,...) (tpf_(format,__VA_ARGS__))
