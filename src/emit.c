
#define NO_JUMP (-0)

typedef i32 GenMemorySlot;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static GenMemorySlot allocate_slots(BytecodeGen *, u32 num);
static int allocate_slot(BytecodeGen *);
static i32 get_expr_mem(BytecodeGen *, AstRef expr);
static int expr_to_any_mem(BytecodeGen *, AstRef expr);
static GenMemorySlot generate_ast_expr(BytecodeGen *gen, AstRef expr, GenMemorySlot slots, u32 nslots);
static int get_mem_state(BytecodeGen *);
static void push_mem_state_(BytecodeGen *);
static void pop_mem_state_(BytecodeGen *);

#define MEMORY_STATE_SCOPE(par) for(i32 i_ = (push_mem_state_(par), 0); i_ ++ < 1; pop_mem_state_(par))
#define ENTITY_SCOPE(par) for(i32 i_ = (begin_scope(par), 0); i_ ++ < 1; close_scope(par))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BCPos emit_bytecode(BytecodeGen *, Source site, Bytecode byte);

static int emit_branch_if_false(BytecodeGen *, jumpS *js, AstRef id);
static int emit_branch_if_true(BytecodeGen *, jumpS *js, AstRef id);
static int *emit_jump_if_true(BytecodeGen *, jumpS *js, AstRef id);
static int *emit_jump_if_false(BytecodeGen *, jumpS *js, AstRef id);
static int *emit_jump_if_not_nil(BytecodeGen *, Source site, jumpS *js, AstRef id);
static int *emit_jump_if_nil(BytecodeGen *, Source site, jumpS *js, AstRef id);
static void begin_if(BytecodeGen *, Source site, JBuf *s, AstRef x, int z);
static void add_elif_clause(BytecodeGen *, Source site, JBuf *s, AstRef x);
static void add_else_clause(BytecodeGen *, Source site, JBuf *s);
static void add_then_clause(BytecodeGen *, Source site, JBuf *s);
static void close_if(BytecodeGen *, Source site, JBuf *s);
static int emit_jump(BytecodeGen *, Source site, int dst);
static int emit_bytecode_x(BytecodeGen *, Source site, int k, int x);
static int emit_bytexy(BytecodeGen *, Source site, int k, int x, int y);
static int emit_bytexyz(BytecodeGen *, Source site, int k, int x, int y, int z);
static void patch_jump2(BytecodeGen *, int src, int dst);
static void patch_jumps2(BytecodeGen *, BCPos *js, BCPos j);
static void patch_jump(BytecodeGen *, BCPos i);
static void patch_jumps(BytecodeGen *, BCPos *js);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static BCPos emit_jump(BytecodeGen *gen, Source site, BCPos target_position)
{
	BCPos relative_distance = target_position - gen->bytecode_buffer.position;
	return emit_bytecode_x(gen, site, BYTECODE_JUMP, relative_distance);
}

// Todo, instead here we need to pass in a bytecode buffer, we can use a temporary fixed sized
// buffer because all instructions emitted are relative to the current function, once the thing is
// done, we emit the instructions to the real module buffer, which should make this not dependent
// on the main state
static BCPos emit_bytecode(BytecodeGen *gen, Source site, Bytecode byte)
{
	BytecodeBuffer *buffer = & gen->bytecode_buffer;
	buffer->bytecode[buffer->position] = byte;
	return buffer->position ++;

	// heap_array_add(state->lines,   site);
	// heap_array_add(state->bytebuf, byte);
	// return state->bytecur ++;
}

static BCPos emit_bytecode_x(BytecodeGen *gen, Source site, int k, int x)
{
	Bytecode byte = BYTECODE_XXX(k, x);
	return emit_bytecode(gen, site, byte);
}

static int emit_bytexy(BytecodeGen *C, Source site, int k, int x, int y)
{
	Bytecode byte=BYTECODE_XYY(k,x,y);
	ASSERT(BYTECODE_TYPE(byte)==k);
	ASSERT(BYTECODE_ARGX(byte)==x);
	ASSERT(BYTECODE_ARGY(byte)==y);
	return emit_bytecode(C,site,byte);
}

static BCPos emit_bytexyz(BytecodeGen *C, Source site, int k, int x, int y, int z)
{
	Bytecode byte = BYTECODE_XYZ(k, x, y, z);
	return emit_bytecode(C, site, byte);
}

