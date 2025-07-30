//
// See Copyright Notice In elf.h
// elf.c
//

// todo: remove
typedef signed   int  			 bool;

typedef signed   int           elf_Bool;
typedef signed   char          elf_i8;
typedef unsigned char          elf_u8;
typedef signed   short         elf_i16;
typedef unsigned short         elf_u16;
typedef signed   int           elf_i32;
typedef signed   int           elf_b32;
typedef unsigned int           elf_u32;
typedef   signed long long int elf_i64;
typedef unsigned long long int elf_u64;
typedef double 			       elf_f64;
typedef float  			       elf_f32;

typedef elf_i64  elf_Int;
typedef elf_f64  elf_Num;
typedef void    *elf_Handle;
typedef elf_i32  elf_Error;

// todo: internal
enum { true = 1, false = 0 };
