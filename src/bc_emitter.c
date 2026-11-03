//
// See Copyright Notice In elf.h
//

#define BC_OPERAND_COUNT_MAX (1u << 15)

typedef struct BcJumpPatch BcJumpPatch;
struct BcJumpPatch
{
	BcJumpPatch *next;
	u32 bytecode_position;
	Ir  label;
};

typedef struct
{
	IrFunction *func;
	u32         slot;
}
BcRef;

typedef struct BcEmitter BcEmitter;
struct BcEmitter
{
	elf_State          *target;
	Arena              *arena;

	// Temporary buffers copied into the runtime prototype after emission.
	Bytecode           *bytecode;
	u32                 bytecode_capacity;
	u32                 bytecode_count;

	elf_Value          *consts;
	u32                 xconsts;
	u32                 nconsts;
	BcRef              *refs;
	u32                 xrefs;
	u32                 nrefs;

	elf_SourceMapEntry *source_map;
	u32                 source_map_capacity;
	u32                 source_map_count;

	u32                 stack_size;
	u32                 stack_top;
	BcJumpPatch        *jump_patches;
};

static inline b32 slot_is_valid(i32 slot)
{
	return slot >= 0;
}

static i32 emit_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout);
static void emit_stat(BcEmitter *emitter, Ir stat);
static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, Ir label);
static void jump_if_expr_true(BcEmitter *emitter, Ir expr, Ir true_label);
static void jump_if_expr_false(BcEmitter *emitter, Ir expr, Ir false_label);

// Scratch buffers and constant pools

// TODO(RJ): use some sort of a chunked system instead!
static void *grow_emitter_array(Arena *arena, void *items, u32 count, u32 *capacity, u32 required_count, u64 item_size)
{
	if (required_count <= *capacity) {
		return items;
	}

	u32 new_capacity = *capacity ? *capacity : 64;
	while (new_capacity < required_count)
	{
		ASSERT(new_capacity <= 0x7fffffffu);
		new_capacity *= 2;
	}

	void *new_items = arena_push(arena, item_size * new_capacity, 8);
	if (count) {
		copy_memory(new_items, items, item_size * count);
	}
	*capacity = new_capacity;
	return new_items;
}

static b32 source_map_entry_matches_site(elf_SourceMapEntry *entry, SourceSite site)
{
	return entry->source_offset == site.offset && entry->source_size == site.size && entry->line_offset == site.line_offset && entry->line_index == site.line_index;
}

static void record_source_map(BcEmitter *emitter, SourceSite site, u32 byte_position)
{
	if (emitter->source_map_count > 0) {
		elf_SourceMapEntry *entry = emitter->source_map + emitter->source_map_count - 1;
		if (entry->byte_end == byte_position && source_map_entry_matches_site(entry, site)) {
			entry->byte_end = byte_position + 1;
			return;
		}
	}

	emitter->source_map = grow_emitter_array(emitter->arena, emitter->source_map, emitter->source_map_count, &emitter->source_map_capacity, emitter->source_map_count + 1, sizeof(*emitter->source_map));
	elf_SourceMapEntry *entry = emitter->source_map + emitter->source_map_count++;
	entry->byte_start = byte_position;
	entry->byte_end = byte_position + 1;
	entry->source_offset = site.offset;
	entry->source_size = site.size;
	entry->line_offset = site.line_offset;
	entry->line_index = site.line_index;
}

static b32 consts_equal(elf_Value x, elf_Value y)
{
	return x.type == y.type && x.x_i64 == y.x_i64;
}