static void patch_jump2(BytecodeGen *gen, BCPos source_position, BCPos target_position)
{
	Bytecode *bytecode_buffer = gen->bytecode_buffer.bytecode;
	Bytecode source_bytecode = bytecode_buffer[source_position];

	BCPos relative_distance = target_position - source_position;
	switch (BYTECODE_TYPE(source_bytecode))
	{
		case BYTECODE_JUMP:
		{
			bytecode_buffer[source_position].b_x = relative_distance;
			bytecode_buffer[source_position] = BYTECODE_XXX(BYTECODE_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BYTECODE_JZ:
		case BYTECODE_JNZ:
		{
			bytecode_buffer[source_position] = BYTECODE_XYZ(BYTECODE_TYPE(source_bytecode), relative_distance, BYTECODE_ARGY(source_bytecode), BYTECODE_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static void patch_jumps2(BytecodeGen *par, BCPos *js, BCPos dst)
{
	FOR_ARRAY(i, js) {
		patch_jump2(par, js[i], dst);
	}
}

static void patch_jump(BytecodeGen *gen, BCPos j)
{
	patch_jump2(gen, j, gen->bytecode_buffer.position);
}

static void patch_jumps(BytecodeGen *par, BCPos *js) {
	FOR_ARRAY(i, js) {
		patch_jump(par, js[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Todo, interning ...
static int add_const_int(elf_State *state, i64 i)
{
	Index index = dynamic_array_allocate(state->integers, 1);
	state->integers[index] = i;
	return index;
}

// Todo, interning ...
static int add_const_num(elf_State *state, f64 i)
{
	Index index = dynamic_array_allocate(state->numbers, 1);
	state->numbers[index] = i;
	return index;
}

static u32 emit_load_constant_int(BytecodeGen *gen, Source site, u32 dest, i64 integer)
{
	u32 index = add_const_int(gen->state, integer);
	return emit_bytexy(gen, site, BYTECODE_LOADKINT, dest, index);
}

static u32 emit_load_constant_num(BytecodeGen *gen, Source site, u32 dest, f64 number)
{
	u32 index = add_const_num(gen->state, number);
	return emit_bytexy(gen, site, BYTECODE_LOADKNUM, dest, index);
}

// Todo, this should have been a string from the start!
static u32 emit_load_constant_str(BytecodeGen *gen, Source site, u32 dest, char *data)
{
	GCStr str = new_string_from_data(gen->state, data);

	u32 index = dynamic_array_allocate(gen->state->globals->array, 1);
	to_str(&gen->state->globals->array[index], str);

	return emit_bytexy(gen, site, BYTECODE_GETGLOBAL, dest, index);
}

static u32 emit_load_nil(BytecodeGen *gen, Source site, u32 dest)
{
	return emit_bytexy(gen, site, BYTECODE_LOADNIL, dest, 0);
}

static u32 emit_get_global_bytecode(BytecodeGen *gen, Source site, u32 dest, u32 global_index)
{
	return emit_bytexy(gen, site, BYTECODE_GETGLOBAL, dest, global_index);
}

static u32 emit_reload_bytecode(BytecodeGen *gen, Source site, u32 dst, u32 src)
{
	return emit_bytexy(gen, site, BYTECODE_RELOAD, dst, src);
}

static u32 emit_return_bytecode(BytecodeGen *gen, Source site, GenMemorySlot slots, u32 nslots)
{
	return emit_bytexy(gen, site, BYTECODE_RETURN, slots, nslots);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Value get_global_value_from_index(elf_State *state, u32 global_index)
{
	return state->globals->array[global_index];
}

static void print_value_basic_shallow(Value value)
{
	switch (value.tag)
	{
		case ELF_VALUE_TYPE_NIL:        printf("nil"                      ); break;
		case ELF_VALUE_TYPE_INTEGER:    printf("%lli"  , value.x_int      ); break;
		case ELF_VALUE_TYPE_NUMBER:     printf("%f"    , value.x_num      ); break;
		case ELF_VALUE_TYPE_HANDLE:     printf("h%llX" , value.x_int      ); break;
		case ELF_VALUE_TYPE_STRING:     printf("%s"    , value.x_str->data); break;
		case ELF_VALUE_TYPE_CLOSURE:    printf("C()"); break;
		case ELF_VALUE_TYPE_CFUNCTION:   printf("F()"); break;
		case ELF_VALUE_TYPE_BUFFER:     printf("B()"); break;
		case ELF_VALUE_TYPE_TABLE:      printf("T()"); break;
		default:              printf("(?)"); break;
	}
}

void print_bytecode_function(elf_State *state, BytecodeFunction function)
{
	printf("FUNCTION:\n");
	printf("  .arity       = %i\n", function.arity);
	printf("  .variadic    = %s\n", function.variadic ? "true" : "false");
	printf("  .offset      = %i\n", function.offset);
	printf("  .length      = %i\n", function.length);
	printf("  .captures    = %i\n", function.captures);
	printf("  .stack_size  = %i\n", function.stack_size);
	printf("BODY:\n");

	Bytecode *bytecode_buffer = state->bytebuf + function.offset;
	for (BCPos i = 0; i < function.length; ++ i)
	{
		Bytecode byte = bytecode_buffer[i];

		printf("  ");
		switch (byte.b_type)
		{
			case BYTECODE_RELOAD:
			{
				printf("r%i = load(r%i)", byte.b_x, byte.b_y);
			}
			break;
			case BYTECODE_LOADKINT:
			{
				printf("r%i = int %lli", byte.b_x, state->integers[byte.b_y]);
			}
			break;
			case BYTECODE_CALL:
			{
				printf("r%i = call(nargs=%i, nrets=%i)", byte.b_x, byte.b_y, byte.b_z);
			}
			break;
			case BYTECODE_GETGLOBAL:
			{
				printf("r%i = load(g%i)", byte.b_x, byte.b_y);
				Value value = get_global_value_from_index(state, byte.b_y);
				printf(" // ");
				print_value_basic_shallow(value);
			}
			break;
			case BYTECODE_CLOSURE:
			{
				printf("r%i = closure(func_id=%i)", byte.b_x, byte.b_y);
			}
			break;
			case BYTECODE_SETGLOBAL:
			{
				printf("g%i = load(r%i)", byte.b_x, byte.b_y);
			}
			break;
			default:
			{
				printf("%s", Static_StrFromBytecode[byte.b_type]);
			}
			break;
		}
		printf("\n");
	}
	printf("END\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////