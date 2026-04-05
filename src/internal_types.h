//
// See Copyright Notice In elf.h
//



// todo:
//
// GET_SFIELD: u8 u8 u16 | u32
//	                      | u16 memo
//	                      | u16 date
//
//
//
// observe that fields are not likely to mutate.
//
// instructions are likely to operate on similar or same
// data structures.
//
//	layouts tend to be similar for related objects.
//
// e.g Looping over an array of entities, over and over again, as it is
// typical in games.
//
// indirect accessing is expensive, if likelyhood of same index
// for same key is high, attempt direct lookup.
//
// cannot verify that index is still valid, date the instruction to
// the date of the object.
//
// When object mutates fields, increment object date.
//
// Old instructions are forced to reupdate.
//
//	What if instructions too old? Say date 0. What if the object mutates
// so much that it wraps back to 0?
//
// Now old instructions think they up to date.
//
// Black list the object, if object mutates too much, probably not worth
// tracking either, too volatile.
//
//	Simply prevent wrap around, if object date == MAX(date), it means black listed,
// can no longer trust.
//
//
//	Black listing is unlikely to happen, either way.
//
//	This means, date can be 8 bits instead, or even 4.
//
// Can use the rest for other information.
//
//
// "hot" objects?
//
//
//
//	Objects are central, easy to track.
//
//
//	Frequently accessed object live longer. Adapt instructions to these
// objects.
//
//
// Field mutation is rare, deletions are rare.
//
//
//	Indirect accesses are expensive, bad for cache, replace with direct access.
//
//
//
// Increment counter for read / writes if > threshold, object is hot...
// The hotter the object, the more priority...
//
//
// Adequate instruction accesses hot object, instruction tracks it,
// cache index in instruction.
//
//	Next time, if instruction up to date, direct access.
//
//	Object mutates, increment mutatecounter, any key could have changed,
// we don't know, not worth knowing.
//
//	Instruction clears cache, re-fetch.
//
//
//
//
//
//
// todo:
// experiment with storing strings entirely
// in values and see if we get some performance benefits!
//
//	todo:
//	do the same with closures!
//
//






#if !defined(ELF_MAX_FILE_PATH)
#define ELF_MAX_FILE_PATH 256
#endif






#define OVERLOAD_ADD "__add"
#define OVERLOAD_SUB "__sub"



#include "internal_shorternames.h"
#include "hash.c"


enum { true = 1, false = 0 };


typedef int BCPos;
#define NO_BYTE (-1)


#include "bytecode.h"


typedef char *Source;



// todo:
extern const char *tag2s[];
extern const char *Static_StrFromBytecode[];

// Todo, how do va-args functions work again?
// if a function is variadic it just means it won't overwrite
// the additional arguments, so add '...' to your functions if
// you care about those!
// and storage for other flags
typedef struct BytecodeFunction BytecodeFunction;
struct BytecodeFunction
{
	u8    variadic;
	u8       arity;
	u8   ncaptures;
	u16  stacksize;
	u16   numbytes;
	u32      bytes;
};

// Todo, just keep name as an atom, in case of error, attempt to load the file or lookup in a cache or something ...
typedef struct BytecodeFile BytecodeFile;
struct BytecodeFile
{
	u32               bytepos;
	u32               byteend;
	BytecodeFunction  main;
	// Todo, why do we store this?
	u32               size;
	char             *data;
	// Todo, use proper atoms!
	char              name[ELF_MAX_FILE_PATH];
};









typedef enum {
	TBIT_NIL       = 1 << ELF_TNIL,
	TBIT_TOMB      = 1 << ELF_TTOMB,
	TBIT_NUMBER    = 1 << ELF_TNUMBER,
	TBIT_INTEGER   = 1 << ELF_TINTEGER,
	TBIT_HANDLE    = 1 << ELF_THANDLE,
	TBIT_FUNCTION  = 1 << ELF_TFUNCTION,
	TBIT_USER      = 1 << ELF_TUSER,
	TBIT_CLOSURE   = 1 << ELF_TCLOSURE,
	TBIT_STRING    = 1 << ELF_TSTRING,
	TBIT_TABLE     = 1 << ELF_TTABLE,
	TBIT_BUFFER    = 1 << ELF_TBUFFER,
	//
	TBIT_ALLMASK   = (1 << ELF_TCOUNT_) - 1,
} TypeBit;

