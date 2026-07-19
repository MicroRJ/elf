
static inline b32 bc_slot_is_valid(BcSlot memory)
{
	return memory.slot >= 0;
}

static inline i32 unwrap_slot(BcSlot memory)
{
	ASSERT(bc_slot_is_valid(memory));
	return memory.slot;
}

static BcSlot emit_expr(BcGen *gen, Ir expr, BcSlot out, u32 nout);
static void do_stat(BcGen *gen, Ir stat);
static void generate_bytecode_function(BcGen *gen, IrFunction *function);
static BcFunction bg_generate_module(elf_State *state, Arena *arena, IrModule module, elf_StrSlice source, elf_String *source_name);
static void define_label(BcGen *gen, u32 label);
static void jump_to_label(BcGen *gen, SourceSite site, u32 label);
static void jump_if_false_slot_to_label(BcGen *gen, SourceSite site, BcSlot pred, u32 label);
static void jump_if_expr_true(BcGen *gen, Ir expr, u32 true_label);
static void jump_if_expr_false(BcGen *gen, Ir expr, u32 false_label);

typedef enum
{
	GENERATION_ERROR_GENERIC = 0,
	GENERATION_ERROR_INTERNAL,
	GENERATION_ERROR_UNREFERENCED_ENTITY,
	GENERATION_ERROR_INVALID_LVALUE,
	GENERATION_ERROR_INVALID_EXPRESSION,
	GENERATION_ERROR_UNDECLARED_IDENTIFIER,
}
GenerationError;

static void report_generation_error(BcGen *gen, GenerationError error, SourceSite site, const char *format, ...)
{
	(void)gen;
	(void)error;

	va_list args;
	va_start(args, format);
	Scratch scratch = get_scratch();
	char *message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1);

	if (site.line_index) {
		log_linef(LOG_LEVEL_ERROR, "generation error:%u: %s", site.line_index, message);
	}
	else {
		log_linef(LOG_LEVEL_ERROR, "generation error: %s", message);
	}
	end_scratch(scratch);
	ASSERT(!"Generation Error");
}

static b32 source_slice_equal(SourceSite left, SourceSite right)
{
	return left.data == right.data &&
		left.size == right.size &&
		left.line_start == right.line_start &&
		left.line_index == right.line_index;
}

static void emit_source_map_entry(BcGen *gen, SourceSite site, u32 byte_position)
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

/* dynamic module */
static int add_const_int(elf_State *state, i64 i)
{
	ASSERT(state->integer_constant_count < state->integer_constant_capacity);
	u32 index = state->integer_constant_count++;
	state->integer_constants[index] = i;
	return index;
}

static int add_const_num(elf_State *state, f64 i)
{
	ASSERT(state->number_constant_count < state->number_constant_capacity);
	u32 index = state->number_constant_count++;
	state->number_constants[index] = i;
	return index;
}

static u32 append_bytecode(elf_State *state, Bytecode *bytecode, u32 count)
{
	ASSERT(state->bytecode_count + count <= state->bytecode_capacity);
	u32 offset = state->bytecode_count;
	copy_memory(state->bytecode + offset, bytecode, sizeof(*bytecode) * count);
	state->bytecode_count += count;
	return offset;
}

#define BC_LABEL_INVALID     ((u32)-1)

