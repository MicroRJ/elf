
#define NO_JUMP (-0)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int emit_bytecode(BytecodeGen *, SourceSite site, Bytecode byte);

static void emit_condition_branch(BytecodeGen *, jumpS *js, b32 if_true, IR id);
static void begin_if(BytecodeGen *, SourceSite site, JBuf *s, IR x, int z);
static void close_if(BytecodeGen *, SourceSite site, JBuf *s);
static int emit_jump(BytecodeGen *, SourceSite site, int dst);
static int emit_bytecode_x(BytecodeGen *, SourceSite site, int k, int x);
static int emit_bytexy(BytecodeGen *, SourceSite site, int k, int x, int y);
static int emit_bytexyz(BytecodeGen *, SourceSite site, int k, int x, int y, int z);
static void patch_jump2(BytecodeGen *, int src, int dst);
static void patch_jumps2(BytecodeGen *, JumpList *js, int j);
static void patch_jump(BytecodeGen *, int i);
static void patch_jumps(BytecodeGen *, JumpList *js);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static int emit_jump(BytecodeGen *gen, SourceSite site, int target_position)
{
	int relative_distance = target_position - gen->bytecode_buffer.position;
	return emit_bytecode_x(gen, site, BC_JUMP, relative_distance);
}

static b32 source_slice_equal(SourceSite left, SourceSite right)
{
	return left.data == right.data &&
		left.size == right.size &&
		left.line_start == right.line_start &&
		left.line_index == right.line_index;
}

static void emit_source_map_entry(BytecodeGen *gen, SourceSite site, u32 byte_position)
{
	SourceMapBuffer *source_map = &gen->source_map_buffer;
	if (source_map->count > 0) {
		SourceMapEntry *entry = &source_map->entries[source_map->count - 1];
		if (entry->byte_end == byte_position && source_slice_equal(entry->site, site)) {
			entry->byte_end = byte_position + 1;
			return;
		}
	}

	ASSERT(source_map->count < source_map->capacity);
	SourceMapEntry *entry = &source_map->entries[source_map->count++];
	entry->byte_start = byte_position;
	entry->byte_end = byte_position + 1;
	entry->site = site;
}

// Todo, instead here we need to pass in a bytecode buffer, we can use a temporary fixed sized
// buffer because all instructions emitted are relative to the current function, once the thing is
// done, we emit the instructions to the real module buffer, which should make this not dependent
// on the main state
static int emit_bytecode(BytecodeGen *gen, SourceSite site, Bytecode byte)
{
	BytecodeBuffer *buffer = & gen->bytecode_buffer;
	ASSERT(buffer->position < buffer->capacity);
	int position = buffer->position++;
	buffer->bytecode[position] = byte;
	emit_source_map_entry(gen, site, position);
	return position;
}

static int emit_bytecode_x(BytecodeGen *gen, SourceSite site, int k, int x)
{
	Bytecode byte = BC_XXX(k, x);
	return emit_bytecode(gen, site, byte);
}

static int emit_bytexy(BytecodeGen *C, SourceSite site, int k, int x, int y)
{
	Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_TYPE(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_bytecode(C,site,byte);
}

static int emit_bytexyz(BytecodeGen *C, SourceSite site, int k, int x, int y, int z)
{
	Bytecode byte = BC_XYZ(k, x, y, z);
	return emit_bytecode(C, site, byte);
}

static void patch_jump2(BytecodeGen *gen, int source_position, int target_position)
{
	Bytecode *bytecode_buffer = gen->bytecode_buffer.bytecode;
	Bytecode source_bytecode = bytecode_buffer[source_position];

	int relative_distance = target_position - source_position;
	switch (BC_TYPE(source_bytecode))
	{
		case BC_JUMP:
		{
			bytecode_buffer[source_position].b_x = relative_distance;
			bytecode_buffer[source_position] = BC_XXX(BC_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BC_JZ:
		case BC_JNZ:
		{
			bytecode_buffer[source_position] = BC_XYZ(BC_TYPE(source_bytecode), relative_distance, BC_ARGY(source_bytecode), BC_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static void patch_jumps2(BytecodeGen *par, JumpList *js, int dst)
{
	for (u32 i = 0; i < js->count; ++ i) {
		patch_jump2(par, js->jumps[i], dst);
	}
}

static void patch_jump(BytecodeGen *gen, int j)
{
	patch_jump2(gen, j, gen->bytecode_buffer.position);
}

static void patch_jumps(BytecodeGen *par, JumpList *js) {
	for (u32 i = 0; i < js->count; ++ i) {
		patch_jump(par, js->jumps[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Todo, interning ...
static int add_const_int(elf_State *state, i64 i)
{
	ASSERT(state->integer_constant_count < state->integer_constant_capacity);
	u32 index = state->integer_constant_count++;
	state->integer_constants[index] = i;
	return index;
}

// Todo, interning ...
static int add_const_num(elf_State *state, f64 i)
{
	ASSERT(state->number_constant_count < state->number_constant_capacity);
	u32 index = state->number_constant_count++;
	state->number_constants[index] = i;
	return index;
}

static u32 emit_load_constant_int(BytecodeGen *gen, SourceSite site, GenMemory dest, i64 integer)
{
	u32 index = add_const_int(gen->state, integer);
	return emit_bytexy(gen, site, BC_LOADKINT, gen_memory_index(dest), index);
}

static u32 emit_load_constant_num(BytecodeGen *gen, SourceSite site, GenMemory dest, f64 number)
{
	u32 index = add_const_num(gen->state, number);
	return emit_bytexy(gen, site, BC_LOADKNUM, gen_memory_index(dest), index);
}

static u32 emit_load_constant_atom(BytecodeGen *gen, SourceSite site, GenMemory dest, elf_Atom *atom)
{
	elf_Value value;
	value = value_from_atom(atom);

	u32 index = elf_array_add(gen->state, gen->state->globals, value);
	return emit_bytexy(gen, site, BC_GETGLOBAL, gen_memory_index(dest), index);
}

static u32 emit_load_nil(BytecodeGen *gen, SourceSite site, GenMemory dest)
{
	return emit_bytexy(gen, site, BC_LOADNIL, gen_memory_index(dest), 0);
}

static u32 emit_get_global(BytecodeGen *gen, SourceSite site, GenMemory dest, u32 global_index)
{
	return emit_bytexy(gen, site, BC_GETGLOBAL, gen_memory_index(dest), global_index);
}

static u32 emit_reload_bytecode(BytecodeGen *gen, SourceSite site, GenMemory dst, GenMemory src)
{
	return emit_bytexy(gen, site, BC_RELOAD, gen_memory_index(dst), gen_memory_index(src));
}

static u32 emit_return_bytecode(BytecodeGen *gen, SourceSite site, GenMemory slots, u32 nslots)
{
	i32 slot_index = nslots ? gen_memory_index(slots) : 0;
	return emit_bytexy(gen, site, BC_RETURN, slot_index, nslots);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