typedef enum {
	TRULE_NONE     = 0,
	TRULE_NIL      = TBIT_NIL,
	TRULE_TOMB     = TBIT_TOMB,
	TRULE_NUMBER   = TBIT_NUMBER,
	TRULE_INTEGER  = TBIT_INTEGER,
	TRULE_HANDLE   = TBIT_HANDLE,
	TRULE_FUNCTION = TBIT_FUNCTION,
	TRULE_USER     = TBIT_USER,
	TRULE_CLOSURE  = TBIT_CLOSURE,
	TRULE_STRING   = TBIT_STRING,
	TRULE_TABLE    = TBIT_TABLE,
	TRULE_BUFFER   = TBIT_BUFFER,
	//
	TYPE_RULE_ANYTHING  = TBIT_ALLMASK,
	TRULE_NONNIL   = TYPE_RULE_ANYTHING & ~TBIT_NIL,

	TRULE_OBJECT   = TBIT_USER|TBIT_CLOSURE|TBIT_STRING|TBIT_TABLE|TBIT_BUFFER,

	TRULE_NUMERIC  = TBIT_INTEGER|TBIT_NUMBER,
	TRULE_CALLABLE = TBIT_FUNCTION|TBIT_CLOSURE,

	TRULE_COUNT,
} TypeRule;


#define NIL_VALUE ((Value) { ELF_TNIL })

#define VALUE_READONLY 2

struct elf_Value {
	u8   tag;
	u8   status;
	u16  unused;
	union {
		i64         x_i64;
		struct {i32 x_i32, y_i32;};
		i64         x_int;
		f64         x_num;
		Sys         x_sys;
		GCRef         x_obj;
		Tab         x_tab;
		GCStr         x_str;
		Fun         x_proc;
		Closure     x_closure;
		Buf         x_buf;
		void       *x_ptr;
	};
};
STATIC_ASSERT(sizeof(Value) == 16);









typedef Index Rank;


typedef struct {
	// 8  bytes
	Rank   rank;

	// 16 bytes
	Value value;
} RankValue;
STATIC_ASSERT(sizeof(RankValue) == 24);






// todo: we should try to make this much smaller, limit the key size to 64 bits
// and the index to 32
typedef struct {
	union {
		Index idx;	 // todo: remove!
		Index index;
	};
	union {
		Value key;
		Value field;
	};
} IndexValue, Entry;
STATIC_ASSERT(sizeof(IndexValue) == 24);

STATIC_ASSERT(offsetof(RankValue, value) == offsetof(IndexValue,   key));
STATIC_ASSERT(offsetof(RankValue,  rank) == offsetof(IndexValue, index));


#define GC_TAG_REACHABLE      1
#define NODE_READONLY       2
#define NODE_NOCHILDREN     4
#define NODE_DEBUGTRAP      8

struct GCNode
{
	u8          type;
	u8        status;
	u16         size;
	Table      *meta;
};

static int markreadonly(elf_State *S, GCRef ref);

struct elf_Closure
{
	GCNode                obj;
	BytecodeFunction function;
	Value            captures[];
};

struct elf_Table {
	GCNode      obj;
	union {
		Index        ntotal;
		Index        nentries;
	};
	union {
		Index        nslots;       // todo: remove
		Index        fillcounter;
	};
	Index    	    ndebug;
	union {
		IndexValue       *slots; // todo: remove
		IndexValue     *entries;
	};
	union {
		Value        *array;
		Value       *values;
	};
};


void _table_freeinternalmemory(Tab tab);
Tab newtable2(elf_State *, Index nentries);
Tab new_table(elf_State *);

Index tableset(elf_State *S, Tab tab, V k, V v);

Value _table_getornil(elf_State *S, Tab tab, Value key);
Index elf_table_ensure(elf_State *S, Tab tab, V key);
Index _table_arrayadd(elf_State *S, Tab tab, Value value);
Value _table_arrayget(elf_State *S, Tab tab, Index index);
Index _table_arraylen(Tab tab);


void tableunparse(elf_State *S, Stringer *sb, Tab tab, int level);



struct elf_Buffer {
	GCNode  obj;
	u8      enc;
	Index   max;
	Index   min;
	char   *mem;
};



//
// Structure for regular strings
//
// | u32 | u32 | u32 u32
// |     |     |
// |     |     | string pointer
// |     |
// |     | hash
// |
// | u16 length
//
//
// A good majority of strings are 16 bytes or less, right?
//
// some_space_and_0
//
// like this is a 15 char string, including null term.
//
//
// | u32 u32 u32 u32
// |
// |
// | data
//
//
//	No need to store hash nor length, determining length becomes
// trivial.
//
//	Max length is known, can read safely 4 byte aligned.
//
//
//	The hash is the string itself, would need to figure out
// how to compare against other strings.
//
//
//	Normally, you compare 4 bytes for length, 4 bytes for hash,
// and then arbitrarily long text.
//
//
// Why not just compare entire block in one go?
//
//
//
// Would to need to ensure regular strings are least 16 bytes,
// they are aligned already, so this is free..
//
//
//
struct elf_String {
	GCNode          obj;
	Hash           hash;
	// todo: remove this! we already know this from
	// the size (obj->size - sizeof(*String) - 1)
	i32 	       length;

