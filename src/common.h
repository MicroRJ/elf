//
// See Copyright Notice In elf.h
//



#define Static_Data static

// common macro definitions
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



// todo: remove!
#define FOR_RANGE(N,X,Y) for (int N = X; N < Y; N += 1)


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


// #define STATIC_ASSERT(x) typedef char _static_assert_[x ? 1 : -1]
#define STATIC_ASSERT(x) _Static_assert(x, "no message")


static char *get_name_from_file_path(const char *p);
static int get_source_info(char *source, char *cursor, char **line_start);
static bool is_eol_chr(char x);
static bool is_digit_chr(char x);
static bool is_lowercase_chr(char x);
static bool is_uppercase_chr(char x);
static bool is_letter_chr(char x);
static bool is_letter_or_digit_chr(char x);
static char chr_to_uppercase(char x);
static char chr_to_lowercase(char x);
static int text_l(char const *text);
static bool text_eql(char const *x, char const *y, int n);
static bool text_eq(char const *x, char const *y);
static char *copy_text2(int length, char const *string);
static char *thread_format_v(char const *format, va_list v);

static char *temporary_format_v(char const *format, va_list vargs);
static char *temporay_format(char const *format, ...);
#define tpf temporay_format
