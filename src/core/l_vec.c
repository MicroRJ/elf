//
// See Copyright Notice In elf.h
//
// In the beginning, the original thought was to make
// elf multi-scalar, so all values are N-component
// vectors, but I never went through with the idea
// because it just never seemed as if it would actually
// pay off in any regard...
//
// I write this comment here because every now and then
// I revisit this idea, sort of like opening the fridge
// door once again hoping something magically appears,
// so this is my way of reminding myself of why this
// wouldn't really make much sense.
//
//	Theoretically this would have led to much
// better throughput on certain scenarios but
// in practice, given the sort of very general
// purpose code one typically writes on a very
// general purpose language, it didn't seem worth
// the additional complexity and semantic
// implications.
// In the end, when you care about performance,
// you would typically write a piece of code that
// is very specific and has very high performance
// characteristics as a whole, when you consider
// the input and the output, we can never do this
// job, no compiler can, with all the semantic
// information in the world, let alone some dynamic
// language with little to no type information.
// The best hope would be to JIT, and vectors would
// just add additional complexity there too.
// My point, if you want performance, do not use this,
// use something else.
// However, what is a good idea is to have a simple
// way to call C functions from elf, to have high
// level operations done directly on the CPU with
// very good throughput.
// For instance, one could develop a math library
// for moving massive amounts of data, and then elf
// could do the high level stuff.
// So I think is best that if the goal is to make a
// very high-level language, to leave concrete and
// well defined processes for specialized routines.
//
// That being said, I still want vector types because
// I use them all the time for games.
//

#if 0
typedef struct { float x, y; } elf_vec2;

static inline elf_vec2 _get_vec2(elf_Table *tab, elf_Value sx, elf_Value sy) {
	elf_Value x = elf_table_get_raw(tab, sx);
	elf_Value y = elf_table_get_raw(tab, sy);
	return (elf_vec2){ VI2N(x), VI2N(y) };
}

#define OP_MUL(x, y) ((x) * (y))
#define OP_DIV(x, y) ((x) / (y))
#define OP_ADD(x, y) ((x) + (y))
#define OP_SUB(x, y) ((x) - (y))
#define OP_POW(x, y) (pow(x, y))

#define VEC2OPDEF(NAME, OP) \
ELF_FUNCTION(lib_vec2__##NAME) { \
	elf_Value sx = VALUE_STRING(elf_new_string(S, "x")); \
	elf_Value sy = VALUE_STRING(elf_new_string(S, "y")); \
	elf_vec2 x = _get_vec2((elf_Table *) elf_get_this(S), sx, sy); \
	elf_vec2 y = _get_vec2(elf_gettabraw(S, 0), sx, sy); \
	elf_vec2 v = { OP(x.x, y.x), OP(x.y, y.y) }; \
	elf_Table *res = elf_new_table(S); \
	elf_raw_table_set(res, sx, VALUE_NUMBER(v.x)); \
	elf_raw_table_set(res, sy, VALUE_NUMBER(v.y)); \
	elf_push_table_raw(S, res); \
	return 1; \
} \
/* end */

VEC2OPDEF(mul, OP_MUL)
VEC2OPDEF(div, OP_DIV)
VEC2OPDEF(add, OP_ADD)
VEC2OPDEF(sub, OP_SUB)
VEC2OPDEF(pow, OP_POW)


elf_Binding metatable_lib_vec2[] = {
	{"mul", lib_vec2__mul},
	{"div", lib_vec2__div},
	{"add", lib_vec2__add},
	{"sub", lib_vec2__sub},
	{"pow", lib_vec2__pow},
};
#endif