/* emitters */
static void patch_jump_to_position(BcGen *gen, u32 source_position, u32 target_position)
{
	Bytecode source_bytecode = gen->bytecode[source_position];

	i32 relative_distance = (i32)target_position - (i32)source_position;
	ASSERT(relative_distance >= -32768 && relative_distance <= 32767);

	switch (BC_TYPE(source_bytecode))
	{
		case BC_JUMP:
		{
			gen->bytecode[source_position] = BC_XXX(BC_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BC_JZ:
		case BC_JNZ:
		{
			gen->bytecode[source_position] = BC_XYZ(BC_TYPE(source_bytecode), relative_distance, BC_ARGY(source_bytecode), BC_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static BytecodeLabel *get_bytecode_label(BcGen *gen, u32 label)
{
	ASSERT(label < ARRAY_COUNT(gen->labels));
	return &gen->labels[label];
}

static u32 create_label(BcGen *gen)
{
	ASSERT(gen->label_count < ARRAY_COUNT(gen->labels));
	return gen->label_count ++;
}

static u32 attach_label(BcGen *gen, Ir ir_label)
{
	ASSERT(ir_label);
	ASSERT(ir_label->kind == IR_LABEL);

	if (!ir_label->ir_label.has_bytecode_label)
	{
		ir_label->ir_label.bytecode_label = create_label(gen);
		ir_label->ir_label.has_bytecode_label = true;
	}

	return ir_label->ir_label.bytecode_label;
}

static void record_jump_patch(BcGen *gen, u32 bytecode_position, u32 label)
{
	ASSERT(label != BC_LABEL_INVALID);
	ASSERT(gen->jump_patch_count < ARRAY_COUNT(gen->jump_patches));

	BytecodeJumpPatch *patch = gen->jump_patches + gen->jump_patch_count++;
	patch->bytecode_position = bytecode_position;
	patch->label = label;
}

static void patch_bytecode_labels(BcGen *gen)
{
	for (u32 i = 0; i < gen->jump_patch_count; ++ i)
	{
		BytecodeJumpPatch patch = gen->jump_patches[i];
		BytecodeLabel *label = get_bytecode_label(gen, patch.label);
		ASSERT(label->defined);
		patch_jump_to_position(gen, patch.bytecode_position, (u32)label->position);
	}
	gen->jump_patch_count = 0;
}

static int emit_bc(BcGen *gen, SourceSite site, Bytecode byte)
{
	ASSERT(gen->bytecode_count < gen->bytecode_capacity);
	int position = gen->bytecode_count++;
	gen->bytecode[position] = byte;
	emit_source_map_entry(gen, site, position);
	return position;
}

static int emit_x_bc(BcGen *gen, SourceSite site, int k, int x)
{
	Bytecode byte = BC_XXX(k, x);
	return emit_bc(gen, site, byte);
}

static int emit_xy_bc(BcGen *gen, SourceSite site, int k, int x, int y)
{
	Bytecode byte = BC_XYY(k,x,y);
	return emit_bc(gen,site,byte);
}

static int emit_xyz_bc(BcGen *gen, SourceSite site, int k, int x, int y, int z)
{
	Bytecode byte = BC_XYZ(k, x, y, z);
	return emit_bc(gen, site, byte);
}

static u32 emit_jump_bc(BcGen *gen, SourceSite site)
{
	return emit_x_bc(gen, site, BC_JUMP, 0);
}

static u32 emit_jump_if_bc(BcGen *gen, SourceSite site, BytecodeType type, BcSlot pred)
{
	ASSERT(type == BC_JZ || type == BC_JNZ);
	return emit_xy_bc(gen, site, type, 0, unwrap_slot(pred));
}

static u32 emit_load_int_bc(BcGen *gen, SourceSite site, BcSlot dest, i64 integer)
{
	u32 index = add_const_int(gen->state, integer);
	return emit_xy_bc(gen, site, BC_LOADKINT, unwrap_slot(dest), index);
}

static u32 emit_load_num_bc(BcGen *gen, SourceSite site, BcSlot dest, f64 number)
{
	u32 index = add_const_num(gen->state, number);
	return emit_xy_bc(gen, site, BC_LOADKNUM, unwrap_slot(dest), index);
}

static u32 emit_load_str_bc(BcGen *gen, SourceSite site, BcSlot dest, elf_String *atom)
{
	elf_Value value = value_from_atom(atom);
	u32 index = elf_array_add(gen->state, gen->state->globals, value);
	return emit_xy_bc(gen, site, BC_GETGLOBAL, unwrap_slot(dest), index);
}

static u32 emit_load_nil_bc(BcGen *gen, SourceSite site, BcSlot dest)
{
	return emit_xy_bc(gen, site, BC_LOADNIL, unwrap_slot(dest), 0);
}

static u32 emit_load_glb_bc(BcGen *gen, SourceSite site, BcSlot dest, u32 global_index)
{
	return emit_xy_bc(gen, site, BC_GETGLOBAL, unwrap_slot(dest), global_index);
}

static u32 emit_reload_bc(BcGen *gen, SourceSite site, BcSlot dst, BcSlot src)
{
	return emit_xy_bc(gen, site, BC_RELOAD, unwrap_slot(dst), unwrap_slot(src));
}

static u32 emit_return_bc(BcGen *gen, SourceSite site, BcSlot out, u32 nout)
{
	i32 slot_index = nout ? unwrap_slot(out) : 0;
	return emit_xy_bc(gen, site, BC_RETURN, slot_index, nout);
}

static Ir bg_check_type(Ir ir, IrKind kind)
{
	ASSERT(ir);
	ASSERT(ir->kind == kind);
	return ir;
}

/* memory management */
static BcSlot mark_slots(BcGen *gen)
{
	return (BcSlot){gen->stack_top};
}

static void restore_slots(BcGen *bg, BcSlot stack_top)
{
	bg->stack_top = stack_top.slot;
}

static BcSlot alloc_slot(BcGen *gen)
{
	BcSlot stack_top = {gen->stack_top};
	gen->stack_top += 1;
	if (gen->stack_size < gen->stack_top) {
		gen->stack_size = gen->stack_top;
	}
	return stack_top;
}

static BcSlot ensure_slot(BcGen *gen, BcSlot slot)
{
	if (!bc_slot_is_valid(slot)) {
		slot = alloc_slot(gen);
	}
	return slot;
}

/* main */
static BcGen *bg_create(elf_State *state, Arena *arena, u32 bytecode_function_base)
{
	BcGen *gen = arena_push_zero(arena, sizeof(*gen));
	gen->state = state;
	gen->arena = arena;
	gen->bytecode_function_base = bytecode_function_base;

	u32 bytecode_capacity = 1 << 13;
	gen->bytecode = arena_push_zero(arena, sizeof(*gen->bytecode) * bytecode_capacity);
	gen->bytecode_capacity = bytecode_capacity;
	gen->bytecode_count = 0;

	u32 source_map_capacity = 1 << 13;
	gen->source_map_buffer.entries = arena_push_zero(arena, sizeof(*gen->source_map_buffer.entries) * source_map_capacity);
	gen->source_map_buffer.capacity = source_map_capacity;
	gen->source_map_buffer.count = 0;
	return gen;
}

static void generate_bytecode_function(BcGen *gen, IrFunction *function)
{
	ASSERT(function);

	PROF_BLOCK("bytecode.generate_function")
	{
		gen->bytecode_count           =0;
		gen->source_map_buffer.count  =0;
		gen->stack_size               =function->arity;
		gen->stack_top                =function->arity;
		zero_memory(gen->labels, sizeof(gen->labels));
		gen->label_count              = 0;
		gen->jump_patch_count         = 0;

		Ir body = function->body;
		ASSERT(gen->bytecode_count == 0);

		BcSlot memory_checkpoint = mark_slots(gen);
		do_stat(gen, body);
		restore_slots(gen, memory_checkpoint);

		if (gen->bytecode_count == 0 ||
			BC_TYPE(gen->bytecode[gen->bytecode_count - 1]) != BC_RETURN)
		{
			emit_return_bc(gen, body->site, NO_MEMORY, 0);
		}

		patch_bytecode_labels(gen);
	}
}

static SourceMapEntry *copy_source_map(Arena *arena, SourceMapEntry *entries, u32 count, u32 bytecode_offset)
{
	if (count == 0)
	{
		return 0;
	}

	SourceMapEntry *source_map = arena_push(arena, sizeof(*source_map) * count);
	for (u32 i = 0; i < count; ++ i)
	{
		SourceMapEntry entry = entries[i];
		entry.byte_start += bytecode_offset;
		entry.byte_end   += bytecode_offset;
		source_map[i] = entry;
	}
	return source_map;
}

static BcFunction *reserve_bytecode_functions(elf_State *state, u32 count)
{
	ASSERT(state->bytecode_function_count + count <= state->bytecode_function_capacity);

	u32 index = state->bytecode_function_count;
	state->bytecode_function_count += count;
	return state->bytecode_functions + index;
}

static BcFunction bg_generate_module(elf_State *state, Arena *arena, IrModule module, elf_StrSlice source, elf_String *source_name)
{
	ASSERT(module.functions);
	ASSERT(module.function_count > 0);
	ASSERT(module.entry_index == 0);

	u32 bytecode_function_base = state->bytecode_function_count;
	BcFunction *bytecode_functions = reserve_bytecode_functions(state, module.function_count);

	BcGen *gen = bg_create(state, arena, bytecode_function_base);
	PROF_BLOCK("compiler.codegen")
	{
		for (u32 i = 0; i < module.function_count; ++ i)
		{
			IrFunction *function = module.functions + i;
			BcFunction *bytecode_function = bytecode_functions + i;
			generate_bytecode_function(gen, function);

			u32 bytecode_offset = append_bytecode(state, gen->bytecode, gen->bytecode_count);
			SourceMapEntry *source_map = copy_source_map(&state->arena
			,	gen->source_map_buffer.entries, gen->source_map_buffer.count, bytecode_offset);

			bytecode_function->variadic         = function->variadic;
			bytecode_function->arity            = function->arity;
			bytecode_function->offset           = bytecode_offset;
			bytecode_function->length           = gen->bytecode_count;
			bytecode_function->captures         = function->capture_count;
			bytecode_function->stack_size       = gen->stack_size;
			bytecode_function->source_map       = source_map;
			bytecode_function->source_map_count = gen->source_map_buffer.count;
			bytecode_function->source_data      = source.data;
			bytecode_function->source_size      = (u32)source.size;
			bytecode_function->source_name      = source_name;
		}
	}

	return bytecode_functions[module.entry_index];
}

static BcSlot ensure_expr_memory(BcGen *gen, Ir expr)
{
	BcSlot slot = NO_MEMORY;
	if (expr->kind == IR_LOAD_LOCAL) {
		Ir local = expr->ir_load_local.local;
		bg_check_type(local, IR_LOCAL);
		slot = local->ir_local.slot;
	}
	else {
		slot = emit_expr(gen, expr, NO_MEMORY, 1);
	}

	ASSERT(bc_slot_is_valid(slot));
	return slot;
}

static BcSlot bg_emit_new_table_expr(BcGen *gen, Ir expr, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	out = ensure_slot(gen, out);
	emit_xy_bc(gen, expr->site, BC_TABLE, unwrap_slot(out), 0);
	return out;
}

static BcSlot bg_emit_call(BcGen *gen, Ir ir, BcSlot out, u32 nout)
{
	Ir expr = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;

	BcSlot stack_top = mark_slots(gen);;
	{
		BcSlot this_slot;
		BcSlot expr_slot;
		if (expr->kind == IR_META_FIELD)
		{
			bg_check_type(expr->ir_binary.y, IR_ATOM);

			expr_slot = emit_expr(gen, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = emit_expr(gen, expr->ir_binary.x, NO_MEMORY, 1);

			emit_xyz_bc(gen, expr->site, BC_GETMETAFIELD
			,	unwrap_slot(expr_slot), unwrap_slot(this_slot), unwrap_slot(expr_slot));
		}
		else if (expr->kind == IR_FIELD)
		{
			bg_check_type(expr->ir_binary.y, IR_ATOM);

			expr_slot = emit_expr(gen, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = emit_expr(gen, expr->ir_binary.x, NO_MEMORY, 1);

			emit_xyz_bc(gen, expr->site, BC_GETFIELD
			,	unwrap_slot(expr_slot), unwrap_slot(this_slot), unwrap_slot(expr_slot));
		}
		else
		{
			expr_slot = emit_expr(gen, expr, NO_MEMORY, 1);
			this_slot = alloc_slot(gen);

			emit_xy_bc(gen, ir->site, BC_RELOAD, unwrap_slot(this_slot), 0);
		}

		ASSERT(expr_slot.slot == stack_top.slot + 0);
		ASSERT(this_slot.slot == stack_top.slot + 1);

		for (u32 i = 0; i < nargs; ++ i)
		{
			BcSlot arg_slot = emit_expr(gen, args.items[i], NO_MEMORY, 1);
			ASSERT(arg_slot.slot == stack_top.slot + 2 + (i32)i);
		}
	}
	restore_slots(gen, stack_top);

	emit_xyz_bc(gen, expr->site, BC_CALL, unwrap_slot(stack_top), nargs + 1, nout);

	if (nout < 1) {
		return out;
	}

	out = ensure_slot(gen, out);

	if (out.slot != stack_top.slot)
	{
		if (nout > 1)
		{
			report_generation_error(gen, GENERATION_ERROR_GENERIC, expr->site, "multi-returns are not fully supported yet!");
		}

		emit_reload_bc(gen, expr->site, out, stack_top);
	}

	return out;
}

static u32 bc_op(IrKind type)
{
	switch (type)
	{
		case IR_ADD:               return BC_ADD;
		case IR_SUB:               return BC_SUB;
		case IR_DIV:               return BC_DIV;
		case IR_MUL:               return BC_MUL;
		case IR_POW:               return BC_POW;
		case IR_MOD:               return BC_MOD;
		case IR_NOT_EQ:            return BC_NEQ;
		case IR_EQ:                return BC_EQ;
		case IR_LESS_THAN:         return BC_LT;
		case IR_LESS_THAN_EQ:      return BC_LTEQ;
		case IR_BITWISE_NOT:       return BC_BIT_NOT;
		case IR_BITWISE_OR:        return BC_BIT_OR;
		case IR_BITWISE_AND:       return BC_BIT_AND;
		case IR_SHIFT_LEFT:        return BC_BIT_SHL;
		case IR_SHIFT_RIGHT:       return BC_BIT_SHR;
		case IR_BITWISE_XOR:       return BC_BIT_XOR;
		case IR_INDEX:             return BC_GETINDEX;
		case IR_FIELD:             return BC_GETFIELD;
		case IR_META_FIELD:        return BC_GETMETAFIELD;
		default: NO_CODE;
	}

	ASSERT(!"Error");
	return BC_HALT;
}

static BcSlot bg_emit_binary_expr(BcGen *bg, Ir expr, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	BcSlot memory_checkpoint = mark_slots(bg);
	BcSlot left_slot = ensure_expr_memory(bg, expr->ir_binary.x);
	BcSlot right_slot = ensure_expr_memory(bg, expr->ir_binary.y);
	restore_slots(bg, memory_checkpoint);

	out = ensure_slot(bg, out);
	emit_xyz_bc(bg, expr->site, bc_op(expr->kind)
	,	unwrap_slot(out), unwrap_slot(left_slot), unwrap_slot(right_slot));
	return out;
}

static BcSlot bg_emit_unary_expr(BcGen *bg, Ir expr, u32 bytecode_type, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	BcSlot memory_checkpoint = mark_slots(bg);
	BcSlot value_slot = ensure_expr_memory(bg, expr->ir_unary);
	restore_slots(bg, memory_checkpoint);

	out = ensure_slot(bg, out);
	emit_xy_bc(bg, expr->site, bytecode_type, unwrap_slot(out), unwrap_slot(value_slot));
	return out;
}

static BcSlot emit_truthy_or_expr(BcGen *gen, Ir expr, BcSlot out, u32 nout)
{
	BcSlot memory_checkpoint;
	BcSlot temp_slot;
	u32 done_label;

	if (nout < 1) return out;

	out = emit_expr(gen, expr->ir_binary.x, out, 1);
	memory_checkpoint = mark_slots(gen);
	temp_slot = alloc_slot(gen);
	emit_load_nil_bc(gen, expr->site, temp_slot);
	emit_xyz_bc(gen, expr->site, BC_EQ,	unwrap_slot(temp_slot), unwrap_slot(temp_slot), unwrap_slot(out));
	done_label = create_label(gen);
	jump_if_false_slot_to_label(gen, expr->site, temp_slot, done_label);
	restore_slots(gen, memory_checkpoint);
	emit_expr(gen, expr->ir_binary.y, out, 1);
	define_label(gen, done_label);
	return out;
}

static BcSlot emit_logical_expr(BcGen *gen, Ir expr, BcSlot out, u32 nout)
{
	u32 false_label;
	u32 done_label;

	if (nout < 1) return out;

	false_label = create_label(gen);
  	done_label = create_label(gen);

	jump_if_expr_false(gen, expr, false_label);
	out = ensure_slot(gen, out);
	emit_load_int_bc(gen, expr->site, out, 1);
	jump_to_label(gen, expr->site, done_label);
	define_label(gen, false_label);
	emit_load_int_bc(gen, expr->site, out, 0);
	define_label(gen, done_label);

	return out;
}

static void jump_if_true_slot_to_label(BcGen *gen, SourceSite site, BcSlot pred, u32 label)
{
	u32 jump = emit_jump_if_bc(gen, site, BC_JNZ, pred);
	record_jump_patch(gen, jump, label);
}

static void jump_if_false_slot_to_label(BcGen *gen, SourceSite site, BcSlot pred, u32 label)
{
	u32 jump = emit_jump_if_bc(gen, site, BC_JZ, pred);
	record_jump_patch(gen, jump, label);
}

static void jump_if_expr_false(BcGen *gen, Ir expr, u32 false_label)
{
	switch (expr->kind)
	{
		// 'a' && 'b'
		case IR_AND:
		{
			jump_if_expr_false(gen, expr->ir_binary.x, false_label);
			jump_if_expr_false(gen, expr->ir_binary.y, false_label);
		}
		break;
		// 'a' || 'b'
		case IR_OR:
		{
			u32 fallthrough_label = create_label(gen);
			jump_if_expr_true(gen, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_false(gen, expr->ir_binary.y, false_label);
			define_label(gen, fallthrough_label);
		}
		break;
		default:
		{
			BcSlot memory_checkpoint = mark_slots(gen);
			BcSlot pred_slot = ensure_expr_memory(gen, expr);
			jump_if_false_slot_to_label(gen, expr->site, pred_slot, false_label);
			restore_slots(gen, memory_checkpoint);
		}
		break;
	}
}

static void jump_if_expr_true(BcGen *gen, Ir expr, u32 true_label)
{
	switch (expr->kind)
	{
		// 'a' && 'b'
		case IR_AND:
		{
			u32 fallthrough_label = create_label(gen);
			jump_if_expr_false(gen, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_true(gen, expr->ir_binary.y, true_label);
			define_label(gen, fallthrough_label);
		}
		break;
		// 'a' || 'b'
		case IR_OR:
		{
			jump_if_expr_true(gen, expr->ir_binary.x, true_label);
			jump_if_expr_true(gen, expr->ir_binary.y, true_label);
		}
		break;
		default:
		{
			BcSlot memory_checkpoint = mark_slots(gen);
			BcSlot pred_slot = ensure_expr_memory(gen, expr);
			jump_if_true_slot_to_label(gen, expr->site, pred_slot, true_label);
			restore_slots(gen, memory_checkpoint);
		}
		break;
	}
}

static BcSlot do_expr_block(BcGen *gen, Ir expr, BcSlot out, u32 nout)
{
	IrArray stats = expr->ir_expr_block.stats;
	Ir value = expr->ir_expr_block.value;

	if (nout < 1)
	{
		BcSlot memory_checkpoint = mark_slots(gen);
		for (u32 i = 0; i < stats.count; ++ i) {
			do_stat(gen, stats.items[i]);
		}
		restore_slots(gen, memory_checkpoint);
		return out;
	}

	out = ensure_slot(gen, out);
	BcSlot memory_checkpoint = mark_slots(gen);
	for (u32 i = 0; i < stats.count; ++ i) {
		do_stat(gen, stats.items[i]);
	}
	BcSlot final_slot = emit_expr(gen, value, out, 1);
	ASSERT(final_slot.slot == out.slot);
	restore_slots(gen, memory_checkpoint);
	return out;
}

static BcSlot emit_expr(BcGen *gen, Ir expr, BcSlot out, u32 nout)
{
	SourceSite site = expr->site;
	switch (expr->kind)
	{
		case IR_ERROR:
		{
			return out;
		}

		case IR_FUNCTION:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);

			u32 bytecode_function_id = gen->bytecode_function_base + expr->ir_function.index;

			IrArray captures = expr->ir_function.captures;
			if (captures.count == 0)
			{
				emit_xy_bc(gen, site, BC_CLOSURE, unwrap_slot(out), bytecode_function_id);
			}
			else
			{
				BcSlot memory_checkpoint = mark_slots(gen);
				BcSlot first_capture_slot = memory_checkpoint;

				for (u32 i = 0; i < captures.count; ++ i)
				{
					BcSlot next_capture_slot = emit_expr(gen, captures.items[i], NO_MEMORY, 1);
					ASSERT(bc_slot_is_valid(next_capture_slot));
					ASSERT(first_capture_slot.slot + i == next_capture_slot.slot);
				}

				emit_xy_bc(gen, site, BC_CLOSURE, unwrap_slot(first_capture_slot), bytecode_function_id);
				if (first_capture_slot.slot != out.slot) {
					emit_reload_bc(gen, site, out, first_capture_slot);
				}
				restore_slots(gen, memory_checkpoint);
			}
		}
		break;

		case IR_CAPTURE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_xy_bc(gen, site, BC_LOADCVAL, unwrap_slot(out), expr->ir_capture);
		}
		break;

		case IR_RECURSE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_xy_bc(gen, site, BC_CURRENT_CLOSURE, unwrap_slot(out), 0);
		}
		break;

		case IR_LOAD_GLOBAL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_load_glb_bc(gen, expr->site, out, expr->ir_global);
		}
		break;

		case IR_LOAD_LOCAL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			Ir local = expr->ir_load_local.local;
			bg_check_type(local, IR_LOCAL);
			ASSERT(bc_slot_is_valid(local->ir_local.slot));

			emit_reload_bc(gen, expr->site, out, local->ir_local.slot);
		}
		break;

		case IR_GET_MEM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			Ir target = expr->ir_unary;
			bg_check_type(target, IR_LOAD_LOCAL);

			Ir local = target->ir_load_local.local;
			bg_check_type(local, IR_LOCAL);
			ASSERT(bc_slot_is_valid(local->ir_local.slot));

			emit_load_int_bc(gen, expr->site, out, unwrap_slot(local->ir_local.slot));
		}
		break;

		case IR_INTEGER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_load_int_bc(gen, expr->site, out, expr->ir_int);
		}
		break;

		case IR_NUMBER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_load_num_bc(gen, expr->site, out, expr->ir_num);
		}
		break;

		case IR_ATOM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_load_str_bc(gen, expr->site, out, expr->atom);
		}
		break;

		case IR_NIL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(gen, out);
			emit_load_nil_bc(gen, expr->site, out);
		}
		break;

		case IR_CALL:
		{
			out = bg_emit_call(gen, expr, out, nout);
		}
		break;

		case IR_TABLE:
		{
			out = bg_emit_new_table_expr(gen, expr, out, nout);
		}
		break;

		case IR_EXPR_BLOCK:
		{
			out = do_expr_block(gen, expr, out, nout);
		}
		break;

		case IR_LENGTH_INTRINSIC:
		{
			out = bg_emit_unary_expr(gen, expr, BC_GETLENGTH, out, nout);
		}
		break;

		case IR_NIL_OR:
		{
			out = emit_truthy_or_expr(gen, expr, out, nout);
		}
		break;

		case IR_AND:
		case IR_OR:
		{
			out = emit_logical_expr(gen, expr, out, nout);
		}
		break;

		case IR_EQ:
		case IR_NOT_EQ:
		case IR_LESS_THAN_EQ:
		case IR_LESS_THAN:
		case IR_DIV:
		case IR_MUL:
		case IR_MOD:
		case IR_SUB:
		case IR_ADD:
		case IR_POW:
		case IR_SHIFT_LEFT:
		case IR_SHIFT_RIGHT:
		case IR_INDEX:
		case IR_FIELD:
		case IR_META_FIELD:
		case IR_BITWISE_XOR:
		case IR_BITWISE_AND:
		case IR_BITWISE_OR:
		{
			out = bg_emit_binary_expr(gen, expr, out, nout);
		}
		break;

		default:
		{
			report_generation_error(gen, GENERATION_ERROR_INVALID_EXPRESSION, expr->site
			, "invalid expression, got: %s", ir_kind_name(expr->kind));

			ASSERT(!"Error!");
		}
		break;
	}

	return out;
}