static u32 add_const(BcEmitter *emitter, elf_Value value)
{
	for (u32 i = 0; i < emitter->nconsts; ++i)
	{
		b32 is_ref = false;
		for (u32 j = 0; j < emitter->nrefs; ++j) {
			is_ref |= emitter->refs[j].slot == i;
		}
		if (!is_ref && consts_equal(emitter->consts[i], value)) {
			return i;
		}
	}

	if (emitter->nconsts >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	emitter->consts = grow_emitter_array(emitter->arena, emitter->consts, emitter->nconsts, &emitter->xconsts, emitter->nconsts + 1, sizeof(*emitter->consts));
	u32 index = emitter->nconsts++;
	emitter->consts[index] = value;
	return index;
}

static u32 add_func(BcEmitter *emitter, IrFunction *func)
{
	for (u32 i = 0; i < emitter->nrefs; ++i) {
		if (emitter->refs[i].func == func) {
			return emitter->refs[i].slot;
		}
	}

	if (emitter->nconsts >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	emitter->consts = grow_emitter_array(emitter->arena, emitter->consts, emitter->nconsts, &emitter->xconsts, emitter->nconsts + 1, sizeof(*emitter->consts));
	u32 slot = emitter->nconsts++;
	emitter->consts[slot] = value_nil();

	emitter->refs = grow_emitter_array(emitter->arena, emitter->refs, emitter->nrefs, &emitter->xrefs, emitter->nrefs + 1, sizeof(*emitter->refs));
	emitter->refs[emitter->nrefs++] = (BcRef) { .func = func, .slot = slot };
	return slot;
}

// Labels and jump patching
static void patch_jump(BcEmitter *emitter, u32 source_position, u32 target_position)
{
	ASSERT(source_position < emitter->bytecode_count);
	Bytecode source_bytecode = emitter->bytecode[source_position];

	i32 relative_distance = (i32)target_position - (i32)source_position;
	ASSERT(relative_distance >= -32768 && relative_distance <= 32767);

	switch (BC_TYPE(source_bytecode))
	{
		case BC_JUMP:
		{
			emitter->bytecode[source_position] = BC_X(BC_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BC_JUMP_IF_ZERO:
		case BC_JUMP_IF_NONZERO:
		{
			emitter->bytecode[source_position] = BC_XYZ(BC_TYPE(source_bytecode), relative_distance, BC_ARGY(source_bytecode), BC_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static Ir new_label(BcEmitter *emitter)
{
	Ir label = arena_push_zero(emitter->arena, sizeof(*label), _Alignof(IrNode));
	label->kind = IR_LABEL;
	return label;
}

static void record_jump_patch(BcEmitter *emitter, u32 bytecode_position, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);
	BcJumpPatch *patch = arena_push_zero(emitter->arena, sizeof(*patch), _Alignof(BcJumpPatch));
	patch->next = emitter->jump_patches;
	patch->bytecode_position = bytecode_position;
	patch->label = label;
	emitter->jump_patches = patch;
}

static void patch_jumps(BcEmitter *emitter)
{
	for (BcJumpPatch *patch = emitter->jump_patches; patch; patch = patch->next)
	{
		Ir label = patch->label;
		ASSERT(label->ir_label.defined);
		patch_jump(emitter, patch->bytecode_position, label->ir_label.position);
	}
}

static u32 emit_bytecode(BcEmitter *emitter, SourceSite site, Bytecode byte)
{
	ASSERT(emitter->bytecode_count < 0xffffffffu);
	emitter->bytecode = grow_emitter_array(emitter->arena, emitter->bytecode, emitter->bytecode_count, &emitter->bytecode_capacity, emitter->bytecode_count + 1, sizeof(*emitter->bytecode));
	u32 position = emitter->bytecode_count++;
	emitter->bytecode[position] = byte;
	record_source_map(emitter, site, position);
	return position;
}

// Bytecode instructions

static u32 emit_bytecode_x(BcEmitter *emitter, SourceSite site, BcType type, i32 x)
{
	return emit_bytecode(emitter, site, BC_X(type, x));
}

static u32 emit_bytecode_xy(BcEmitter *emitter, SourceSite site, BcType type, i32 x, i32 y)
{
	return emit_bytecode(emitter, site, BC_XY(type, x, y));
}

static u32 emit_bytecode_xyz(BcEmitter *emitter, SourceSite site, BcType type, i32 x, i32 y, i32 z)
{
	return emit_bytecode(emitter, site, BC_XYZ(type, x, y, z));
}

static u32 emit_jump(BcEmitter *emitter, SourceSite site)
{
	return emit_bytecode_x(emitter, site, BC_JUMP, 0);
}

static u32 emit_conditional_jump(BcEmitter *emitter, SourceSite site, BcType type, i32 predicate)
{
	ASSERT(type == BC_JUMP_IF_ZERO || type == BC_JUMP_IF_NONZERO);
	ASSERT(slot_is_valid(predicate));
	return emit_bytecode_xy(emitter, site, type, 0, predicate);
}

static void define_label(BcEmitter *emitter, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);
	ASSERT(!label->ir_label.defined);
	label->ir_label.defined = true;
	label->ir_label.position = emitter->bytecode_count;
}

static void jump_to_label(BcEmitter *emitter, SourceSite site, Ir label)
{
	u32 jump = emit_jump(emitter, site);
	record_jump_patch(emitter, jump, label);
}

static u32 emit_integer(BcEmitter *emitter, SourceSite site, i32 destination, i64 integer)
{
	ASSERT(slot_is_valid(destination));
	u32 index = add_const(emitter, int_value(integer));
	return emit_bytecode_xy(emitter, site, BC_LOAD_CONST, destination, index);
}

static u32 emit_number(BcEmitter *emitter, SourceSite site, i32 destination, f64 number)
{
	ASSERT(slot_is_valid(destination));
	u32 index = add_const(emitter, num_value(number));
	return emit_bytecode_xy(emitter, site, BC_LOAD_CONST, destination, index);
}

static u32 emit_string(BcEmitter *emitter, SourceSite site, i32 destination, Atom *atom)
{
	ASSERT(slot_is_valid(destination));
	elf_String *string = elf_string_from_data_size(emitter->target, atom_data(atom), atom_size(atom));
	u32 index = add_const(emitter, str_value(string));
	return emit_bytecode_xy(emitter, site, BC_LOAD_CONST, destination, index);
}

static u32 emit_nil(BcEmitter *emitter, SourceSite site, i32 destination)
{
	ASSERT(slot_is_valid(destination));
	return emit_bytecode_xy(emitter, site, BC_LOAD_NIL, destination, 0);
}

static u32 emit_move(BcEmitter *emitter, SourceSite site, i32 destination, i32 source)
{
	ASSERT(slot_is_valid(destination));
	ASSERT(slot_is_valid(source));
	return emit_bytecode_xy(emitter, site, BC_MOVE, destination, source);
}

static u32 emit_return(BcEmitter *emitter, SourceSite site, i32 output, u32 output_count)
{
	ASSERT(!output_count || slot_is_valid(output));
	i32 slot = output_count ? output : 0;
	return emit_bytecode_xy(emitter, site, BC_RETURN, slot, output_count);
}

static Ir require_ir_kind(Ir ir, IrKind kind)
{
	ASSERT(ir);
	ASSERT(ir->kind == kind);
	return ir;
}

// Stack slots
static u32 ir_stack_top(BcEmitter *emitter)
{
	return emitter->stack_top;
}

static void ir_set_stack_top(BcEmitter *emitter, u32 stack_top)
{
	ASSERT(slot_is_valid(stack_top));
	emitter->stack_top = (u32)stack_top;
}

static u32 ir_stack_alloc(BcEmitter *emitter)
{
	u32 stack_top = emitter->stack_top;
	emitter->stack_top += 1;
	if (emitter->stack_size < emitter->stack_top) {
		emitter->stack_size = emitter->stack_top;
	}
	return stack_top;
}

static u32 ensure_slot(BcEmitter *emitter, u32 slot)
{
	if (!slot_is_valid(slot)) {
		slot = ir_stack_alloc(emitter);
	}
	return slot;
}

// Expressions and calls

static i32 emit_expr_to_slot(BcEmitter *emitter, Ir expr)
{
	i32 slot = NO_SLOT;
	if (expr->kind == IR_LOAD) {
		Ir local = require_ir_kind(expr->ir_load.local, IR_LOCAL);
		slot = local->ir_local.slot;
	}
	else {
		slot = emit_expr(emitter, expr, NO_SLOT, 1);
	}

	ASSERT(slot_is_valid(slot));
	return slot;
}

static i32 emit_table_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_bytecode_x(emitter, expr->site, BC_NEW_TABLE, out);
	return out;
}

static i32 emit_array_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1)
	{
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_bytecode_x(emitter, expr->site, BC_NEW_ARRAY, out);
	return out;
}

static i32 finish_call_result(BcEmitter *emitter, Ir ir, i32 call_slot, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	if (!slot_is_valid(out))
	{
		out = ir_stack_alloc(emitter);
		for (u32 i = 1; i < nout; ++i) {
			ir_stack_alloc(emitter);
		}
	}

	if (out != call_slot)
	{
		for (u32 i = 0; i < nout; ++i)
		{
			i32 destination = out + (i32)i;
			i32 source = call_slot + (i32)i;
			emit_move(emitter, ir->site, destination, source);
		}
	}

	return out;
}

static u32 emit_dynamic_call(BcEmitter *emitter, Ir ir, u32 dest, u32 ndest)
{
	Ir expr = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;

	u32 rets = ir_stack_top(emitter);
	u32 slot, call = NO_SLOT;
	{
		if (expr->kind == IR_META_FIELD)
		{
			require_ir_kind(expr->ir_binary.y, IR_ATOM);
			call = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);
			emit_bytecode_xyz(emitter, expr->site, BC_GET_META_FIELD, call, slot, call);
			nargs ++;
		}
		else if (expr->kind == IR_FIELD)
		{
			require_ir_kind(expr->ir_binary.y, IR_ATOM);
			call = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);
			emit_bytecode_xyz(emitter, expr->site, BC_GET_FIELD, call, slot, call);
			nargs ++;
		}
		else
		{
			call = emit_expr(emitter, expr, NO_SLOT, 1);
		}

		for (u32 i = 0; i < args.count; ++ i) {
			emit_expr(emitter, args.items[i], NO_SLOT, 1);
		}

		/* the argument slots become the destination slots, allocate padding */
		while (emitter->stack_top < rets + MAX(nargs + 1, ndest)) {
			ir_stack_alloc(emitter);
		}
	}
	ir_set_stack_top(emitter, rets);

	ASSERT(call == rets);
	emit_bytecode_xyz(emitter, expr->site, BC_CALL_DYNAMIC, rets, nargs, ndest);
	return finish_call_result(emitter, ir, rets, dest, ndest);
}

static i32 emit_binary_expr(BcEmitter *emitter, Ir expr, BcType bytecode_type, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 slot_checkpoint = ir_stack_top(emitter);
	i32 left_slot = emit_expr_to_slot(emitter, expr->ir_binary.x);
	i32 right_slot = emit_expr_to_slot(emitter, expr->ir_binary.y);
	ir_set_stack_top(emitter, slot_checkpoint);

	out = ensure_slot(emitter, out);
	emit_bytecode_xyz(emitter, expr->site, bytecode_type, out, left_slot, right_slot);
	return out;
}

static i32 emit_unary_expr(BcEmitter *emitter, Ir expr, BcType bytecode_type, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 slot_checkpoint = ir_stack_top(emitter);
	i32 value_slot = emit_expr_to_slot(emitter, expr->ir_unary);
	ir_set_stack_top(emitter, slot_checkpoint);

	out = ensure_slot(emitter, out);
	emit_bytecode_xy(emitter, expr->site, bytecode_type, out, value_slot);
	return out;
}

static i32 emit_nil_or_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	i32 slot_checkpoint;
	i32 temp_slot;
	Ir done_label;

	if (nout < 1) {
		return out;
	}

	out = emit_expr(emitter, expr->ir_binary.x, out, 1);
	slot_checkpoint = ir_stack_top(emitter);
	temp_slot = ir_stack_alloc(emitter);
	emit_nil(emitter, expr->site, temp_slot);
	emit_bytecode_xyz(emitter, expr->site, BC_EQ, temp_slot, temp_slot, out);
	done_label = new_label(emitter);
	jump_if_false_slot_to_label(emitter, expr->site, temp_slot, done_label);
	ir_set_stack_top(emitter, slot_checkpoint);
	emit_expr(emitter, expr->ir_binary.y, out, 1);
	define_label(emitter, done_label);
	return out;
}

static i32 emit_logical_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	Ir false_label;
	Ir done_label;

	if (nout < 1) {
		return out;
	}

	false_label = new_label(emitter);
	done_label = new_label(emitter);

	jump_if_expr_false(emitter, expr, false_label);
	out = ensure_slot(emitter, out);
	emit_integer(emitter, expr->site, out, 1);
	jump_to_label(emitter, expr->site, done_label);
	define_label(emitter, false_label);
	emit_integer(emitter, expr->site, out, 0);
	define_label(emitter, done_label);

	return out;
}

