//
// See Copyright Notice In elf.h
//

#include <stdlib.h>

#include <dy.h>

#include "elf.h"
#include "elf_target.h"
#include "base.h"
#include "bytecode.h"
#include "atom_view.h"
#include "ir.h"
#include "bc_emitter.h"

#define BC_OPERAND_COUNT_MAX (1u << 15)
#define BC_LABEL_COUNT_MAX   4096
#define BC_LABEL_INVALID     ((u32)-1)

typedef struct
{
	b32 defined;
	i32 position;
}
BcLabel;

typedef struct
{
	u32 bytecode_position;
	u32 label;
}
BcJumpPatch;

typedef struct BcEmitter BcEmitter;
struct BcEmitter
{
	elf_State          *target;
	Arena              *arena;

	// Reused scratch buffers. Each emitted function is copied into its runtime prototype.
	Bytecode           *bytecode;
	u32                 bytecode_capacity;
	u32                 bytecode_count;

	u32                 function_count;
	u32                 reserved_function_count;
	u32                 emitted_function_count;

	i64                *integer_constants;
	u32                 integer_constant_capacity;
	u32                 integer_constant_count;

	f64                *number_constants;
	u32                 number_constant_capacity;
	u32                 number_constant_count;

	BcSourceMapEntry   *source_map;
	u32                 source_map_capacity;
	u32                 source_map_count;

	Atom              **string_atoms;
	u32                 string_atom_capacity;
	u32                 string_atom_count;

	u32                 stack_size;
	u32                 stack_top;
	BcLabel             labels[BC_LABEL_COUNT_MAX];
	u32                 label_count;
	BcJumpPatch         jump_patches[BC_LABEL_COUNT_MAX];
	u32                 jump_patch_count;
};

static inline b32 slot_is_valid(i32 slot)
{
	return slot >= 0;
}

static i32 emit_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout);
static void emit_stat(BcEmitter *emitter, Ir stat);
static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label);
static void jump_if_expr_true(BcEmitter *emitter, Ir expr, u32 true_label);
static void jump_if_expr_false(BcEmitter *emitter, Ir expr, u32 false_label);
static u32 reserve_prototype_index(BcEmitter *emitter, IrFunction *function);

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

static b32 source_site_equal(SourceSite left, SourceSite right)
{
	return left.source == right.source &&
	left.offset == right.offset &&
	left.size == right.size &&
	left.line_offset == right.line_offset &&
	left.line_index == right.line_index;
}

static void record_source_map(BcEmitter *emitter, SourceSite site, u32 byte_position)
{
	if (emitter->source_map_count > 0) {
		BcSourceMapEntry *entry = emitter->source_map + emitter->source_map_count - 1;
		if (entry->byte_end == byte_position && source_site_equal(entry->site, site)) {
			entry->byte_end = byte_position + 1;
			return;
		}
	}

	emitter->source_map = grow_emitter_array(emitter->arena, emitter->source_map, emitter->source_map_count, &emitter->source_map_capacity, emitter->source_map_count + 1, sizeof(*emitter->source_map));
	BcSourceMapEntry *entry = emitter->source_map + emitter->source_map_count++;
	entry->byte_start = byte_position;
	entry->byte_end = byte_position + 1;
	entry->site = site;
}

