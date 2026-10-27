//
// See Copyright Notice In elf.h
//

#ifndef ELF_RUNTIME_H
#define ELF_RUNTIME_H

#include "base.h"
#include "error.h"

#include "bytecode.h"

typedef struct elf_State      elf_State;
typedef struct elf_Object     elf_Object;
typedef struct elf_Value      elf_Value;
typedef struct elf_String     elf_String;
typedef struct elf_Table      elf_Table;
typedef struct elf_Array      elf_Array;
typedef struct elf_Closure    elf_Closure;
typedef struct elf_GCError    elf_GCError;
typedef struct elf_ByteBuffer elf_ByteBuffer;
typedef struct elf_UserObject elf_UserObject;
typedef struct elf_Prototype   elf_Prototype;
typedef struct elf_Source      elf_Source;
typedef struct StackFrame     StackFrame;
typedef struct elf_SymField   elf_SymField;

typedef enum
{
	ELF_GC_ACTIVE = 0,
	ELF_GC_PAUSED,
}
elf_GCMode;

typedef enum
{
	ELF_OBJECT_USER = 0,
	ELF_OBJECT_CLOSURE,
	ELF_OBJECT_TABLE,
	ELF_OBJECT_STRING,
	ELF_OBJECT_ERROR,
	ELF_OBJECT_BUFFER,
	ELF_OBJECT_ARRAY,
	ELF_OBJECT_COUNT,
}
elf_ObjectType;

struct elf_Object
{
	u8  type;
	u8  status;
	u16 unused;
	u32 size;
};

struct elf_Value
{
	u8  type;
	u8  status;
	u16 unused;
	union {
		i64             x_i64;
		i64             x_int;
		f64             x_num;
		elf_Object     *x_obj;
		elf_Table      *x_tab;
		elf_Array      *x_array;
		elf_String     *x_string;
		elf_GCError    *x_error;
		elf_ByteBuffer *x_buffer;
		elf_UserObject *x_user;
		elf_Function    x_proc;
		elf_Function    x_function;
		elf_Closure    *x_closure;
		elf_Prototype  *x_proto;
		void           *x_ptr;
	};
};
STATIC_ASSERT(sizeof(elf_Value) == 16);

struct elf_String
{
	elf_Object  obj;
	elf_String *next;
	u32         hash;
	char        data[];
};

#define ELF_STRING_HEADER_SIZE ((u32)offsetof(elf_String, data))
#define ELF_STRING_MAX_SIZE    (UINT32_MAX - ELF_STRING_HEADER_SIZE - 1)

struct elf_ByteBuffer
{
	elf_Object obj;
	u8        *data;
	u32        size;
	u32        capacity;
};

struct elf_UserObject
{
	elf_Object         obj;
	const elf_UserType *type;
	elf_Table          *metatable;
	max_align_t         data[];
};

struct elf_Array
{
	elf_Object obj;
	u32        capacity;
	u32        count;
	elf_Value *items;
};

#define ENTRY_TOMB_TYPE 0xFFu

typedef struct
{
	elf_Value key;
	elf_Value value;
}
Entry;
STATIC_ASSERT(sizeof(Entry) == 32);

struct elf_Table
{
	elf_Object obj;
	Entry     *entries;
	u32        capacity;
	u32        count;
	u32        occupied;
};

typedef enum
{
	ELF_SYMBOL_INTEGER = 0,
	ELF_SYMBOL_NUMBER,
	ELF_SYMBOL_STRING,
	ELF_SYMBOL_TABLE,
	ELF_SYMBOL_FUNCTION,
}
elf_SymbolType;

struct elf_SymField
{
	elf_SymField *next;
	dy_String     name;
	u8            type;
	union
	{
		i64           integer;
		f64           number;
		dy_String     string;
		elf_SymTable *table;
		elf_Function  function;
	};
};

struct elf_SymTable
{
	elf_SymTable *parent;
	elf_SymField *first_field;
	elf_SymField *last_field;
	u32           field_count;
};

// TODO(RJ): we could compress this WAY down, encode the line index & column as a u16,
// store base line index in the prototype as a u32, byte start and byte end could be
// replaced with a single u16 end marker ...
typedef struct
{
	u32 byte_start;
	u32 byte_end;
	u32 source_offset;
	u32 source_size;
	u32 line_offset;
	u32 line_index;
}
elf_SourceMapEntry;

struct elf_Prototype
{
	u64                 allocation_size;
	elf_Source         *source;
	b32                 variadic;
	u8                  arity;
	u8                  captures;
	// TODO(RJ): rename this to nregs
	u16                 stack_size;

	elf_String        **strings;
	u32                 string_count;
	f64                *num_data;
	u32                 number_constant_count;
	i64                *int_data;
	u32                 integer_constant_count;
	Bytecode           *bytecode;
	u32                 bytecode_count;
	elf_SourceMapEntry *source_map;
	u32                 source_map_count;
};

typedef struct
{
	elf_Prototype *prototype;
	u32            instruction;
}
elf_ErrorFrame;

struct elf_GCError
{
	elf_Object     obj;
	elf_String    *message;
	elf_Table     *data;
	u32            frame_count;
	elf_ErrorFrame frames[];
};

struct elf_Source
{
	elf_Source *next;
	u64         allocation_size;
	dy_String   name;
	dy_String   text;
};

struct elf_Closure
{
	elf_Object     obj;
	elf_Prototype *prototype;
	elf_Value      captures[];
};

