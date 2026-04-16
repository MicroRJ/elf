//
// See Copyright Notice In elf.h
//
#ifndef SHORTER_NAMES
#define SHORTER_NAMES



typedef struct elf_Buffer  elf_Buffer;
typedef struct GCNode  GCNode;
typedef struct elf_Table   elf_Table;
typedef struct elf_String  elf_String;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;



typedef struct elf_Buffer  *Buf;



typedef elf_Value   V;
typedef elf_Value   Value;
typedef elf_Value   Val;


typedef elf_Value   *Array;



typedef elf_Handle  Handle;
typedef elf_Handle  Sys;


typedef elf_Time    Time;
typedef elf_Index   Index;
typedef elf_Hash    Hash;


typedef elf_Function Fun;


typedef elf_Integer Int;
typedef elf_Number  Num;


typedef GCNode *GCRef;


typedef elf_Table    Table;
typedef elf_Table   *Tab;

typedef elf_String  *GCStr;
typedef elf_Closure *Closure;


typedef ELF_ValueType     Tag;


typedef elf_u8  u8;
typedef elf_u16 u16;
typedef elf_u32 u32;
typedef elf_u64 u64;

typedef elf_i8  i8;
typedef elf_i16 i16;
typedef elf_i32 i32;
typedef elf_i64 i64;
typedef elf_f64 f64;
typedef elf_i32 b32;

#endif