
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
static int emit_bytecode(BytecodeGen *, Source site, Bytecode byte);
static int emit_bytex(BytecodeGen *, Source site, int k, int x);
static int emit_bytexy(BytecodeGen *, Source site, int k, int x, int y);
static int emit_bytexyz(BytecodeGen *, Source site, int k, int x, int y, int z);
static void patch_jump2(BytecodeGen *, int src, int dst);
static void patch_jumps2(BytecodeGen *, BCPos *js, BCPos j);
static void patch_jump(BytecodeGen *, BCPos i);
static void patch_jumps(BytecodeGen *, BCPos *js);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static BCPos emit_jump(BytecodeGen *parser, Source site, BCPos dst)
{
	BCPos rel = dst - parser->state->bytecur;
	return emit_bytex(parser, site, BYTECODE_J, rel);
}

static int emit_branch_if(BytecodeGen *parser, jumpS *js, b32 if_true, AstRef expr)
{
	int jmp;
	switch (expr->kind)
	{
		case AST_AND:
		{
			emit_jump_if_false(parser, js, expr->ast_binary_expr.x);
			jmp = emit_branch_if(parser, js, if_true, expr->ast_binary_expr.y);
		}
		break;
		case AST_OR:
		{
			emit_jump_if_true(parser, js, expr->ast_binary_expr.x);
			jmp = emit_branch_if(parser, js, if_true, expr->ast_binary_expr.y);
		}
		break;
		default:
		{
			int mem;
			MEMORY_STATE_SCOPE(parser)
			{
				mem=expr_to_any_mem(parser,expr);
			}
			if (if_true)
			{
				jmp=emit_bytexy(parser,expr->site,BYTECODE_JNZ,NO_JUMP,mem);
				heap_array_add(js->t,jmp);
			}
			else
			{
				jmp=emit_bytexy(parser,expr->site,BYTECODE_JZ,NO_JUMP,mem);
				heap_array_add(js->f,jmp);
			}
		} break;
	}
	return jmp;
}

static inline int emit_branch_if_false(BytecodeGen *parser, jumpS *js, AstRef id)
{
	return emit_branch_if(parser,js,0,id);
}

static inline int emit_branch_if_true(BytecodeGen *parser, jumpS *js, AstRef id)
{
	return emit_branch_if(parser,js,1,id);
}

/* similar to branch if true, but additionally all false jumps converge here */
static inline BCPos *emit_jump_if_true(BytecodeGen *fs, jumpS *js, AstRef id)
{
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	free_heap_array(js->f);
	js->f = 0;
	return js->t;
}

static inline BCPos *emit_jump_if_false(BytecodeGen *fs, jumpS *js, AstRef id)
{
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	free_heap_array(js->t);
	js->t = 0;
	return js->f;
}

// todo: dedicated instructions?
static inline int *emit_jump_if_not_nil(BytecodeGen *parser, Source site, jumpS *js, AstRef id)
{
	__debugbreak();
	// return emit_jump_if_false(parser,js,create_binary_expr_ast(parser,site,AST_EQ,id,tree_nil(parser,site)));
	return 0;
}

// todo: dedicated instructions?
static inline int *emit_jump_if_nil(BytecodeGen *parser, Source site, jumpS *js, AstRef id)
{
	__debugbreak();
	// return emit_jump_if_true(parser,js,create_binary_expr_ast(parser,site,AST_EQ,id,tree_nil(parser,site)));
	return 0;
}

static void begin_if(BytecodeGen *parser, Source site, JBuf *jb, AstRef x, int if_true)
{
	jumpS js = {0};
	emit_branch_if(parser,&js,if_true,x);
	if (if_true) {
		ASSERT(js.t != 0);
		patch_jumps(parser,js.f);
		free_heap_array(js.f);
		js.f = 0;
		jb->jz = js.t;
	} else {
		ASSERT(js.f != 0);
		patch_jumps(parser,js.t);
		free_heap_array(js.t);
		js.t = 0;
		jb->jz = js.f;
	}
}

/* closes previous conditional block by emitting
escape jump, patches previous jz (jump if false)
list to enter this block. */
void add_else_clause(BytecodeGen *fs, Source site, JBuf *s) {
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,site,-1);
	heap_array_add(s->j,j);

	patch_jumps(fs,s->jz);
	free_heap_array(s->jz);
	s->jz = 0;
}



static void add_elif_clause(BytecodeGen *parser, Source site, JBuf *s, AstRef x) {
	add_else_clause(parser,site,s);
	begin_if(parser,site,s,x,0);
}



