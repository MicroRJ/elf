//
// See Copyright Notice In elf.h
//

#include "base.h"

#define NO_BYTE (-1)

#include "bytecode.h"

typedef char *Source;

typedef struct
{
	char *data;
	u32   size;
	char *line_start;
	u32   line_index;
}
SourceSite;

typedef struct
{
	u32        byte_start;
	u32        byte_end;
	SourceSite site;
}
SourceMapEntry;

typedef struct elf_Object  elf_Object;
typedef struct elf_String  elf_String;
typedef struct elf_Table   elf_Table;
typedef struct elf_Closure elf_Closure;
typedef struct elf_Value   elf_Value;

#include "value/value.h"

#include "gc.h"
#include "atom/atom.h"
#include "table/table.h"

typedef struct BcFunction BcFunction;
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
	char           *source_data;
	u32             source_size;
	elf_String       *source_name;
};

struct elf_Closure
{
	elf_Object              obj;
	BcFunction   function;
	elf_Value        captures[];
};

typedef struct
{
	char         *name;
	elf_Function function;
}
elf_Binding;


typedef struct StackFrame StackFrame;
struct StackFrame
{
	elf_Value  *framebase;
	u8              nargs;
	u8              nrets;
	u8              arity;
	u8           variadic;
	int             bytes;
	int             bytec;
	int         framesize;
	int         nextinstr;
	elf_Value  *reference;
	elf_Value *closureenv;
	u8        closuresize;
};

typedef struct elf_State elf_State;
struct elf_State
{
	elf_Arena    arena;
	void        *user_data;

	struct
	{
		f64          *number_constants;
		u32           number_constant_count;
		u32           number_constant_capacity;
		i64          *integer_constants;
		u32           integer_constant_count;
		u32           integer_constant_capacity;
		BcFunction   *bytecode_functions;
		u32           bytecode_function_count;
		u32           bytecode_function_capacity;
		Bytecode     *bytecode;
		u32           bytecode_count;
		u32           bytecode_capacity;
	};
	struct
	{
		u32          gc_mode;
		u32          gc_live_bytes;
		u32          gc_next_cycle_bytes;
		u32          gc_reference_count;
		u32          gc_reference_capacity;
		elf_Object **gc_references;
		elf_Object **gc_scratch_references;
	};
	struct
	{
		elf_Table   *globals;
		elf_Table   *ref_table;
		u32          next_ref;

		u32          stack_size;
		elf_Value   *stack_ptr;
		elf_Value   *stack;

		u64          frame_stack_size;
		StackFrame  *frame_stack;
		u64          frame_index;
		StackFrame   frame;
		u64          byte;
	};
	struct
	{
		u32          atom_count;
		u32          atom_bucket_count;
		elf_String **atom_buckets;
	};
	struct
	{
		elf_Table *integer;
		elf_Table *number;
		elf_Table *atom;
		elf_Table *table;
	}
	metatables;
};

/* Internal constructors used by the runtime and its tests. */
elf_Table *elf_push_new_table(elf_State *state);

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