static u32 add_integer_constant(BcEmitter *emitter, i64 value)
{
	if (emitter->integer_constant_count >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	emitter->integer_constants = grow_emitter_array(emitter->arena, emitter->integer_constants, emitter->integer_constant_count, &emitter->integer_constant_capacity, emitter->integer_constant_count + 1, sizeof(*emitter->integer_constants));
	u32 index = emitter->integer_constant_count++;
	emitter->integer_constants[index] = value;
	return index;
}

static u32 add_number_constant(BcEmitter *emitter, f64 value)
{
	if (emitter->number_constant_count >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	emitter->number_constants = grow_emitter_array(emitter->arena, emitter->number_constants, emitter->number_constant_count, &emitter->number_constant_capacity, emitter->number_constant_count + 1, sizeof(*emitter->number_constants));
	u32 index = emitter->number_constant_count++;
	emitter->number_constants[index] = value;
	return index;
}

static u32 add_string_atom(BcEmitter *emitter, Atom *atom)
{
	for (u32 i = 0; i < emitter->string_atom_count; ++i) {
		if (emitter->string_atoms[i] == atom) {
			return i;
		}
	}

	if (emitter->string_atom_count >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	emitter->string_atoms = grow_emitter_array(emitter->arena, emitter->string_atoms, emitter->string_atom_count, &emitter->string_atom_capacity, emitter->string_atom_count + 1, sizeof(*emitter->string_atoms));
	u32 index = emitter->string_atom_count++;
	emitter->string_atoms[index] = atom;
	return index;
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
			emitter->bytecode[source_position] = BC_XXX(BC_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BC_JZ:
		case BC_JNZ:
		{
			emitter->bytecode[source_position] = BC_XYZ(BC_TYPE(source_bytecode), relative_distance, BC_ARGY(source_bytecode), BC_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static BcLabel *get_label(BcEmitter *emitter, u32 label)
{
	ASSERT(label < ARRAY_COUNT(emitter->labels));
	return &emitter->labels[label];
}

static u32 new_label(BcEmitter *emitter)
{
	ASSERT(emitter->label_count < ARRAY_COUNT(emitter->labels));
	return emitter->label_count ++;
}

static u32 label_for_ir(BcEmitter *emitter, Ir ir_label)
{
	ASSERT(ir_label);
	ASSERT(ir_label->kind == IR_LABEL);

	if (!ir_label->ir_label.has_bytecode_label)
	{
		ir_label->ir_label.bytecode_label = new_label(emitter);
		ir_label->ir_label.has_bytecode_label = true;
	}

	return ir_label->ir_label.bytecode_label;
}

static void record_jump_patch(BcEmitter *emitter, u32 bytecode_position, u32 label)
{
	ASSERT(label != BC_LABEL_INVALID);
	ASSERT(emitter->jump_patch_count < ARRAY_COUNT(emitter->jump_patches));

	BcJumpPatch *patch = emitter->jump_patches + emitter->jump_patch_count++;
	patch->bytecode_position = bytecode_position;
	patch->label = label;
}

static void patch_jumps(BcEmitter *emitter)
{
	for (u32 i = 0; i < emitter->jump_patch_count; ++i)
	{
		BcJumpPatch patch = emitter->jump_patches[i];
		BcLabel *label = get_label(emitter, patch.label);
		ASSERT(label->defined);
		patch_jump(emitter, patch.bytecode_position, (u32)label->position);
	}
	emitter->jump_patch_count = 0;
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
	return emit_bytecode(emitter, site, BC_XXX(type, x));
}

static u32 emit_bytecode_xy(BcEmitter *emitter, SourceSite site, BcType type, i32 x, i32 y)
{
	return emit_bytecode(emitter, site, BC_XYY(type, x, y));
}

static u32 emit_bytecode_xyz(BcEmitter *emitter, SourceSite site, BcType type, i32 x, i32 y, i32 z)
{
	return emit_bytecode(emitter, site, BC_XYZ(type, x, y, z));
}

static u32 emit_bytecode_xyzw(BcEmitter *emitter, SourceSite site, BcType type, i32 x, i32 y, i32 z, i32 w)
{
	return emit_bytecode(emitter, site, BC_XYZW(type, x, y, z, w));
}

static u32 emit_jump(BcEmitter *emitter, SourceSite site)
{
	return emit_bytecode_x(emitter, site, BC_JUMP, 0);
}

static u32 emit_conditional_jump(BcEmitter *emitter, SourceSite site, BcType type, i32 predicate)
{
	ASSERT(type == BC_JZ || type == BC_JNZ);
	ASSERT(slot_is_valid(predicate));
	return emit_bytecode_xy(emitter, site, type, 0, predicate);
}

static void define_label(BcEmitter *emitter, u32 label)
{
	BcLabel *target = get_label(emitter, label);
	ASSERT(!target->defined);
	target->defined = true;
	target->position = emitter->bytecode_count;
}

static void jump_to_label(BcEmitter *emitter, SourceSite site, u32 label)
{
	u32 jump = emit_jump(emitter, site);
	record_jump_patch(emitter, jump, label);
}

static u32 emit_integer(BcEmitter *emitter, SourceSite site, i32 destination, i64 integer)
{
	ASSERT(slot_is_valid(destination));
	u32 index = add_integer_constant(emitter, integer);
	return emit_bytecode_xy(emitter, site, BC_LOADKINT, destination, index);
}

static u32 emit_number(BcEmitter *emitter, SourceSite site, i32 destination, f64 number)
{
	ASSERT(slot_is_valid(destination));
	u32 index = add_number_constant(emitter, number);
	return emit_bytecode_xy(emitter, site, BC_LOADKNUM, destination, index);
}

static u32 emit_string(BcEmitter *emitter, SourceSite site, i32 destination, Atom *atom)
{
	ASSERT(slot_is_valid(destination));
	u32 index = add_string_atom(emitter, atom);
	return emit_bytecode_xy(emitter, site, BC_LOADKSTRING, destination, index);
}

static u32 emit_nil(BcEmitter *emitter, SourceSite site, i32 destination)
{
	ASSERT(slot_is_valid(destination));
	return emit_bytecode_xy(emitter, site, BC_LOADNIL, destination, 0);
}

static u32 emit_get_global(BcEmitter *emitter, SourceSite site, i32 destination, Atom *name)
{
	ASSERT(slot_is_valid(destination));
	elf_StrSlice symbol = {(char *)atom_data(name), atom_size(name)};
	u32 global = elf_ensure_global_symbol(emitter->target, symbol);
	if (global >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	return emit_bytecode_xy(emitter, site, BC_GETGLOBAL, destination, global);
}

static u32 emit_set_global(BcEmitter *emitter, SourceSite site, Atom *name, i32 source)
{
	ASSERT(slot_is_valid(source));
	elf_StrSlice symbol = {(char *)atom_data(name), atom_size(name)};
	u32 global = elf_ensure_global_symbol(emitter->target, symbol);
	if (global >= BC_OPERAND_COUNT_MAX) {
		abort();
	}
	return emit_bytecode_xy(emitter, site, BC_SETGLOBAL, global, source);
}

static u32 emit_move(BcEmitter *emitter, SourceSite site, i32 destination, i32 source)
{
	ASSERT(slot_is_valid(destination));
	ASSERT(slot_is_valid(source));
	return emit_bytecode_xy(emitter, site, BC_RELOAD, destination, source);
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
static i32 save_slots(BcEmitter *emitter)
{
	return (i32)emitter->stack_top;
}

static void restore_slots(BcEmitter *emitter, i32 stack_top)
{
	ASSERT(slot_is_valid(stack_top));
	emitter->stack_top = (u32)stack_top;
}

static i32 push_slot(BcEmitter *emitter)
{
	i32 stack_top = (i32)emitter->stack_top;
	emitter->stack_top += 1;
	if (emitter->stack_size < emitter->stack_top) {
		emitter->stack_size = emitter->stack_top;
	}
	return stack_top;
}

static i32 ensure_slot(BcEmitter *emitter, i32 slot)
{
	if (!slot_is_valid(slot)) {
		slot = push_slot(emitter);
	}
	return slot;
}

// Expressions and calls

static i32 emit_expr_to_slot(BcEmitter *emitter, Ir expr)
{
	i32 slot = NO_SLOT;
	if (expr->kind == IR_LOAD) {
		Ir local = expr->ir_load.local;
		require_ir_kind(local, IR_LOCAL);
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
	emit_bytecode_x(emitter, expr->site, BC_TABLE, out);
	return out;
}

static i32 emit_array_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1)
	{
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_bytecode_x(emitter, expr->site, BC_ARRAY, out);
	return out;
}

static i32 finish_call_result(BcEmitter *emitter, Ir ir, i32 call_slot, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	if (!slot_is_valid(out))
	{
		out = push_slot(emitter);
		for (u32 i = 1; i < nout; ++i) {
			push_slot(emitter);
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

static b32 can_reuse_local_callee(Ir expr, IrArray args, i32 *callee_slot)
{
	if (expr->kind != IR_LOAD) {
		return false;
	}

	for (u32 i = 0; i < args.count; ++i)
	{
		switch (args.items[i]->kind)
		{
			case IR_NUMBER:
			case IR_INTEGER:
			case IR_ATOM:
			case IR_NIL:
			case IR_LOAD:
			case IR_GLOBAL:
			case IR_CAPTURE:
			{
			}
			break;

			default:
			{
				return false;
			}
		}
	}

	Ir local = require_ir_kind(expr->ir_load.local, IR_LOCAL);
	ASSERT(slot_is_valid(local->ir_local.slot));
	*callee_slot = local->ir_local.slot;
	return true;
}

static i32 emit_dynamic_call(BcEmitter *emitter, Ir ir, i32 out, u32 nout)
{
	ASSERT(ir->ir_call.type == IR_CALL_DYNAMIC);
	Ir expr = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;

	i32 slot_checkpoint = save_slots(emitter);
	i32 callee_slot = NO_SLOT;
	i32 call_slot = slot_checkpoint;
	{
		i32 this_slot;
		if (expr->kind == IR_META_FIELD)
		{
			require_ir_kind(expr->ir_binary.y, IR_ATOM);

			callee_slot = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);

			emit_bytecode_xyz(emitter, expr->site, BC_GETMETAFIELD, callee_slot, this_slot, callee_slot);
		}
		else if (expr->kind == IR_FIELD)
		{
			require_ir_kind(expr->ir_binary.y, IR_ATOM);

			callee_slot = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);

			emit_bytecode_xyz(emitter, expr->site, BC_GETFIELD, callee_slot, this_slot, callee_slot);
		}
		else if (can_reuse_local_callee(expr, args, &callee_slot))
		{
			i32 result_slot = push_slot(emitter);
			ASSERT(result_slot == call_slot);
			this_slot = push_slot(emitter);
			emit_move(emitter, ir->site, this_slot, 0);
		}
		else
		{
			callee_slot = emit_expr(emitter, expr, NO_SLOT, 1);
			this_slot = push_slot(emitter);

			emit_move(emitter, ir->site, this_slot, 0);
		}

		ASSERT(callee_slot <= call_slot);
		ASSERT(this_slot == call_slot + 1);

		for (u32 i = 0; i < nargs; ++i)
		{
			i32 arg_slot = emit_expr(emitter, args.items[i], NO_SLOT, 1);
			ASSERT(arg_slot == call_slot + 2 + (i32)i);
		}

		u32 call_width = MAX(nargs + 2, nout);
		while ((i32)emitter->stack_top < call_slot + (i32)call_width) {
			push_slot(emitter);
		}
	}
	restore_slots(emitter, slot_checkpoint);

	emit_bytecode_xyzw(emitter, expr->site, BC_DYNAMIC_CALL, call_slot, callee_slot, nargs + 1, nout);
	return finish_call_result(emitter, ir, call_slot, out, nout);
}

static i32 emit_direct_call(BcEmitter *emitter, Ir ir, i32 out, u32 nout)
{
	Ir function = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;
	ASSERT(ir->ir_call.type == IR_CALL_DIRECT);
	ASSERT(function->kind == IR_FUNCTION);
	ASSERT(function->ir_function->captures.count == 0);
	ASSERT(nout <= 0xff);
	u32 prototype_index = reserve_prototype_index(emitter, function->ir_function);

	i32 slot_checkpoint = save_slots(emitter);
	i32 call_slot = slot_checkpoint;
	{
		i32 result_slot = push_slot(emitter);
		ASSERT(result_slot == call_slot);

		i32 this_slot = push_slot(emitter);
		ASSERT(this_slot == call_slot + 1);
		emit_move(emitter, ir->site, this_slot, 0);

		for (u32 i = 0; i < nargs; ++i)
		{
			i32 arg_slot = emit_expr(emitter, args.items[i], NO_SLOT, 1);
			ASSERT(arg_slot == call_slot + 2 + (i32)i);
		}

		u32 call_width = MAX(nargs + 2, nout);
		while ((i32)emitter->stack_top < call_slot + (i32)call_width) {
			push_slot(emitter);
		}
	}
	restore_slots(emitter, slot_checkpoint);

	emit_bytecode_xyzw(emitter, ir->site, BC_CALL, call_slot, prototype_index, nargs + 1, nout);
	return finish_call_result(emitter, ir, call_slot, out, nout);
}

static BcType bytecode_type_from_ir(IrKind type)
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

	ASSERT(!"unsupported binary IR kind");
	return BC_HALT;
}

static i32 emit_binary_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 slot_checkpoint = save_slots(emitter);
	i32 left_slot = emit_expr_to_slot(emitter, expr->ir_binary.x);
	i32 right_slot = emit_expr_to_slot(emitter, expr->ir_binary.y);
	restore_slots(emitter, slot_checkpoint);

	out = ensure_slot(emitter, out);
	emit_bytecode_xyz(emitter, expr->site, bytecode_type_from_ir(expr->kind), out, left_slot, right_slot);
	return out;
}

static i32 emit_unary_expr(BcEmitter *emitter, Ir expr, BcType bytecode_type, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 slot_checkpoint = save_slots(emitter);
	i32 value_slot = emit_expr_to_slot(emitter, expr->ir_unary);
	restore_slots(emitter, slot_checkpoint);

	out = ensure_slot(emitter, out);
	emit_bytecode_xy(emitter, expr->site, bytecode_type, out, value_slot);
	return out;
}

static i32 emit_nil_or_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	i32 slot_checkpoint;
	i32 temp_slot;
	u32 done_label;

	if (nout < 1) {
		return out;
	}

	out = emit_expr(emitter, expr->ir_binary.x, out, 1);
	slot_checkpoint = save_slots(emitter);
	temp_slot = push_slot(emitter);
	emit_nil(emitter, expr->site, temp_slot);
	emit_bytecode_xyz(emitter, expr->site, BC_EQ, temp_slot, temp_slot, out);
	done_label = new_label(emitter);
	jump_if_false_slot_to_label(emitter, expr->site, temp_slot, done_label);
	restore_slots(emitter, slot_checkpoint);
	emit_expr(emitter, expr->ir_binary.y, out, 1);
	define_label(emitter, done_label);
	return out;
}

static i32 emit_logical_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	u32 false_label;
	u32 done_label;

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

static void jump_if_true_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label)
{
	u32 jump = emit_conditional_jump(emitter, site, BC_JNZ, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label)
{
	u32 jump = emit_conditional_jump(emitter, site, BC_JZ, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_expr_false(BcEmitter *emitter, Ir expr, u32 false_label)
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
			u32 fallthrough_label = new_label(emitter);
			jump_if_expr_true(emitter, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_false(emitter, expr->ir_binary.y, false_label);
			define_label(emitter, fallthrough_label);
		}
		break;
		default:
		{
			i32 slot_checkpoint = save_slots(emitter);
			i32 pred_slot = emit_expr_to_slot(emitter, expr);
			jump_if_false_slot_to_label(emitter, expr->site, pred_slot, false_label);
			restore_slots(emitter, slot_checkpoint);
		}
		break;
	}
}

static void jump_if_expr_true(BcEmitter *emitter, Ir expr, u32 true_label)
{
	switch (expr->kind)
	{
		// 'a' && 'b'
		case IR_AND:
		{
			u32 fallthrough_label = new_label(emitter);
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
			i32 slot_checkpoint = save_slots(emitter);
			i32 pred_slot = emit_expr_to_slot(emitter, expr);
			jump_if_true_slot_to_label(emitter, expr->site, pred_slot, true_label);
			restore_slots(emitter, slot_checkpoint);
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
		i32 slot_checkpoint = save_slots(emitter);
		for (u32 i = 0; i < stats.count; ++i) {
			emit_stat(emitter, stats.items[i]);
		}
		restore_slots(emitter, slot_checkpoint);
		return out;
	}

	out = ensure_slot(emitter, out);
	i32 slot_checkpoint = save_slots(emitter);
	for (u32 i = 0; i < stats.count; ++i) {
		emit_stat(emitter, stats.items[i]);
	}
	i32 final_slot = emit_expr(emitter, value, out, 1);
	ASSERT(final_slot == out);
	restore_slots(emitter, slot_checkpoint);
	return out;
}

static i32 emit_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
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

			out = ensure_slot(emitter, out);
			IrFunction *function = expr->ir_function;
			u32 prototype_index = reserve_prototype_index(emitter, function);
			IrArray captures = function->captures;
			if (captures.count == 0)
			{
				emit_bytecode_xy(emitter, site, BC_CLOSURE, out, prototype_index);
			}
			else
			{
				i32 slot_checkpoint = save_slots(emitter);
				i32 first_capture_slot = slot_checkpoint;

				for (u32 i = 0; i < captures.count; ++i)
				{
					i32 next_capture_slot = emit_expr(emitter, captures.items[i], NO_SLOT, 1);
					ASSERT(slot_is_valid(next_capture_slot));
					ASSERT(first_capture_slot + (i32)i == next_capture_slot);
				}

				emit_bytecode_xy(emitter, site, BC_CLOSURE, first_capture_slot, prototype_index);
				if (first_capture_slot != out) {
					emit_move(emitter, site, out, first_capture_slot);
				}
				restore_slots(emitter, slot_checkpoint);
			}
		}
		break;

		case IR_CAPTURE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_bytecode_xy(emitter, site, BC_LOADCVAL, out, expr->ir_capture);
		}
		break;

		case IR_RECURSE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_bytecode_xy(emitter, site, BC_CURRENT_CLOSURE, out, 0);
		}
		break;

		case IR_GLOBAL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_get_global(emitter, expr->site, out, expr->ir_global);
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
			if (expr->ir_call.type == IR_CALL_DIRECT) {
				out = emit_direct_call(emitter, expr, out, nout);
			}
			else if (expr->ir_call.type == IR_CALL_DYNAMIC) {
				out = emit_dynamic_call(emitter, expr, out, nout);
			}
			else {
				NO_CODE;
			}
		}
		break;

		case IR_TABLE:
		{
			out = emit_table_expr(emitter, expr, out, nout);
		}
		break;

		case IR_ARRAY:
		{
			out = emit_array_expr(emitter, expr, out, nout);
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
			out = emit_binary_expr(emitter, expr, out, nout);
		}
		break;

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
	i32 slot_checkpoint = save_slots(emitter);
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

			case IR_GLOBAL:
			{
				i32 expr_slot = emit_expr_to_slot(emitter, expr);
				emit_set_global(emitter, destination->site, destination->ir_global, expr_slot);
			}
			break;

			case IR_FIELD:
			{
				i32 value_slot = emit_expr_to_slot(emitter, expr);
				i32 object_slot = emit_expr_to_slot(emitter, destination->ir_binary.x);
				i32 field_slot = emit_expr_to_slot(emitter, destination->ir_binary.y);

				emit_bytecode_xyz(emitter, site, BC_SETFIELD, object_slot, field_slot, value_slot);
			}
			break;

			case IR_INDEX:
			{
				i32 value_slot = emit_expr_to_slot(emitter, expr);
				i32 array_slot = emit_expr_to_slot(emitter, destination->ir_binary.x);
				i32 index_slot = emit_expr_to_slot(emitter, destination->ir_binary.y);
				emit_bytecode_xyz(emitter, site, BC_SETINDEX, array_slot, index_slot, value_slot);
			}
			break;

			default:
			{
				ASSERT(!"invalid l-value IR");
			}
			break;
		}
	}
	restore_slots(emitter, slot_checkpoint);
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
			if (stat->ir_local.expr)
			{
				slot = emit_expr(emitter, stat->ir_local.expr, NO_SLOT, 1);
				ASSERT(slot_is_valid(slot));
			}
			else
			{
				slot = push_slot(emitter);
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
			i32 slot_checkpoint = save_slots(emitter);
			i32 array_slot = emit_expr_to_slot(emitter, stat->ir_binary.x);
			i32 value_slot = emit_expr_to_slot(emitter, stat->ir_binary.y);
			emit_bytecode_xy(emitter, stat->site, BC_ARRAYADD, array_slot, value_slot);
			restore_slots(emitter, slot_checkpoint);
		}
		break;

		case IR_LABEL:
		{
			define_label(emitter, label_for_ir(emitter, stat));
		}
		break;

		case IR_JUMP:
		{
			u32 label = label_for_ir(emitter, stat->ir_jump.label);
			jump_to_label(emitter, stat->site, label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			u32 label = label_for_ir(emitter, stat->ir_jump_if_false.label);
			jump_if_expr_false(emitter, stat->ir_jump_if_false.pred, label);
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = stat->ir_block.stats;

			i32 slot_checkpoint = save_slots(emitter);
			{
				for (u32 i = 0; i < stats.count; ++i)
				{
					emit_stat(emitter, stats.items[i]);
				}
			}
			restore_slots(emitter, slot_checkpoint);
		}
		break;

		case IR_IF:
		{
			Ir pred = stat->ir_if.pred;
			Ir true_clause = stat->ir_if.true_clause;
			Ir else_clause = stat->ir_if.else_clause;

			u32 else_label = new_label(emitter);
			jump_if_expr_false(emitter, pred, else_label);

			{
				i32 true_slots = save_slots(emitter);
				emit_stat(emitter, true_clause);
				restore_slots(emitter, true_slots);
			}

			if (else_clause)
			{
				u32 end_label = new_label(emitter);
				jump_to_label(emitter, stat->site, end_label);

				i32 else_slots = save_slots(emitter);
				define_label(emitter, else_label);
				restore_slots(emitter, else_slots);
				emit_stat(emitter, else_clause);
				restore_slots(emitter, else_slots);
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
			if (expr)
			{
				i32 slot = emit_expr_to_slot(emitter, expr);
				emit_return(emitter, stat->site, slot, 1);
			}
			else
			{
				emit_return(emitter, stat->site, NO_SLOT, 0);
			}
		}
		break;

		default:
		{
			i32 slot_checkpoint = save_slots(emitter);
			emit_expr(emitter, stat, NO_SLOT, 0);
			restore_slots(emitter, slot_checkpoint);
		}
		break;
	}
}

// Functions and module

static u32 reserve_prototype_index(BcEmitter *emitter, IrFunction *function)
{
	ASSERT(function);
	if (function->index == IR_FUNCTION_INDEX_INVALID)
	{
		ASSERT(emitter->reserved_function_count < emitter->function_count);
		function->index = elf_reserve_prototype(emitter->target);
		if (function->index == ELF_PROTOTYPE_INVALID || function->index >= BC_OPERAND_COUNT_MAX) {
			abort();
		}
		emitter->reserved_function_count += 1;
	}
	return function->index;
}

static BcString string_from_atom(Atom *atom)
{
	return (BcString) {.data = atom_data(atom), .size = atom_size(atom)};
}

static void begin_function(BcEmitter *emitter, IrFunction *function)
{
	emitter->bytecode_count = 0;
	emitter->integer_constant_count = 0;
	emitter->number_constant_count = 0;
	emitter->source_map_count = 0;
	emitter->string_atom_count = 0;
	emitter->stack_size = function->param_count;
	emitter->stack_top = function->param_count;
	emitter->label_count = 0;
	emitter->jump_patch_count = 0;
	zero_memory(emitter->labels, sizeof(emitter->labels));
}

static void install_function(BcEmitter *emitter, IrFunction *function, u32 prototype_index)
{
	ASSERT(emitter->emitted_function_count < emitter->function_count);
	BcString *strings = 0;
	if (emitter->string_atom_count)
	{
		strings = arena_push(emitter->arena, sizeof(*strings) * emitter->string_atom_count, _Alignof(BcString));
		for (u32 i = 0; i < emitter->string_atom_count; ++i) {
			strings[i] = string_from_atom(emitter->string_atoms[i]);
		}
	}

	BcFunction output = {
		.variadic = function->variadic,
		.arity = function->param_count,
		.captures = function->captures.count,
		.stack_size = emitter->stack_size,
		.strings = strings,
		.string_count = emitter->string_atom_count,
		.integer_constants = emitter->integer_constants,
		.integer_constant_count = emitter->integer_constant_count,
		.number_constants = emitter->number_constants,
		.number_constant_count = emitter->number_constant_count,
		.bytecode = emitter->bytecode,
		.bytecode_count = emitter->bytecode_count,
		.source_map = emitter->source_map,
		.source_map_count = emitter->source_map_count,
	};
	if (!elf_init_prototype(emitter->target, prototype_index, &output)) {
		abort();
	}
	emitter->emitted_function_count += 1;
}

static void emit_function(BcEmitter *emitter, IrFunction *function)
{
	ASSERT(emitter);
	ASSERT(function);
	u32 prototype_index = reserve_prototype_index(emitter, function);
	begin_function(emitter, function);

	for (u32 i = 0; i < function->param_count; ++i)
	{
		Ir param = function->params[i];
		ASSERT(param && param->kind == IR_LOCAL);
		ASSERT(!slot_is_valid(param->ir_local.slot));
		param->ir_local.slot = (i32)i;
		if (param->type_rule != TRULE_NONE) {
			emit_bytecode_xy(emitter, param->site, BC_ENFORCE, param->ir_local.slot, param->type_rule);
		}
	}

	i32 slot_checkpoint = save_slots(emitter);
	emit_stat(emitter, function->body);
	restore_slots(emitter, slot_checkpoint);
	if (emitter->bytecode_count == 0 || BC_TYPE(emitter->bytecode[emitter->bytecode_count - 1]) != BC_RETURN) {
		emit_return(emitter, function->body->site, NO_SLOT, 0);
	}
	patch_jumps(emitter);
	install_function(emitter, function, prototype_index);
}

u32 bc_emit_module(elf_State *target, IrModule *module)
{
	ASSERT(target);
	ASSERT(module);
	ASSERT(module->function_count);
	ASSERT(module->last_function);
	ASSERT(!module->last_function->next);
	if (module->function_count > BC_OPERAND_COUNT_MAX) {
		abort();
	}

	u32 function_count = 0;
	for (IrFunction *function = module->first_function; function; function = function->next)
	{
		function->index = IR_FUNCTION_INDEX_INVALID;
		function_count += 1;
	}
	ASSERT(function_count == module->function_count);

	BcEmitter *emitter = arena_push_zero(module->arena, sizeof(*emitter), _Alignof(BcEmitter));
	emitter->target = target;
	emitter->arena = module->arena;
	emitter->function_count = module->function_count;
	emitter->bytecode_capacity = 1 << 13;
	emitter->bytecode = arena_push_zero(emitter->arena, sizeof(*emitter->bytecode) * emitter->bytecode_capacity, _Alignof(Bytecode));

	for (IrFunction *function = module->first_function; function; function = function->next) {
		emit_function(emitter, function);
	}
	ASSERT(emitter->reserved_function_count == emitter->function_count);
	ASSERT(emitter->emitted_function_count == emitter->function_count);
	return module->first_function->index;
}
