/*
** See Copyright Notice In elf.h
** text.h
** Text Tools
*/


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
static char *copy_text2(Allocator fn, int length, char const *string);
static char *copy_text(Allocator fn, char const *contents);
static elf_Bool match_entire_text_noclause(char *pattern, char *text);
static elf_Bool match_entire_text(char *pattern, char *text);
static char *match_text_single_clause_ex(char *p, char *s);

static char *xpfv(Allocator alloc, char const *format, va_list v);
static char *xpf_(Allocator alloc, char const *format, ...);

static char *tpfv(char const *format, va_list v);
static char *tpf_(char const *format, ...);

// CHECK_FORMAT(format,__VA_ARGS__),
// CHECK_FORMAT(format,__VA_ARGS__),
#define elf_tpf(format,...) (tpf_(format,__VA_ARGS__))
#define elf_xpf(alloc,format,...) (xpf_(alloc,format,__VA_ARGS__))
