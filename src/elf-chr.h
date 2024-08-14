/*
** See Copyright Notice In elf.h
** elf-chr.h
** Text Tools
*/


elBool elf_chriseol(char x);
elBool elf_is_digit_char(char x);
elBool elf_chrislowercase(char x);
elBool elf_chrisuppercase(char x);
elBool elf_is_letter_char(char x);
elBool elf_is_letter_or_digit_char(char x);
char elf_chrtouppercase(char x);
char elf_chrtolowercase(char x);



int elf_textlength(char const *s);
elBool elf_texteql(char const *x, char const *y, int n);
elBool elf_texteq(char const *x, char const *y);
char *elf_copyltext(elAllocator *allocator, int length, char const *string);
char *elf_copytext(elAllocator *allocator, char const *string);
char *S_pfv(elAllocator *cator, char const *format, va_list v);
char *S_tpfv(char const *format, va_list v);
char *S_tpf_(char const *format, ...);
elBool elf_match_entire_string_noclause(char *p, char *s);
elBool elf_match_entire_string(char *p, char *s);



elBool elf_match_entire_string(char *p, char *s);
char *elf_copytext(elAllocator *cator, char const *contents);
int elf_textlength(char const *contents);
elBool elf_texteq(char const *x, char const *y);
unsigned int S_hashcontents (char const *contents, unsigned int length);
char *S_tpf_(char const *format, ...);
#define elf_tpf(format,...) (LCHECKPRINTF(format,__VA_ARGS__),S_tpf_(format,__VA_ARGS__))