static void jump_if_true_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, Ir label)
{
	u32 jump = emit_conditional_jump(emitter, site, BC_JUMP_IF_NONZERO, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, Ir label)
{
	u32 jump = emit_conditional_jump(emitter, site, BC_JUMP_IF_ZERO, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_expr_false(BcEmitter *emitter, Ir expr, Ir false_label)
{
	switch (expr->kind)
	{
		// 'a' && 'b'
		case IR_AND:
		{
			jump_if_expr_false(emitter, expr->ir_binary.x, false_label);
			jump_if_expr_false(emitter, expr->ir_binary.y, false_label);
		}
		break;
		// 'a' || 'b'
		case IR_OR:
		{
			Ir fallthrough_label = new_label(emitter);
			jump_if_expr_true(emitter, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_false(emitter, expr->ir_binary.y, false_label);
			define_label(emitter, fallthrough_label);
		}
		break;
		default:
		{
			i32 slot_checkpoint = ir_stack_top(emitter);
			i32 pred_slot = emit_expr_to_slot(emitter, expr);
			jump_if_false_slot_to_label(emitter, expr->site, pred_slot, false_label);
			ir_set_stack_top(emitter, slot_checkpoint);
		}
		break;
	}
}

static void jump_if_expr_true(BcEmitter *emitter, Ir expr, Ir true_label)
{
	switch (expr->kind)
	{
		// 'a' && 'b'
		case IR_AND:
		{
			Ir fallthrough_label = new_label(emitter);
			jump_if_expr_false(emitter, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_true(emitter, expr->ir_binary.y, true_label);
			define_label(emitter, fallthrough_label);
		}
		break;
		// 'a' || 'b'
		case IR_OR:
		{
			jump_if_expr_true(emitter, expr->ir_binary.x, true_label);
			jump_if_expr_true(emitter, expr->ir_binary.y, true_label);
		}
		break;
		default:
		{
			i32 slot_checkpoint = ir_stack_top(emitter);
			i32 pred_slot = emit_expr_to_slot(emitter, expr);
			jump_if_true_slot_to_label(emitter, expr->site, pred_slot, true_label);
			ir_set_stack_top(emitter, slot_checkpoint);
		}
		break;
	}
}

static i32 emit_expr_block(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	IrArray stats = expr->ir_expr_block.stats;
	Ir value = expr->ir_expr_block.value;

	if (nout < 1)
	{
		i32 slot_checkpoint = ir_stack_top(emitter);
		for (u32 i = 0; i < stats.count; ++i) {
			emit_stat(emitter, stats.items[i]);
		}
		ir_set_stack_top(emitter, slot_checkpoint);
		return out;
	}

	out = ensure_slot(emitter, out);
	i32 slot_checkpoint = ir_stack_top(emitter);
	for (u32 i = 0; i < stats.count; ++i) {
		emit_stat(emitter, stats.items[i]);
	}
	i32 final_slot = emit_expr(emitter, value, out, 1);
	ASSERT(final_slot == out);
	ir_set_stack_top(emitter, slot_checkpoint);
	return out;
}

// Todo, remove nout
static i32 emit_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	ASSERT(expr);
	ASSERT(nout <= 1 || (expr->kind == IR_MULTI_LOCAL || expr->kind == IR_CALL) || !"Dayan - internal error, only call expressions and multi-locals are allowed to have more than one output");

	SourceSite site = expr->site;
	switch (expr->kind)
	{
		case IR_ERROR:
		{
			return out;
		}

		case IR_TUPLE:
		{
			ASSERT(out == NO_SLOT || !"Dayan - internal error, when lowering a tuple, you must not specify the resulting slot, otherwise we'd have to emit code for relocation, which is not the intended usage");
			ASSERT(expr->ir_tuple.items || !expr->ir_tuple.count);
			ASSERT(nout == 0 || nout == 1);

			i32 first_slot = ir_stack_top(emitter);
			for (u32 i = 0; i < expr->ir_tuple.count; ++ i) {
				ASSERT(expr->ir_tuple.items[i] != 0);
				i32 expr_slot = emit_expr(emitter, expr->ir_tuple.items[i], NO_SLOT, 1);
				ASSERT(first_slot + i == expr_slot);
			}
			out = first_slot;
		}
		break;

		case IR_FUNCTION_REF:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			IrFunction *function = expr->func;
			u32 slot = add_func(emitter, function);
			if (function->ncaps == 0)
			{
				emit_bytecode_xy(emitter, site, BC_LOAD_CONST, out, slot);
			}
			else
			{
				i32 slot_checkpoint = ir_stack_top(emitter);
				i32 first_capture_slot = slot_checkpoint;
				i32 prev_capture_slot = 0;
				for (u32 i = 0; i < function->ncaps; ++i)
				{
					i32 next_capture_slot = emit_expr(emitter, function->caps[i].target, NO_SLOT, 1);
					ASSERT(slot_is_valid(next_capture_slot));
					ASSERT(first_capture_slot + prev_capture_slot == next_capture_slot);
					prev_capture_slot ++;
				}

				emit_bytecode_xy(emitter, site, BC_NEW_CLOSURE, first_capture_slot, slot);
				if (first_capture_slot != out) {
					emit_move(emitter, site, out, first_capture_slot);
				}
				ir_set_stack_top(emitter, slot_checkpoint);
			}
		}
		break;

		case IR_CAPTURE_REF:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_bytecode_xy(emitter, site, BC_LOAD_CAPTURE, out, expr->capture);
		}
		break;

		case IR_RECURSE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_move(emitter, site, out, 0);
		}
		break;

		case IR_NATIVE_FUNCTION:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			ASSERT(expr->native_function_index < emitter->target->native_function_count);
			elf_Function function = emitter->target->native_functions[expr->native_function_index];
			u32 index = add_const(emitter, value_from_function(function));
			emit_bytecode_xy(emitter, site, BC_LOAD_CONST, out, index);
		}
		break;

		case IR_LOAD:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			Ir local = expr->ir_load.local;
			require_ir_kind(local, IR_LOCAL);
			ASSERT(slot_is_valid(local->ir_local.slot));

			emit_move(emitter, expr->site, out, local->ir_local.slot);
		}
		break;

		case IR_GET_MEM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			Ir target = expr->ir_unary;
			require_ir_kind(target, IR_LOAD);

			Ir local = target->ir_load.local;
			require_ir_kind(local, IR_LOCAL);
			ASSERT(slot_is_valid(local->ir_local.slot));

			emit_integer(emitter, expr->site, out, local->ir_local.slot);
		}
		break;

		case IR_INTEGER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_integer(emitter, expr->site, out, expr->ir_int);
		}
		break;

		case IR_NUMBER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_number(emitter, expr->site, out, expr->ir_num);
		}
		break;

		case IR_ATOM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_string(emitter, expr->site, out, expr->atom);
		}
		break;

		case IR_NIL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_nil(emitter, expr->site, out);
		}
		break;

		case IR_CALL:
		{
			out = emit_dynamic_call(emitter, expr, out, nout);
		}
		break;

		case IR_NEW_TAB:
		{
			out = emit_table_expr(emitter, expr, out, nout);
		}
		break;

		case IR_NEW_ARR:
		{
			out = emit_array_expr(emitter, expr, out, nout);
		}
		break;

		case IR_TAB_REF:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			u32 index = add_const(emitter, value_from_table(expr->ir_tab));
			emit_bytecode_xy(emitter, site, BC_LOAD_CONST, out, index);
		}
		break;

		case IR_ARR_REF:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			u32 index = add_const(emitter, value_from_array(expr->ir_arr));
			emit_bytecode_xy(emitter, site, BC_LOAD_CONST, out, index);
		}
		break;

		case IR_EXPR_BLOCK:
		{
			out = emit_expr_block(emitter, expr, out, nout);
		}
		break;

		case IR_MEASURE:
		{
			out = emit_unary_expr(emitter, expr, BC_MEASURE, out, nout);
		}
		break;

		case IR_BITWISE_NOT:
		{
			out = emit_unary_expr(emitter, expr, BC_BITWISE_NOT, out, nout);
		}
		break;

		case IR_NIL_OR:
		{
			out = emit_nil_or_expr(emitter, expr, out, nout);
		}
		break;

		case IR_AND:
		case IR_OR:
		{
			out = emit_logical_expr(emitter, expr, out, nout);
		}
		break;

		case IR_EQ:           return emit_binary_expr(emitter, expr, BC_EQ, out, nout);
		case IR_NOT_EQ:       return emit_binary_expr(emitter, expr, BC_NOT_EQ, out, nout);
		case IR_LESS_THAN_EQ: return emit_binary_expr(emitter, expr, BC_LESS_THAN_EQ, out, nout);
		case IR_LESS_THAN:    return emit_binary_expr(emitter, expr, BC_LESS_THAN, out, nout);
		case IR_DIV:          return emit_binary_expr(emitter, expr, BC_DIV, out, nout);
		case IR_MUL:          return emit_binary_expr(emitter, expr, BC_MUL, out, nout);
		case IR_MOD:          return emit_binary_expr(emitter, expr, BC_MOD, out, nout);
		case IR_SUB:          return emit_binary_expr(emitter, expr, BC_SUB, out, nout);
		case IR_ADD:          return emit_binary_expr(emitter, expr, BC_ADD, out, nout);
		case IR_POW:          return emit_binary_expr(emitter, expr, BC_POW, out, nout);
		case IR_SHIFT_LEFT:   return emit_binary_expr(emitter, expr, BC_SHIFT_LEFT, out, nout);
		case IR_SHIFT_RIGHT:  return emit_binary_expr(emitter, expr, BC_SHIFT_RIGHT, out, nout);
		case IR_INDEX:        return emit_binary_expr(emitter, expr, BC_GET_INDEX, out, nout);
		case IR_FIELD:        return emit_binary_expr(emitter, expr, BC_GET_FIELD, out, nout);
		case IR_META_FIELD:   return emit_binary_expr(emitter, expr, BC_GET_META_FIELD, out, nout);
		case IR_BITWISE_XOR:  return emit_binary_expr(emitter, expr, BC_BITWISE_XOR, out, nout);
		case IR_BITWISE_AND:  return emit_binary_expr(emitter, expr, BC_BITWISE_AND, out, nout);
		case IR_BITWISE_OR:   return emit_binary_expr(emitter, expr, BC_BITWISE_OR, out, nout);

		default:
		{
			ASSERT(!"invalid expression IR");
		}
		break;
	}

	return out;
}

