//
// See Copyright Notice In elf.h
//

#ifndef ELF_CORE_H
#define ELF_CORE_H

#include "base.h"

#define NO_BYTE (-1)

#include "bytecode.h"

#include "source.h"

typedef struct elf_State      elf_State;
typedef struct elf_Object     elf_Object;
typedef struct elf_Value      elf_Value;
typedef struct elf_String     elf_String;
typedef struct elf_Table      elf_Table;
typedef struct elf_Array      elf_Array;
typedef struct elf_Closure    elf_Closure;
typedef struct elf_ByteBuffer elf_ByteBuffer;
typedef struct elf_UserObject elf_UserObject;
typedef struct elf_Module     elf_Module;
typedef struct BcFunction     BcFunction;
typedef struct BcFunctionRef  BcFunctionRef;
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

typedef struct
{
	elf_State   *state;
	GlobalEntry *entries;
	u32          entry_capacity;
	elf_Value   *values;
	u32          value_capacity;
	u32          count;
}
elf_Globals;

// TODO(RJ): this needs to be defined else-where!
struct BcFunction
{
	b32          variadic;
	u8              arity;
	u8           captures;
	u16        stack_size;
	u16            length;
	u32            offset;
	SourceMapEntry *source_map;
	u32             source_map_count;
};

struct elf_Module
{
	elf_Module   *next;
	elf_String   *source_name;
	char         *source_data;
	u32           source_size;
	elf_String  **strings;
	u32           string_count;
	f64          *number_constants;
	u32           number_constant_count;
	i64          *integer_constants;
	u32           integer_constant_count;
	BcFunction   *bytecode_functions;
	u32           bytecode_function_count;
	Bytecode     *bytecode;
	u32           bytecode_count;
};

struct BcFunctionRef
{
	elf_Module *module;
	u32          index;
};

struct elf_Closure
{
	elf_Object         obj;
	BcFunctionRef function;
	elf_Value     captures[];
};

struct StackFrame
{
	elf_Module      *module;
	BcFunction    *function;
	elf_Value    *framebase;
	elf_Value    *reference;
	elf_Value     *captures;
	u32           framesize;
	u32         instruction;
	u8                nargs;
	u8                nrets;
	u8                arity;
	u8             variadic;
	u8            ncaptures;
};

struct elf_State
{
	elf_Arena      arena;
	void          *user_data;

	elf_Module    *modules;

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
		elf_Globals  globals;
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

#include "value/value.h"
#include "globals.h"
#include "memory.h"
#include "gc.h"
#include "string/elf_string.h"
#include "buffer/elf_buffer.h"
#include "user/elf_user_object.h"
#include "array/array.h"
#include "table/table.h"

static inline b32 bc_function_ref_is_valid(BcFunctionRef function)
{
	return function.module && function.index < function.module->bytecode_function_count;
}

static inline BcFunction *bc_function_from_ref(BcFunctionRef function)
{
	ASSERT(bc_function_ref_is_valid(function));
	return function.module->bytecode_functions + function.index;
}

// TODO(RJ): remove this!
typedef enum
{
	RUNTIME_ERROR_NONE = 0,
	RUNTIME_ERROR_GENERIC,
	RUNTIME_ERROR_EXPECTS_CALLABLE,
	RUNTIME_ERROR_INVALID_ARGUMENT_COUNT,
	RUNTIME_ERROR_UNKNOWN_BYTECODE
}
RuntimeErrorType;

void elf_report_runtime_error(elf_State *state, RuntimeErrorType error, int instr, const char *format, ...);
void elf_print_current_runtime_source_location(elf_State *state);

#endif