// Bytecode stack frame layouts:
//
//   ordinary call or variadic call without extra args
//
//     [callee][args / bytecode regs...]
//              ^ args
//              ^ regs
//
//   variadic call with extra args
//
//     [callee][original args...][bytecode regs...]
//              ^ args             ^ regs
//
// args preserves the original call args for elf.arg() and
// elf.varg(). Bytecode operands index the register window through regs.
// Named parameters are copied into that window when the two bases differ.
struct StackFrame
{
	elf_Value          *base;
	elf_Value          *regs;
	Bytecode          *instr;
	u16                 padd;
	u16                 size;
	u8                 nargs;
	u8                 ndest;
};

struct elf_State
{
	Arena          arena;
	void          *user_data;

	elf_Prototype **prototypes;
	u32             prototype_count;
	u32             prototype_capacity;
	elf_Function   *native_functions;
	u32             native_function_count;
	u32             native_function_capacity;
	elf_SymTable   *symbols;
	elf_Source     *sources;

	struct
	{
		u32          gc_mode;
		u64          gc_live_bytes;
		u64          gc_peak_live_bytes;
		u64          gc_next_cycle_bytes;
		u32          gc_reference_count;
		u32          gc_reference_capacity;
		elf_Object **gc_references;
		elf_Object **gc_scratch_references;
		u64          gc_allocated_objects;
		u64          gc_allocated_bytes;
		u64          gc_allocated_by_type[ELF_OBJECT_COUNT];
		u64          gc_freed_objects;
		u64          gc_freed_bytes;
		u64          gc_collection_count;
		u64          gc_collection_time_ns;
		u64          gc_last_collection_time_ns;
		u64          gc_max_collection_time_ns;
	};
	struct
	{
		elf_Table   *ref_table;
		u32          next_ref;

		u32          stack_size;
		elf_Value   *stack_top;
		elf_Value   *stack;

		u64          frame_stack_size;
		StackFrame  *frame_stack;
		StackFrame  *frame;
	};
	struct
	{
		u32          string_count;
		u32          string_bucket_count;
		elf_String **string_buckets;
	};

	// TODO(RJ): should we just allow the user to add metatables to their things?
	struct
	{
		elf_Table *integer;
		elf_Table *number;
		elf_Table *string;
		elf_Table *table;
		elf_Table *array;
		elf_Table *buffer;
		elf_Table *error;
		elf_Table *fs_file;
	}
	metatables;
};

#define ELF_OBJECT_REACHABLE 1
#define ELF_OBJECT_READONLY  2
#define ELF_GC_NEXT_MIN ((u32)MEGABYTES(1))
#define ELF_GC_NEXT_MAX ((u32)GIGABYTES(1))

elf_State *elf_runtime_create_state(void);

void *elf_alloc(elf_State *state, elf_u64 size);
void *elf_alloc_zero(elf_State *state, elf_u64 size);
void *elf_realloc(elf_State *state, void *memory, elf_u64 old_size, elf_u64 new_size);
void elf_free(elf_State *state, void *memory, elf_u64 size);

ErrorCode elf_native_function_ensure(elf_State *state, elf_Function function, u32 *index);
void elf_native_functions_destroy(elf_State *state);

elf_Source *elf_source_create(elf_State *state, dy_String name, dy_String text);
void elf_sources_destroy(elf_State *state);

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size);

elf_Closure *elf_closure_new(elf_State *state, elf_Prototype *prototype, elf_Value *captures, u32 capture_count);
elf_GCError *elf_error_new(elf_State *state, elf_String *message, elf_Table *data);

elf_String *elf_string_from_data_size(elf_State *state, const char *data, u32 size);

elf_ByteBuffer *elf_buffer_new_object(elf_State *state, u32 capacity);
b32 elf_buffer_require_writable(elf_State *state, elf_ByteBuffer *buffer);
b32 elf_buffer_reserve_object(elf_State *state, elf_ByteBuffer *buffer, u32 capacity);
b32 elf_buffer_resize_object(elf_State *state, elf_ByteBuffer *buffer, u32 size);
b32 elf_buffer_append_object(elf_State *state, elf_ByteBuffer *buffer, const void *data, u32 size);
b32 elf_buffer_splice_object(elf_State *state, elf_ByteBuffer *buffer, u32 position, u32 delete_size, const void *data, u32 insert_size);

elf_UserObject *elf_user_object_new(elf_State *state, const elf_UserType *type, elf_Table *metatable, u32 size);

elf_Array *elf_array_new(elf_State *state);
void elf_array_check_write(elf_State *state, elf_Array *array);
void elf_array_reserve(elf_State *state, elf_Array *array, u32 min_capacity);
elf_Value *elf_array_slot(elf_State *state, elf_Array *array, u32 index);
u32 elf_array_add(elf_State *state, elf_Array *array, elf_Value value);
elf_Value elf_array_get(elf_State *state, elf_Array *array, u32 index);
void elf_array_set(elf_State *state, elf_Array *array, u32 index, elf_Value value);
u32 elf_array_length(elf_Array *array);

elf_Table *elf_table_new(elf_State *state);
void elf_table_set(elf_State *state, elf_Table *table, elf_Value key, elf_Value value);
elf_Value elf_table_get_or_nil(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_contains(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_delete(elf_State *state, elf_Table *table, elf_Value key, elf_Value *removed);
void elf_table_clear(elf_State *state, elf_Table *table);

// README: STOP RELYING ON THIS TO ABORT THE PROGRAM
void report_runtime_error(elf_State *state, ErrorCode code, const char *format, ...);

// README: THIS IS ONLY FOR TESTING
void print_current_runtime_source_location(elf_State *state);

#include "elf_runtime_helpers.h"

#endif
