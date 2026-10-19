//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_H
#define ELF_CORE_H

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
typedef struct elf_ByteBuffer elf_ByteBuffer;
typedef struct elf_UserObject elf_UserObject;
typedef struct elf_Prototype   elf_Prototype;
typedef struct elf_Source      elf_Source;
typedef struct StackFrame     StackFrame;
typedef struct GlobalEntry    GlobalEntry;

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
		elf_ByteBuffer *x_buffer;
		elf_UserObject *x_user;
		elf_Function    x_proc;
		elf_Closure    *x_closure;
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

struct elf_Prototype
{
	u64               allocation_size;
	elf_Source       *source;
	b32               variadic;
	u8                arity;
	u8                captures;
	u16               stack_size;
	elf_String      **strings;
	u32               string_count;
	f64              *number_constants;
	u32               number_constant_count;
	i64              *integer_constants;
	u32               integer_constant_count;
	Bytecode         *bytecode;
	u32               bytecode_count;
	BcSourceMapEntry *source_map;
	u32               source_map_count;
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

struct StackFrame
{
	elf_Prototype *prototype;
	elf_Closure     *closure;
	elf_Value     *framebase;
	elf_Value     *reference;
	elf_Value      *captures;
	u32          instruction;
	u32            framesize;
	u8                 nargs;
	u8                 nrets;
	u8                 arity;
	u8              variadic;
	u8             ncaptures;
};

struct elf_State
{
	Arena          arena;
	void          *user_data;

	elf_Prototype **prototypes;
	u32             prototype_count;
	u32             prototype_capacity;
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
		GlobalEntry *global_entries;
		u32          global_entry_capacity;
		elf_Value   *global_values;
		u32          global_value_capacity;
		u32          global_count;
		elf_Table   *ref_table;
		u32          next_ref;
		u64          vm_closure_calls;
		u64          vm_native_calls;
		u64          vm_instructions;
		u64          vm_max_stack_slots;
		u64          vm_max_call_depth;

		u32          stack_size;
		elf_Value   *stack_ptr;
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
	}
	metatables;
};

#define ELF_OBJECT_REACHABLE 1
#define ELF_OBJECT_READONLY  2
#define ELF_GC_NEXT_MIN ((u32)MEGABYTES(1))
#define ELF_GC_NEXT_MAX ((u32)GIGABYTES(1))

void *elf_alloc(elf_State *state, elf_u64 size);
void *elf_alloc_zero(elf_State *state, elf_u64 size);
void *elf_realloc(elf_State *state, void *memory, elf_u64 old_size, elf_u64 new_size);
void elf_free(elf_State *state, void *memory, elf_u64 size);

elf_Source *elf_source_create(elf_State *state, dy_String name, dy_String text);
void elf_sources_destroy(elf_State *state);

void *elf_gc_alloc(elf_State *state, elf_ObjectType type, u32 size);

elf_String *elf_string_from_data_size(elf_State *state, const char *data, u32 size);

elf_ByteBuffer *elf_buffer_new_object(elf_State *state, u32 capacity);
b32 elf_buffer_reserve_object(elf_State *state, elf_ByteBuffer *buffer, u32 capacity);
b32 elf_buffer_resize_object(elf_State *state, elf_ByteBuffer *buffer, u32 size);
b32 elf_buffer_append_object(elf_State *state, elf_ByteBuffer *buffer, const void *data, u32 size);
b32 elf_buffer_splice_object(elf_State *state, elf_ByteBuffer *buffer, u32 position, u32 delete_size, const void *data, u32 insert_size);

elf_UserObject *elf_user_object_new(elf_State *state, const elf_UserType *type, elf_Table *metatable, u32 size);

elf_Array *elf_array_new(elf_State *state);
u32 elf_array_add(elf_State *state, elf_Array *array, elf_Value value);
u32 elf_array_insert(elf_State *state, elf_Array *array, u32 index, elf_Value value);
elf_Value elf_array_get(elf_State *state, elf_Array *array, u32 index);
void elf_array_set(elf_State *state, elf_Array *array, u32 index, elf_Value value);
void elf_array_remove(elf_State *state, elf_Array *array, u32 index, u32 count);
void elf_array_splice(elf_State *state, elf_Array *array, u32 index, u32 delete_count, elf_Array *replacement);
void elf_array_swap(elf_State *state, elf_Array *array, u32 left, u32 right);
u32 elf_array_length(elf_Array *array);

elf_Table *elf_table_new(elf_State *state);
void elf_table_set(elf_State *state, elf_Table *table, elf_Value key, elf_Value value);
elf_Value elf_table_get_or_nil(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_contains(elf_State *state, elf_Table *table, elf_Value key);
b32 elf_table_delete(elf_State *state, elf_Table *table, elf_Value key, elf_Value *removed);
void elf_table_clear(elf_State *state, elf_Table *table);

void elf_globals_destroy(elf_State *state);
b32 elf_globals_find(elf_State *state, elf_StrSlice name, u32 *slot);
u32 elf_globals_ensure(elf_State *state, elf_StrSlice name);
elf_Value elf_globals_get_by_name(elf_State *state, elf_StrSlice name);
void elf_globals_set_by_name(elf_State *state, elf_StrSlice name, elf_Value value);
elf_Value elf_globals_get_by_slot(elf_State *state, u32 slot);
void elf_globals_set_by_slot(elf_State *state, u32 slot, elf_Value value);

// README: STOP RELYING ON THIS TO ABORT THE PROGRAM
void report_runtime_error(elf_State *state, ErrorCode code, const char *format, ...);

// README: THIS IS ONLY FOR TESTING
void print_current_runtime_source_location(elf_State *state);

#include "elf_helpers.h"

#endif