// Statements

static void emit_store(BcEmitter *emitter, SourceSite site, Ir destination, Ir expr)
{
	i32 slot_checkpoint = ir_stack_top(emitter);
	{
		switch (destination->kind)
		{
			case IR_LOCAL:
			{
				i32 dest_slot = destination->ir_local.slot;
				ASSERT(slot_is_valid(dest_slot));

				i32 final_slot = emit_expr(emitter, expr, dest_slot, 1);
				ASSERT(final_slot == dest_slot);
			}
			break;

			case IR_FIELD:
			{
				i32 value_slot = emit_expr_to_slot(emitter, expr);
				i32 object_slot = emit_expr_to_slot(emitter, destination->ir_binary.x);
				i32 field_slot = emit_expr_to_slot(emitter, destination->ir_binary.y);

				emit_bytecode_xyz(emitter, site, BC_SET_FIELD, object_slot, field_slot, value_slot);
			}
			break;

			case IR_INDEX:
			{
				i32 value_slot = emit_expr_to_slot(emitter, expr);
				i32 array_slot = emit_expr_to_slot(emitter, destination->ir_binary.x);
				i32 index_slot = emit_expr_to_slot(emitter, destination->ir_binary.y);
				emit_bytecode_xyz(emitter, site, BC_SET_INDEX, array_slot, index_slot, value_slot);
			}
			break;

			default:
			{
				ASSERT(!"invalid l-value IR");
			}
			break;
		}
	}
	ir_set_stack_top(emitter, slot_checkpoint);
}

