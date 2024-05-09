/*
** See Copyright Notice In elf.h
** elf-chr.h
** Text Tools
*/


elf_bool elf_chriseol(char x);
elf_bool elf_chrisdigit(char x);
elf_bool elf_chrislowercase(char x);
elf_bool elf_chrisuppercase(char x);
elf_bool elf_chrisletter(char x);
elf_bool elf_chrisalphanum(char x);
char elf_chrtouppercase(char x);
char elf_chrtolowercase(char x);



int elf_cstrlen(char const *s);
elf_bool S_eql(char const *x, char const *y, int n);
elf_bool S_eq(char const *x, char const *y);
char *S_ncopy(Alloc *allocator, int length, char const *string);
char *S_copy(Alloc *allocator, char const *string);
char *S_pfv(Alloc *cator, char const *format, va_list v);
char *S_tpfv(char const *format, va_list v);
char *S_tpf_(char const *format, ...);
elf_bool elf_cstrmatchsingle(char *p, char *s);
elf_bool elf_cstrmatch(char *p, char *s);



elf_bool elf_cstrmatch(char *p, char *s);
char *S_copy(Alloc *cator, char const *contents);
int elf_cstrlen(char const *contents);
elf_bool S_eq(char const *x, char const *y);
unsigned int S_hashcontents (char const *contents, unsigned int length);
char *S_tpf_(char const *format, ...);
#define elf_tpf(format,...) (LCHECKPRINTF(format,__VA_ARGS__),S_tpf_(format,__VA_ARGS__))