static void add_then_clause(BytecodeGen *parser, Source site, JBuf *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. (can't believe I used the word natuarally twice)
	---
	(Can't believe you misspelled naturally) */
	patch_jumps(parser,s->j);
	free_heap_array(s->j);
	s->j = 0;
}

void close_if(BytecodeGen *fs, Source site, JBuf *s)
{
	/* collect missing else branch */
	if (s->jz != 0) {
		patch_jumps(fs,s->jz);
		free_heap_array(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		patch_jumps(fs,s->j);
		free_heap_array(s->j);
		s->j = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Todo, instead here we need to pass in a bytecode buffer, we can use a temporary fixed sized
// buffer because all instructions emitted are relative to the current function, once the thing is
// done, we emit the instructions to the real module buffer, which should make this not dependent
// on the main state
static int emit_bytecode(BytecodeGen *gen, Source site, Bytecode byte)
{
	elf_State *state = gen->state;
	heap_array_add(state->lines, site);
	heap_array_add(state->bytebuf, byte);
	return state->bytecur ++;
}

static int emit_bytex(BytecodeGen *C, Source site, int k, int x)
{
	Bytecode byte=BYTECODE_XXX(k,x);
	ASSERT(BYTECODE_OP(byte)==k);
	ASSERT(BYTECODE_ARGX(byte)==x);
	return emit_bytecode(C,site,byte);
}

static int emit_bytexy(BytecodeGen *C, Source site, int k, int x, int y)
{
	Bytecode byte=BYTECODE_XYY(k,x,y);
	ASSERT(BYTECODE_OP(byte)==k);
	ASSERT(BYTECODE_ARGX(byte)==x);
	ASSERT(BYTECODE_ARGY(byte)==y);
	return emit_bytecode(C,site,byte);
}

static int emit_bytexyz(BytecodeGen *C, Source site, int k, int x, int y, int z)
{
	Bytecode byte=BYTECODE_XYZ(k,x,y,z);
	ASSERT(BYTECODE_OP(byte)==k);
	ASSERT(BYTECODE_ARGX(byte)==x);
	ASSERT(BYTECODE_ARGY(byte)==y);
	ASSERT(BYTECODE_ARGZ(byte)==z);
	return emit_bytecode(C,site,byte);
}

static void patch_jump2(BytecodeGen *parser, BCPos src, BCPos dst)
{
	Bytecode byte, *bytebuf;

	bytebuf = parser->state->bytebuf;
	byte = bytebuf[src];

	int j = dst - src;
	switch (BYTECODE_OP(byte)) {
#if 0
		case BYTECODE_EXIT_LOOP_JUMP:
#endif
		case BYTECODE_J: {
			bytebuf[src].b_x = j;
			bytebuf[src]=BYTECODE_XXX(BYTECODE_OP(byte),j);
		} break;

		case BYTECODE_JZ: case BYTECODE_JNZ: {
			// bytebuf[src].x = j;
			bytebuf[src]=BYTECODE_XYZ(BYTECODE_OP(byte),j,BYTECODE_ARGY(byte),BYTECODE_ARGZ(byte));
		} break;

		default: NO_CODE;
	}
}

static void patch_jumps2(BytecodeGen *par, BCPos *js, BCPos dst)
{
	FOR_ARRAY(i, js) {
		patch_jump2(par, js[i], dst);
	}
}

static void patch_jump(BytecodeGen *par, BCPos j)
{
	patch_jump2(par, j, par->state->bytecur);
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
		case ELF_TNIL:        printf("nil"                      ); break;
		case ELF_TINTEGER:    printf("%lli"  , value.x_int      ); break;
		case ELF_TNUMBER:     printf("%f"    , value.x_num      ); break;
		case ELF_THANDLE:     printf("h%llX" , value.x_int      ); break;
		case ELF_TSTRING:     printf("%s"    , value.x_str->data); break;
		case ELF_TCLOSURE:    printf("C()"); break;
		case ELF_TFUNCTION:   printf("F()"); break;
		case ELF_TBUFFER:     printf("B()"); break;
		case ELF_TTABLE:      printf("T()"); break;
		default:              printf("(?)"); break;
	}
}

void print_bytecode_function(elf_State *state, BytecodeFunction function)
{
	Bytecode *bytecode_buffer = state->bytebuf;
	printf("FUNCTION:\n");
	printf("  .variadic   = %s\n", function.variadic ? "true" : "false");
	printf("  .arity      = %i\n", function.arity);
	printf("  .ncaptures  = %i\n", function.ncaptures);
	printf("  .stacksize  = %i\n", function.stacksize);
	printf("  .numbytes   = %i\n", function.numbytes);
	printf("  .bytes      = %i\n", function.bytes);
	printf("BODY:\n");

	for (u32 i = 0; i < function.numbytes; ++ i)
	{
		Bytecode byte = bytecode_buffer[i];
		printf("  ");
		switch (byte.b_k)
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
				printf("%s", Static_StrFromBytecode[byte.b_k]);
			}
			break;
		}
		printf("\n");
	}
	printf("END\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////