static void emit_stat(BcEmitter *emitter, Ir stat)
{
	switch (stat->kind)
	{
		case IR_ERROR:
		{
			return;
		}

		case IR_LOCAL:
		{
			i32 slot = stat->ir_local.slot;
			ASSERT(!slot_is_valid(slot));
			ASSERT(!stat->ir_local.expr || stat->ir_local.expr->kind != IR_TUPLE || stat->ir_local.expr->ir_tuple.count == 1);
			if (stat->ir_local.expr)
			{
				slot = emit_expr(emitter, stat->ir_local.expr, NO_SLOT, 1);
				ASSERT(slot_is_valid(slot));
			}
			else
			{
				slot = ir_stack_alloc(emitter);
			}

			stat->ir_local.slot = slot;
		}
		break;

		case IR_MULTI_LOCAL:
		{
			IrArray locals = stat->ir_multi_local.locals;
			ASSERT(locals.count > 1);
			i32 first = emit_expr(emitter, stat->ir_multi_local.expr, NO_SLOT, locals.count);
			ASSERT(slot_is_valid(first));
			for (u32 i = 0; i < locals.count; ++i)
			{
				Ir local = locals.items[i];
				ASSERT(local && local->kind == IR_LOCAL);
				ASSERT(!slot_is_valid(local->ir_local.slot));
				local->ir_local.slot = first + (i32)i;
			}
		}
		break;

		case IR_STORE:
		{
			emit_store(emitter, stat->site, stat->ir_binary.x, stat->ir_binary.y);
		}
		break;

		case IR_ARRAY_ADD:
		{
			i32 slot_checkpoint = ir_stack_top(emitter);
			i32 array_slot = emit_expr_to_slot(emitter, stat->ir_binary.x);
			i32 value_slot = emit_expr_to_slot(emitter, stat->ir_binary.y);
			emit_bytecode_xy(emitter, stat->site, BC_ARRAY_APPEND, array_slot, value_slot);
			ir_set_stack_top(emitter, slot_checkpoint);
		}
		break;

		case IR_LABEL:
		{
			define_label(emitter, stat);
		}
		break;

		case IR_JUMP:
		{
			jump_to_label(emitter, stat->site, stat->ir_jump.label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			jump_if_expr_false(emitter, stat->ir_jump_if_false.pred, stat->ir_jump_if_false.label);
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = stat->ir_block.stats;

			i32 slot_checkpoint = ir_stack_top(emitter);
			{
				for (u32 i = 0; i < stats.count; ++i)
				{
					emit_stat(emitter, stats.items[i]);
				}
			}
			ir_set_stack_top(emitter, slot_checkpoint);
		}
		break;

		case IR_IF:
		{
			Ir pred = stat->ir_if.pred;
			Ir true_clause = stat->ir_if.true_clause;
			Ir else_clause = stat->ir_if.else_clause;

			Ir else_label = new_label(emitter);
			jump_if_expr_false(emitter, pred, else_label);

			{
				i32 true_slots = ir_stack_top(emitter);
				emit_stat(emitter, true_clause);
				ir_set_stack_top(emitter, true_slots);
			}

			if (else_clause)
			{
				Ir end_label = new_label(emitter);
				jump_to_label(emitter, stat->site, end_label);

				i32 else_slots = ir_stack_top(emitter);
				define_label(emitter, else_label);
				ir_set_stack_top(emitter, else_slots);
				emit_stat(emitter, else_clause);
				ir_set_stack_top(emitter, else_slots);
				define_label(emitter, end_label);
			}
			else
			{
				define_label(emitter, else_label);
			}
		}
		break;

		case IR_RETURN:
		{
			Ir expr = stat->ir_return.expr;
			ASSERT(!expr || expr->kind == IR_TUPLE);
			if (expr)
			{
				i32 slot = emit_expr_to_slot(emitter, expr);
				u32 size = expr->ir_tuple.count;
				emit_return(emitter, stat->site, slot, size);
			}
			else
			{
				emit_return(emitter, stat->site, NO_SLOT, 0);
			}
		}
		break;

		default:
		{
			i32 slot_checkpoint = ir_stack_top(emitter);
			emit_expr(emitter, stat, NO_SLOT, 0);
			ir_set_stack_top(emitter, slot_checkpoint);
		}
		break;
	}
}

// Functions and module

static u64 prototype_append_region(u64 *allocation_size, u64 size, u64 alignment)
{
	ASSERT(alignment && !(alignment & (alignment - 1)));
	if (!size)
	{
		return 0;
	}
	u64 offset = (*allocation_size + alignment - 1) & ~(alignment - 1);
	ASSERT(offset <= UINT64_MAX - size);
	*allocation_size = offset + size;
	return offset;
}

static void install_function(BcEmitter *emitter, IrFunction *function)
{
	ASSERT(function->source);
	ASSERT(function->source->runtime_source);
	ASSERT(!function->proto);

	u64 allocation_size = sizeof(elf_Prototype);
	u64 constants_offset = prototype_append_region(&allocation_size, sizeof(elf_Value) * emitter->nconsts, _Alignof(elf_Value));
	u64 bytecode_offset = prototype_append_region(&allocation_size, sizeof(Bytecode) * emitter->bytecode_count, _Alignof(Bytecode));
	u64 source_map_offset = prototype_append_region(&allocation_size, sizeof(elf_SourceMapEntry) * emitter->source_map_count, _Alignof(elf_SourceMapEntry));
	ASSERT(allocation_size <= UINT32_MAX);
	elf_Prototype *prototype = elf_gc_alloc(emitter->target, ELF_OBJECT_PROTOTYPE, (u32)allocation_size);
	ASSERT(prototype);

	prototype->source = function->source->runtime_source;
	prototype->variadic = function->variadic;
	prototype->arity = function->params.count;
	prototype->captures = function->ncaps;
	prototype->stack_size = emitter->stack_size;
	prototype->nconsts = emitter->nconsts;
	prototype->bytecode_count = emitter->bytecode_count;
	prototype->source_map_count = emitter->source_map_count;
	if (prototype->nconsts)
	{
		prototype->consts = (elf_Value *)((u8 *)prototype + constants_offset);
		copy_memory(prototype->consts, emitter->consts, sizeof(*prototype->consts) * prototype->nconsts);
	}
	if (prototype->bytecode_count)
	{
		prototype->bytecode = (Bytecode *)((u8 *)prototype + bytecode_offset);
		copy_memory(prototype->bytecode, emitter->bytecode, sizeof(*prototype->bytecode) * prototype->bytecode_count);
	}
	if (prototype->source_map_count)
	{
		prototype->source_map = (elf_SourceMapEntry *)((u8 *)prototype + source_map_offset);
		copy_memory(prototype->source_map, emitter->source_map, sizeof(*prototype->source_map) * prototype->source_map_count);
	}

	function->proto = prototype;
}

static elf_Prototype *bc_emit_func(elf_State *target, IrModule *module, IrFunction *function)
{
	ASSERT(target);
	ASSERT(module);
	ASSERT(function);

	if (function->proto) {
		return function->proto;
	}

	u64 arena_checkpoint = arena_mark(module->arena);
	BcEmitter *emitter = arena_push_zero(module->arena, sizeof(*emitter), _Alignof(BcEmitter));
	emitter->target = target;
	emitter->arena = module->arena;
	emitter->bytecode_capacity = 1 << 13;
	emitter->bytecode = arena_push_zero(emitter->arena, sizeof(*emitter->bytecode) * emitter->bytecode_capacity, _Alignof(Bytecode));

	ASSERT(!function->ast || function->ast->function.params.count == function->params.count);
	emitter->stack_size = 1;
	emitter->stack_top = 1;
	for (u32 i = 0; i < function->params.count; ++i)
	{
		Ir param = function->params.items[i];
		ASSERT(param && param->kind == IR_LOCAL);
		ASSERT(!slot_is_valid(param->ir_local.slot));

		param->ir_local.slot = ir_stack_alloc(emitter);
		ASSERT(param->ir_local.slot == 1 + i);

		// TODO(RJ): these could be intrinsic to the prototype itself, could prob save us some instructions or something
		if (param->type_rule != TRULE_NONE) {
			emit_bytecode_xy(emitter, param->site, BC_CHECK_TYPE, param->ir_local.slot, param->type_rule);
		}
	}

	i32 slot_checkpoint = ir_stack_top(emitter);
	emit_stat(emitter, function->body);
	ir_set_stack_top(emitter, slot_checkpoint);
	if (emitter->bytecode_count == 0 || BC_TYPE(emitter->bytecode[emitter->bytecode_count - 1]) != BC_RETURN) {
		emit_return(emitter, function->body->site, NO_SLOT, 0);
	}
	patch_jumps(emitter);
	install_function(emitter, function);

	for (u32 i = 0; i < emitter->nrefs; ++i)
	{
		BcRef ref = emitter->refs[i];
		elf_Prototype *proto = bc_emit_func(target, module, ref.func);
		ASSERT(ref.slot < function->proto->nconsts);
		function->proto->consts[ref.slot] = proto_value(proto);
	}

	arena_restore(module->arena, arena_checkpoint);
	return function->proto;
}

static elf_Prototype *bc_emit_module(elf_State *target, IrModule *module)
{
	ASSERT(target);
	ASSERT(module);
	ASSERT(module->function_count);
	ASSERT(module->last_function);
	ASSERT(!module->last_function->next);

	u32 emitted_function_count = 0;
	for (IrFunction *function = module->first_function; function; function = function->next)
	{
		bc_emit_func(target, module, function);
		emitted_function_count += 1;
	}
	ASSERT(emitted_function_count == module->function_count);

	return module->first_function->proto;
}