	// todo: ensure 16 byte aligned
	union
	{
	char        text[1];
	char        data[1];
	};
};


GCStr new_empty_string(elf_State *, u32 size);
GCStr new_string_from_data_size(elf_State *, const char *data, int size);
GCStr new_string_from_data(elf_State *, const char *data);
int         strl(GCStr str);
const char *strt(GCStr str);
Hash        strh(GCStr str);








//
// todo: store closure frames entirely on the stack, values are large,
// so we can pack lots of frame info there.
//
//
typedef struct Stack_Frame Stack_Frame;
struct Stack_Frame {

	// the start of this frame on the stack right after the function
	V            *framebase;

	u8                nargs;
	u8                nrets;


	// the rest is only for interpreter frames


	// todo: you could get these from the closure at framebase-1
	u8                arity;
	u8             variadic;


	// todo: could be stored along with the closure value
	// the starting byte and the byte count
	int               bytes;
	int               bytec;


	//
	//
	//	Closure value
	//
	//	first 64 bits are prototype information, the next 64 are a pointer
	// into the closure where you can access its captures and other information
	// that isn't as hot.
	// Values are much larger than this so we could pack everything in a closure
	// but only do if performace actually goes up!
	//
	//         | u16 byte count
	//         | u8  number of captures
	//         | u8  arity and whether it is variadic
	//         |
	//         |
	// | u32   | u32 | u32 u32
	// |             |
	//	|             | closure pointer
	//	|
	//	|	u32 byte start
	//
	//
	//
	//
	//	Use special tag to not trip the interpreter!
	//	call info value
	//
	//
	//	| u32              | u32           | u32   | u32
	//	|                  |               |       | nextinstr
	// | caller frame     |               |
	//                    |               | u8 vaoffset
	//                    |               | u8  frame size
	//							 | return instr  |
	//



	// todo: frame size could be a u8!
	int           framesize;


	// todo: nextinstr could be a u16
	int           nextinstr;


	// to handle v-args, could be a u8, an additional offset
	// on top of framebase
	V            *reference;


	// todo: could get this from the closure at framebase-1
	// since values are large, we can store that along with
	// the closure to avoid having to fetch the closure pointer
	V           *closureenv;
	u8          closuresize;
};









typedef enum
{
	GC_OBJECT = 0,
	GC_CLOSURE,
	GC_STRING,
	GC_TABLE,
	GC_BUFFER,
}
GCType;

#define GC_MEM_THRESHOLD_MIN ((elf_i64) MEGABYTES(1))
#define GC_MEM_THRESHOLD_MAX ((elf_i64) GIGABYTES(1))
#define GC_OBJ_THRESHOLD_MIN ((elf_i64) ((512) * 1))
#define GC_OBJ_THRESHOLD_MAX ((elf_i64) ((512) * 1))

typedef struct GCState GCState;
struct GCState
{
	u32         state;
	u32         memory_counter;
	u32         memory_thresh;
	u32         counter;
	u32         counter_thresh;
	GCRef      *references1;
	GCRef      *references2;
};

void *collector_alloc(elf_State *state, GCType type, u32 size);

// Todo, remove this? ...
typedef struct Record Record;
struct Record {
	Bytecode b;
	V o, x, y;
};

// Todo, move to arena.h
typedef struct
{
	u64 size, used;
	u8 *data;
}
elf_Arena;


typedef struct elf_State elf_State;
struct elf_State
{
	// Todo,
	elf_Arena    arena_;
	elf_Arena    scratch_arena_;
	elf_Arena   *arena;
	elf_Arena   *scratch_arena;

	struct
	{
		Tab      globals;
		// minimize usage of these!
		Num     *numbers;
		Int     *integers;
	};

	struct
	{
		BytecodeFunction   *protos;

		// todo: compress this
		// todo: also, allocate per BytecodeFile?
		char              **lines;


		// todo: why is this a double pointer?
		BytecodeFile      **files;



		// todo: I don't know how long we'll manage with just the one
		// buffer, especially with people loading stuff at runtime,
		// I think we'll do our own memory management here, and once
		// we run out of space we can either reallocate to get more, or
		// if too fragmented do a copy and compact
		Bytecode        *bytebuf;
		int              bytecur;
	};


	GCState      collector_state;

	struct {
		V           *stack;
		V           *stack_ptr;
		int          stack_max;

		u32          record_min;
		u32          record_max;
		Record      *record;

		int          frame_index;
		int          frame_stack_max;
		Stack_Frame *frame_stack;
		Stack_Frame  frame;

		BCPos        byte;
	};

	struct {
		Tab integer;
		Tab number;
		Tab string;
		Tab table;
		Tab buffer;
	} metatables;
};

void _initstate(elf_State *);
void reporterror(elf_State *, int instr, const char *error);
void reporterrorf(elf_State *, int instr, const char *format, ...);

