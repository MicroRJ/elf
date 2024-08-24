/*
** See Copyright Notice In elf.h
** text.h
** Text Tools
*/


static elBool is_eol_chr(char x);
static elBool is_digit_chr(char x);
static elBool is_lowercase_chr(char x);
static elBool is_uppercase_chr(char x);
static elBool is_letter_chr(char x);
static elBool is_letter_or_digit_chr(char x);
static char chr_to_uppercase(char x);
static char chr_to_lowercase(char x);
static int text_length(char const *text);
static elBool text_eql(char const *x, char const *y, int n);
static elBool text_eq(char const *x, char const *y);
static char *copy_text2(elAllocator fn, int length, char const *string);
static char *copy_text(elAllocator fn, char const *contents);
static elBool match_entire_text_noclause(char *pattern, char *text);
static elBool match_entire_text(char *pattern, char *text);
static char *match_text_single_clause_ex(char *p, char *s);

static char *xpf(elAllocator fn, char const *format, va_list v);
static char *tpfv(char const *format, va_list v);
static char *tpf_(char const *format, ...);

#define elf_tpf(format,...) (CHECK_FORMAT(format,__VA_ARGS__),tpf_(format,__VA_ARGS__))