static void bg_emit_store(BcGen *gen, SourceSite site, Ir dest, Ir expr)
{
	BcSlot memory_checkpoint = mark_slots(gen);
	{
		switch (dest->kind)
		{
			case IR_LOAD_LOCAL:
			{
				Ir local = dest->ir_load_local.local;
				bg_check_type(local, IR_LOCAL);

				BcSlot dest_slot = local->ir_local.slot;
				ASSERT(bc_slot_is_valid(dest_slot));

				BcSlot final_slot = emit_expr(gen, expr, dest_slot, 1);
				ASSERT(final_slot.slot == dest_slot.slot);
			}
			break;

			case IR_LOAD_GLOBAL:
			{
				u32 dest_slot = dest->ir_global;

				BcSlot expr_slot = ensure_expr_memory(gen, expr);
				emit_xy_bc(gen, dest->site, BC_SETGLOBAL, dest_slot, unwrap_slot(expr_slot));
			}
			break;

			case IR_FIELD:
			{
				BcSlot value_slot = ensure_expr_memory(gen, expr);
				BcSlot object_slot = ensure_expr_memory(gen, dest->ir_binary.x);
				BcSlot field_slot = ensure_expr_memory(gen, dest->ir_binary.y);

				emit_xyz_bc(gen, site, BC_SETFIELD
				,	unwrap_slot(object_slot), unwrap_slot(field_slot), unwrap_slot(value_slot));
			}
			break;

			case IR_INDEX:
			{
				BcSlot value_slot = ensure_expr_memory(gen, expr);
				BcSlot array_slot = ensure_expr_memory(gen, dest->ir_binary.x);
				BcSlot index_slot = ensure_expr_memory(gen, dest->ir_binary.y);
				emit_xyz_bc(gen, site, BC_SETINDEX
				,	unwrap_slot(array_slot), unwrap_slot(index_slot), unwrap_slot(value_slot));
			}
			break;

			default:
			{
				report_generation_error(gen, GENERATION_ERROR_INVALID_LVALUE, dest->site
				, "invalid l-value, got: %s", ir_kind_name(dest->kind));
			}
			break;
		}
	}
	restore_slots(gen, memory_checkpoint);
}

static void do_stat(BcGen *gen, Ir stat)
{
	switch (stat->kind)
	{
		case IR_ERROR:
		{
			return;
		}

		case IR_LOCAL:
		{
			BcSlot slot = stat->ir_local.slot;
			ASSERT(!bc_slot_is_valid(slot));
			if (stat->ir_local.expr)
			{
				slot = emit_expr(gen, stat->ir_local.expr, NO_MEMORY, 1);
				ASSERT(bc_slot_is_valid(slot));
			}
			else
			{
				slot = alloc_slot(gen);
			}

			stat->ir_local.slot = slot;
		}
		break;

		case IR_STORE:
		{
			bg_emit_store(gen, stat->site, stat->ir_binary.x, stat->ir_binary.y);
		}
		break;

		case IR_ARRAY_ADD:
		{
			BcSlot memory_checkpoint = mark_slots(gen);
			BcSlot table_slot = ensure_expr_memory(gen, stat->ir_binary.x);
			BcSlot value_slot = ensure_expr_memory(gen, stat->ir_binary.y);
			emit_xy_bc(gen, stat->site, BC_ARRAYADD, unwrap_slot(table_slot), unwrap_slot(value_slot));
			restore_slots(gen, memory_checkpoint);
		}
		break;

		case IR_LABEL:
		{
			define_label(gen, attach_label(gen, stat));
		}
		break;

		case IR_JUMP:
		{
			u32 label = attach_label(gen, stat->ir_jump.label);
			jump_to_label(gen, stat->site, label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			u32 label = attach_label(gen, stat->ir_jump_if_false.label);
			jump_if_expr_false(gen, stat->ir_jump_if_false.pred, label);
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = stat->ir_block.stats;

			BcSlot memory_checkpoint = mark_slots(gen);
			{
				for (u32 i = 0; i < stats.count; ++ i)
				{
					do_stat(gen, stats.items[i]);
				}
			}
			restore_slots(gen, memory_checkpoint);
		}
		break;

		case IR_IF:
		{
			Ir pred = stat->ir_if.pred;
			Ir true_clause = stat->ir_if.true_clause;
			Ir else_clause = stat->ir_if.else_clause;

			u32 else_label = create_label(gen);
			jump_if_expr_false(gen, pred, else_label);

			{
				BcSlot true_memory = mark_slots(gen);
				do_stat(gen, true_clause);
				restore_slots(gen, true_memory);
			}

			if (else_clause)
			{
				u32 end_label = create_label(gen);
				jump_to_label(gen, stat->site, end_label);

				BcSlot else_memory = mark_slots(gen);
				define_label(gen, else_label);
				restore_slots(gen, else_memory);
				do_stat(gen, else_clause);
				restore_slots(gen, else_memory);
				define_label(gen, end_label);
			}
			else
			{
				define_label(gen, else_label);
			}
		}
		break;

		case IR_RETURN:
		{
			Ir expr = stat->ir_return.expr;
			if (expr)
			{
				BcSlot slot = ensure_expr_memory(gen, expr);
				emit_return_bc(gen, stat->site, slot, 1);
			}
			else
			{
				emit_return_bc(gen, stat->site, NO_MEMORY, 0);
			}
		}
		break;

		default:
		{
			BcSlot memory_checkpoint = mark_slots(gen);
			emit_expr(gen, stat, NO_MEMORY, 0);
			restore_slots(gen, memory_checkpoint);
		}
		break;
	}
}

static void define_label(BcGen *gen, u32 label)
{
	BytecodeLabel *target = get_bytecode_label(gen, label);
	ASSERT(!target->defined);

	target->defined = true;
	target->position = gen->bytecode_count;
}

static void jump_to_label(BcGen *gen, SourceSite site, u32 label)
{
	u32 jump = emit_jump_bc(gen, site);
	record_jump_patch(gen, jump, label);
}
