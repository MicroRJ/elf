//
// See Copyright Notice In elf.h
//

#include <stdlib.h>

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "bytecode.h"
#include "atom_view.h"
#include "ir.h"
#include "bc_emitter.h"


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
	IrModule         *input;
	elf_Arena        *arena;
	Bytecode         *bytecode;
	u32               bytecode_capacity;
	u32               bytecode_count;
	BcFunction       *functions;
	u32               function_count;
	u32               indexed_function_count;
	u32               generated_function_count;
	Atom            **atoms;
	u32               atom_capacity;
	u32               atom_count;
	i64              *integer_constants;
	u32               integer_constant_capacity;
	u32               integer_constant_count;
	f64              *number_constants;
	u32               number_constant_capacity;
	u32               number_constant_count;
	BcGlobalRelocation *global_relocations;
	u32                global_relocation_capacity;
	u32                global_relocation_count;
	BcSourceMapEntry *source_map;
	u32               source_map_capacity;
	u32               source_map_count;
	// -- per function state --
	u32               stack_size;
	u32               stack_top;
	BcLabel           labels[4096];
	u32               label_count;
	BcJumpPatch       jump_patches[4096];
	u32               jump_patch_count;
};

static inline b32 bc_slot_is_valid(i32 slot)
{
	return slot >= 0;
}

static i32 emit_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout);
static void emit_stat(BcEmitter *emitter, Ir stat);
static void define_label(BcEmitter *emitter, u32 label);
static void jump_to_label(BcEmitter *emitter, SourceSite site, u32 label);
static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label);
static void jump_if_expr_true(BcEmitter *emitter, Ir expr, u32 true_label);
static void jump_if_expr_false(BcEmitter *emitter, Ir expr, u32 false_label);
static void *grow_module_array(elf_Arena *arena, void *items, u32 count, u32 *capacity, u32 required_count, u64 item_size);

static b32 source_slice_equal(SourceSite left, SourceSite right)
{
	return left.source == right.source &&
	left.offset == right.offset &&
	left.size == right.size &&
	left.line_offset == right.line_offset &&
	left.line_index == right.line_index;
}

static void emit_source_map_entry(BcEmitter *emitter, SourceSite site, u32 byte_position)
{
	if (emitter->source_map_count > 0) {
		BcSourceMapEntry *entry = emitter->source_map + emitter->source_map_count - 1;
		if (entry->byte_end == byte_position && source_slice_equal(entry->site, site)) {
			entry->byte_end = byte_position + 1;
			return;
		}
	}

	emitter->source_map = grow_module_array(emitter->arena, emitter->source_map, emitter->source_map_count, &emitter->source_map_capacity, emitter->source_map_count + 1, sizeof(*emitter->source_map));
	BcSourceMapEntry *entry = emitter->source_map + emitter->source_map_count++;
	entry->byte_start = byte_position;
	entry->byte_end = byte_position + 1;
	entry->site = site;
}

#define BC_MODULE_OPERAND_COUNT_MAX (1u << 15)

// TODO(RJ): use some sort of a chunked system instead!
static void *grow_module_array(elf_Arena *arena, void *items, u32 count, u32 *capacity, u32 required_count, u64 item_size)
{
	if (required_count <= *capacity) return items;

	u32 new_capacity = *capacity ? *capacity : 64;
	while (new_capacity < required_count)
	{
		ASSERT(new_capacity <= 0x7fffffffu);
		new_capacity *= 2;
	}

	void *new_items = dy_arena_push(arena, item_size * new_capacity, 8);
	if (count) copy_memory(new_items, items, item_size * count);
	*capacity = new_capacity;
	return new_items;
}

static u32 add_const_int(BcEmitter *module, i64 i)
{
	if (module->integer_constant_count >= BC_MODULE_OPERAND_COUNT_MAX) abort();
	module->integer_constants = grow_module_array(module->arena, module->integer_constants
	, module->integer_constant_count, &module->integer_constant_capacity
	, module->integer_constant_count + 1, sizeof(*module->integer_constants));
	u32 index = module->integer_constant_count++;
	module->integer_constants[index] = i;
	return index;
}

static u32 add_const_num(BcEmitter *module, f64 i)
{
	if (module->number_constant_count >= BC_MODULE_OPERAND_COUNT_MAX) abort();
	module->number_constants = grow_module_array(module->arena, module->number_constants
	, module->number_constant_count, &module->number_constant_capacity
	, module->number_constant_count + 1, sizeof(*module->number_constants));
	u32 index = module->number_constant_count++;
	module->number_constants[index] = i;
	return index;
}

static u32 add_atom(BcEmitter *module, Atom *atom)
{
	for (u32 i = 0; i < module->atom_count; ++ i) {
		if (module->atoms[i] == atom) return i;
	}

	if (module->atom_count >= BC_MODULE_OPERAND_COUNT_MAX) abort();
	module->atoms = grow_module_array(module->arena, module->atoms
	, module->atom_count, &module->atom_capacity
	, module->atom_count + 1, sizeof(*module->atoms));
	u32 index = module->atom_count++;
	module->atoms[index] = atom;
	return index;
}

static void add_global_relocation(BcEmitter *emitter, u32 bytecode_index, Atom *name)
{
	u32 name_index = add_atom(emitter, name);
	emitter->global_relocations = grow_module_array(emitter->arena, emitter->global_relocations, emitter->global_relocation_count, &emitter->global_relocation_capacity, emitter->global_relocation_count + 1, sizeof(*emitter->global_relocations));
	emitter->global_relocations[emitter->global_relocation_count++] = (BcGlobalRelocation) {.bytecode_index = bytecode_index, .name_index = name_index};
}

#define BC_LABEL_INVALID     ((u32)-1)

/* emitters */
static void patch_jump_to_position(BcEmitter *emitter, u32 source_position, u32 target_position)
{
	BcEmitter *module = emitter;
	ASSERT(source_position < module->bytecode_count);
	Bytecode source_bytecode = module->bytecode[source_position];

	i32 relative_distance = (i32)target_position - (i32)source_position;
	ASSERT(relative_distance >= -32768 && relative_distance <= 32767);

	switch (BC_TYPE(source_bytecode))
	{
		case BC_JUMP:
		{
			module->bytecode[source_position] = BC_XXX(BC_TYPE(source_bytecode), relative_distance);
		}
		break;
		case BC_JZ:
		case BC_JNZ:
		{
			module->bytecode[source_position] = BC_XYZ(BC_TYPE(source_bytecode), relative_distance, BC_ARGY(source_bytecode), BC_ARGZ(source_bytecode));
		}
		break;
		default: NO_CODE;
	}
}

static BcLabel *get_bytecode_label(BcEmitter *emitter, u32 label)
{
	ASSERT(label < ARRAY_COUNT(emitter->labels));
	return &emitter->labels[label];
}

static u32 create_label(BcEmitter *emitter)
{
	ASSERT(emitter->label_count < ARRAY_COUNT(emitter->labels));
	return emitter->label_count ++;
}

static u32 attach_label(BcEmitter *emitter, Ir ir_label)
{
	ASSERT(ir_label);
	ASSERT(ir_label->kind == IR_LABEL);

	if (!ir_label->ir_label.has_bytecode_label)
	{
		ir_label->ir_label.bytecode_label = create_label(emitter);
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

static void patch_bytecode_labels(BcEmitter *emitter)
{
	for (u32 i = 0; i < emitter->jump_patch_count; ++ i)
	{
		BcJumpPatch patch = emitter->jump_patches[i];
		BcLabel *label = get_bytecode_label(emitter, patch.label);
		ASSERT(label->defined);
		patch_jump_to_position(emitter, patch.bytecode_position, (u32)label->position);
	}
	emitter->jump_patch_count = 0;
}

static u32 emit_bc(BcEmitter *emitter, SourceSite site, Bytecode byte)
{
	BcEmitter *module = emitter;
	ASSERT(module->bytecode_count < 0xffffffffu);
	module->bytecode = grow_module_array(module->arena, module->bytecode, module->bytecode_count, &module->bytecode_capacity, module->bytecode_count + 1, sizeof(*module->bytecode));
	u32 position = module->bytecode_count++;
	module->bytecode[position] = byte;
	emit_source_map_entry(emitter, site, position);
	return position;
}

static u32 emit_x_bc(BcEmitter *emitter, SourceSite site, BcType k, i32 x)
{
	Bytecode byte = BC_XXX(k, x);
	return emit_bc(emitter, site, byte);
}

static u32 emit_xy_bc(BcEmitter *emitter, SourceSite site, BcType k, i32 x, i32 y)
{
	Bytecode byte = BC_XYY(k,x,y);
	return emit_bc(emitter,site,byte);
}

static u32 emit_xyz_bc(BcEmitter *emitter, SourceSite site, BcType k, i32 x, i32 y, i32 z)
{
	Bytecode byte = BC_XYZ(k, x, y, z);
	return emit_bc(emitter, site, byte);
}

static u32 emit_xyzw_bc(BcEmitter *emitter, SourceSite site, BcType k, i32 x, i32 y, i32 z, i32 w)
{
	Bytecode byte = BC_XYZW(k, x, y, z, w);
	return emit_bc(emitter, site, byte);
}

static u32 emit_jump_bc(BcEmitter *emitter, SourceSite site)
{
	return emit_x_bc(emitter, site, BC_JUMP, 0);
}

static u32 emit_jump_if_bc(BcEmitter *emitter, SourceSite site, BcType type, i32 pred)
{
	ASSERT(type == BC_JZ || type == BC_JNZ);
	ASSERT(bc_slot_is_valid(pred));
	return emit_xy_bc(emitter, site, type, 0, pred);
}

static u32 emit_load_int_bc(BcEmitter *emitter, SourceSite site, i32 dest, i64 integer)
{
	ASSERT(bc_slot_is_valid(dest));
	u32 index = add_const_int(emitter, integer);
	return emit_xy_bc(emitter, site, BC_LOADKINT, dest, index);
}

static u32 emit_load_num_bc(BcEmitter *emitter, SourceSite site, i32 dest, f64 number)
{
	ASSERT(bc_slot_is_valid(dest));
	u32 index = add_const_num(emitter, number);
	return emit_xy_bc(emitter, site, BC_LOADKNUM, dest, index);
}

static u32 emit_load_atom_bc(BcEmitter *emitter, SourceSite site, i32 dest, Atom *atom)
{
	ASSERT(bc_slot_is_valid(dest));
	u32 index = add_atom(emitter, atom);
	return emit_xy_bc(emitter, site, BC_LOADKSTRING, dest, index);
}

static u32 emit_load_nil_bc(BcEmitter *emitter, SourceSite site, i32 dest)
{
	ASSERT(bc_slot_is_valid(dest));
	return emit_xy_bc(emitter, site, BC_LOADNIL, dest, 0);
}

static u32 emit_load_glb_bc(BcEmitter *emitter, SourceSite site, i32 dest, Atom *name)
{
	ASSERT(bc_slot_is_valid(dest));
	u32 bytecode_index = emit_xy_bc(emitter, site, BC_GETGLOBAL, dest, 0);
	add_global_relocation(emitter, bytecode_index, name);
	return bytecode_index;
}

static u32 emit_store_glb_bc(BcEmitter *emitter, SourceSite site, Atom *name, i32 source)
{
	ASSERT(bc_slot_is_valid(source));
	u32 bytecode_index = emit_xy_bc(emitter, site, BC_SETGLOBAL, 0, source);
	add_global_relocation(emitter, bytecode_index, name);
	return bytecode_index;
}

static u32 emit_reload_bc(BcEmitter *emitter, SourceSite site, i32 dst, i32 src)
{
	ASSERT(bc_slot_is_valid(dst));
	ASSERT(bc_slot_is_valid(src));
	return emit_xy_bc(emitter, site, BC_RELOAD, dst, src);
}

static u32 emit_return_bc(BcEmitter *emitter, SourceSite site, i32 out, u32 nout)
{
	ASSERT(!nout || bc_slot_is_valid(out));
	i32 slot_index = nout ? out : 0;
	return emit_xy_bc(emitter, site, BC_RETURN, slot_index, nout);
}

static Ir check_ir_type(Ir ir, IrKind kind)
{
	ASSERT(ir);
	ASSERT(ir->kind == kind);
	return ir;
}

/* memory management */
static i32 mark_slots(BcEmitter *emitter)
{
	return (i32)emitter->stack_top;
}

static void restore_slots(BcEmitter *emitter, i32 stack_top)
{
	ASSERT(bc_slot_is_valid(stack_top));
	emitter->stack_top = (u32)stack_top;
}

static i32 alloc_slot(BcEmitter *emitter)
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
	if (!bc_slot_is_valid(slot)) {
		slot = alloc_slot(emitter);
	}
	return slot;
}

static BcString copy_emitter_string(elf_Arena *arena, Atom *atom)
{
	u32 size = atom_size(atom);
	char *data = dy_arena_push_zero(arena, size + 1, 1);
	copy_memory(data, atom_data(atom), size);
	return (BcString) {.data = data, .size = size};
}

static u32 bc_function_index(BcEmitter *emitter, IrFunction *function)
{
	ASSERT(function);
	if (function->index == IR_FUNCTION_INDEX_INVALID)
	{
		ASSERT(emitter->indexed_function_count < emitter->function_count);
		function->index = emitter->indexed_function_count++;
	}
	ASSERT(function->index < emitter->function_count);
	return function->index;
}

static BcModule *bc_finish_module(BcEmitter *emitter)
{
	ASSERT(emitter->indexed_function_count == emitter->function_count);
	ASSERT(emitter->generated_function_count == emitter->function_count);
	elf_Arena *arena = emitter->arena;
	BcModule *module = dy_arena_push_zero(arena, sizeof(*module), 8);
	module->bytecode = emitter->bytecode;
	module->bytecode_count = emitter->bytecode_count;
	module->source_map = emitter->source_map;
	module->source_map_count = emitter->source_map_count;
	module->functions = emitter->functions;
	module->function_count = emitter->function_count;
	module->string_count = emitter->atom_count;
	if (module->string_count)
	{
		module->strings = dy_arena_push_zero(arena, sizeof(*module->strings) * module->string_count, 8);
		for (u32 i = 0; i < module->string_count; ++i) {
			module->strings[i] = copy_emitter_string(arena, emitter->atoms[i]);
		}
	}
	module->integer_constants = emitter->integer_constants;
	module->integer_constant_count = emitter->integer_constant_count;
	module->number_constants = emitter->number_constants;
	module->number_constant_count = emitter->number_constant_count;
	module->global_relocations = emitter->global_relocations;
	module->global_relocation_count = emitter->global_relocation_count;
	return module;
}

static void bc_emit_function(BcEmitter *emitter, IrFunction *function)
{
	ASSERT(emitter);
	ASSERT(function);
	BcEmitter *module = emitter;
	u32 function_index = bc_function_index(emitter, function);

	u32 bytecode_start = module->bytecode_count;
	emitter->stack_size = function->param_count;
	emitter->stack_top = function->param_count;
	emitter->label_count = 0;
	emitter->jump_patch_count = 0;
	zero_memory(emitter->labels, sizeof(emitter->labels));

	Ir body = function->body;
	for (u32 i = 0; i < function->param_count; ++i)
	{
		Ir param = function->params[i];
		ASSERT(param && param->kind == IR_LOCAL);
		ASSERT(!bc_slot_is_valid(param->ir_local.slot));
		param->ir_local.slot = (i32)i;
		if (param->type_rule != TRULE_NONE) {
			emit_xy_bc(emitter, param->site, BC_ENFORCE, param->ir_local.slot, param->type_rule);
		}
	}

	i32 memory_checkpoint = mark_slots(emitter);
	emit_stat(emitter, body);
	restore_slots(emitter, memory_checkpoint);

	if (module->bytecode_count == bytecode_start || BC_TYPE(module->bytecode[module->bytecode_count - 1]) != BC_RETURN)
	{
		emit_return_bc(emitter, body->site, NO_SLOT, 0);
	}

	patch_bytecode_labels(emitter);

	ASSERT(module->generated_function_count < module->function_count);
	BcFunction *bc_function = module->functions + function_index;
	module->generated_function_count += 1;
	bc_function->variadic = function->variadic;
	bc_function->arity = function->param_count;
	bc_function->offset = bytecode_start;
	bc_function->length = module->bytecode_count - bytecode_start;
	bc_function->captures = function->captures.count;
	bc_function->stack_size = emitter->stack_size;
}

BcModule *bc_emit_module(IrModule *input)
{
	ASSERT(input);
	ASSERT(input->function_count);
	u32 function_count = 0;
	for (IrFunction *function = input->first_function; function; function = function->next)
	{
		function->index = IR_FUNCTION_INDEX_INVALID;
		function_count += 1;
	}
	ASSERT(function_count == input->function_count);
	ASSERT(input->last_function);
	ASSERT(!input->last_function->next);

	elf_Arena *arena = input->arena;

	// TODO(RJ): proper error handling
	if (input->function_count > BC_MODULE_OPERAND_COUNT_MAX) abort();

	// TODO(RJ): allocate the emitter itself on a scratch arena!
	BcEmitter *emitter = dy_arena_push_zero(arena, sizeof(*emitter), 8);
	emitter->input = input;
	emitter->arena = arena;
	emitter->function_count = input->function_count;
	emitter->functions = dy_arena_push_zero(arena, sizeof(*emitter->functions) * emitter->function_count, 8);

	emitter->bytecode_capacity = 1 << 13;
	emitter->bytecode = dy_arena_push_zero(arena, sizeof(*emitter->bytecode) * emitter->bytecode_capacity, 8);

	for (IrFunction *function = input->first_function; function; function = function->next) {
		bc_emit_function(emitter, function);
	}
	return bc_finish_module(emitter);
}

static i32 ensure_expr_memory(BcEmitter *emitter, Ir expr)
{
	i32 slot = NO_SLOT;
	if (expr->kind == IR_LOAD_LOCAL) {
		Ir local = expr->ir_load_local.local;
		check_ir_type(local, IR_LOCAL);
		slot = local->ir_local.slot;
	}
	else {
		slot = emit_expr(emitter, expr, NO_SLOT, 1);
	}

	ASSERT(bc_slot_is_valid(slot));
	return slot;
}

static i32 emit_new_table_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_x_bc(emitter, expr->site, BC_TABLE, out);
	return out;
}

static i32 emit_new_array_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1)
	{
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_x_bc(emitter, expr->site, BC_ARRAY, out);
	return out;
}

static i32 finish_call(BcEmitter *emitter, Ir ir, i32 call_slot, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	if (!bc_slot_is_valid(out))
	{
		out = alloc_slot(emitter);
		for (u32 i = 1; i < nout; ++i) {
			alloc_slot(emitter);
		}
	}

	if (out != call_slot)
	{
		for (u32 i = 0; i < nout; ++i)
		{
			i32 destination = out + (i32)i;
			i32 source = call_slot + (i32)i;
			emit_reload_bc(emitter, ir->site, destination, source);
		}
	}

	return out;
}

static b32 call_can_reference_local_directly(Ir expr, IrArray args, i32 *callee_slot)
{
	if (expr->kind != IR_LOAD_LOCAL) {
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
			case IR_LOAD_LOCAL:
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

	Ir local = check_ir_type(expr->ir_load_local.local, IR_LOCAL);
	ASSERT(bc_slot_is_valid(local->ir_local.slot));
	*callee_slot = local->ir_local.slot;
	return true;
}

static i32 emit_dynamic_call(BcEmitter *emitter, Ir ir, i32 out, u32 nout)
{
	ASSERT(ir->ir_call.type == IR_CALL_DYNAMIC);
	Ir expr = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;

	i32 stack_top = mark_slots(emitter);
	i32 callee_slot = NO_SLOT;
	i32 call_slot = NO_SLOT;
	{
		i32 this_slot;
		if (expr->kind == IR_META_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			callee_slot = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);

			emit_xyz_bc(emitter, expr->site, BC_GETMETAFIELD
			,	callee_slot, this_slot, callee_slot);
		}
		else if (expr->kind == IR_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			callee_slot = emit_expr(emitter, expr->ir_binary.y, NO_SLOT, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_SLOT, 1);

			emit_xyz_bc(emitter, expr->site, BC_GETFIELD
			,	callee_slot, this_slot, callee_slot);
		}
		else if (call_can_reference_local_directly(expr, args, &callee_slot))
		{
			this_slot = alloc_slot(emitter);
			emit_xy_bc(emitter, ir->site, BC_RELOAD, this_slot, 0);
		}
		else
		{
			callee_slot = emit_expr(emitter, expr, NO_SLOT, 1);
			this_slot = alloc_slot(emitter);

			emit_xy_bc(emitter, ir->site, BC_RELOAD, this_slot, 0);
		}

		call_slot = this_slot;
		ASSERT(callee_slot < call_slot);

		for (u32 i = 0; i < nargs; ++ i)
		{
			i32 arg_slot = emit_expr(emitter, args.items[i], NO_SLOT, 1);
			ASSERT(arg_slot == call_slot + 1 + (i32)i);
		}

		u32 call_width = MAX(nargs + 1, nout);
		while ((i32)emitter->stack_top < call_slot + (i32)call_width) {
			alloc_slot(emitter);
		}
	}
	restore_slots(emitter, stack_top);

	emit_xyzw_bc(emitter, expr->site, BC_DYNAMIC_CALL, call_slot, callee_slot, nargs + 1, nout);
	return finish_call(emitter, ir, call_slot, out, nout);
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
	u32 function_index = bc_function_index(emitter, function->ir_function);

	i32 stack_top = mark_slots(emitter);
	i32 call_slot = stack_top;
	{
		i32 this_slot = alloc_slot(emitter);
		ASSERT(this_slot == call_slot);
		emit_xy_bc(emitter, ir->site, BC_RELOAD, this_slot, 0);

		for (u32 i = 0; i < nargs; ++i)
		{
			i32 arg_slot = emit_expr(emitter, args.items[i], NO_SLOT, 1);
			ASSERT(arg_slot == call_slot + 1 + (i32)i);
		}

		u32 call_width = MAX(nargs + 1, nout);
		while ((i32)emitter->stack_top < call_slot + (i32)call_width) {
			alloc_slot(emitter);
		}
	}
	restore_slots(emitter, stack_top);

	emit_xyzw_bc(emitter, ir->site, BC_CALL, call_slot, function_index, nargs + 1, nout);
	return finish_call(emitter, ir, call_slot, out, nout);
}

static BcType bc_op(IrKind type)
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

static i32 emit_binary_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 memory_checkpoint = mark_slots(emitter);
	i32 left_slot = ensure_expr_memory(emitter, expr->ir_binary.x);
	i32 right_slot = ensure_expr_memory(emitter, expr->ir_binary.y);
	restore_slots(emitter, memory_checkpoint);

	out = ensure_slot(emitter, out);
	emit_xyz_bc(emitter, expr->site, bc_op(expr->kind)
	,	out, left_slot, right_slot);
	return out;
}

static i32 emit_unary_expr(BcEmitter *emitter, Ir expr, BcType bytecode_type, i32 out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	i32 memory_checkpoint = mark_slots(emitter);
	i32 value_slot = ensure_expr_memory(emitter, expr->ir_unary);
	restore_slots(emitter, memory_checkpoint);

	out = ensure_slot(emitter, out);
	emit_xy_bc(emitter, expr->site, bytecode_type, out, value_slot);
	return out;
}

static i32 emit_truthy_or_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	i32 memory_checkpoint;
	i32 temp_slot;
	u32 done_label;

	if (nout < 1) return out;

	out = emit_expr(emitter, expr->ir_binary.x, out, 1);
	memory_checkpoint = mark_slots(emitter);
	temp_slot = alloc_slot(emitter);
	emit_load_nil_bc(emitter, expr->site, temp_slot);
	emit_xyz_bc(emitter, expr->site, BC_EQ,	temp_slot, temp_slot, out);
	done_label = create_label(emitter);
	jump_if_false_slot_to_label(emitter, expr->site, temp_slot, done_label);
	restore_slots(emitter, memory_checkpoint);
	emit_expr(emitter, expr->ir_binary.y, out, 1);
	define_label(emitter, done_label);
	return out;
}

static i32 emit_logical_expr(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	u32 false_label;
	u32 done_label;

	if (nout < 1) return out;

	false_label = create_label(emitter);
	done_label = create_label(emitter);

	jump_if_expr_false(emitter, expr, false_label);
	out = ensure_slot(emitter, out);
	emit_load_int_bc(emitter, expr->site, out, 1);
	jump_to_label(emitter, expr->site, done_label);
	define_label(emitter, false_label);
	emit_load_int_bc(emitter, expr->site, out, 0);
	define_label(emitter, done_label);

	return out;
}

static void jump_if_true_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label)
{
	u32 jump = emit_jump_if_bc(emitter, site, BC_JNZ, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_false_slot_to_label(BcEmitter *emitter, SourceSite site, i32 pred, u32 label)
{
	u32 jump = emit_jump_if_bc(emitter, site, BC_JZ, pred);
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
			u32 fallthrough_label = create_label(emitter);
			jump_if_expr_true(emitter, expr->ir_binary.x, fallthrough_label);
			jump_if_expr_false(emitter, expr->ir_binary.y, false_label);
			define_label(emitter, fallthrough_label);
		}
		break;
		default:
		{
			i32 memory_checkpoint = mark_slots(emitter);
			i32 pred_slot = ensure_expr_memory(emitter, expr);
			jump_if_false_slot_to_label(emitter, expr->site, pred_slot, false_label);
			restore_slots(emitter, memory_checkpoint);
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
			u32 fallthrough_label = create_label(emitter);
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
			i32 memory_checkpoint = mark_slots(emitter);
			i32 pred_slot = ensure_expr_memory(emitter, expr);
			jump_if_true_slot_to_label(emitter, expr->site, pred_slot, true_label);
			restore_slots(emitter, memory_checkpoint);
		}
		break;
	}
}

static i32 do_expr_block(BcEmitter *emitter, Ir expr, i32 out, u32 nout)
{
	IrArray stats = expr->ir_expr_block.stats;
	Ir value = expr->ir_expr_block.value;

	if (nout < 1)
	{
		i32 memory_checkpoint = mark_slots(emitter);
		for (u32 i = 0; i < stats.count; ++ i) {
			emit_stat(emitter, stats.items[i]);
		}
		restore_slots(emitter, memory_checkpoint);
		return out;
	}

	out = ensure_slot(emitter, out);
	i32 memory_checkpoint = mark_slots(emitter);
	for (u32 i = 0; i < stats.count; ++ i) {
		emit_stat(emitter, stats.items[i]);
	}
	i32 final_slot = emit_expr(emitter, value, out, 1);
	ASSERT(final_slot == out);
	restore_slots(emitter, memory_checkpoint);
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
			u32 bytecode_function_id = bc_function_index(emitter, function);
			IrArray captures = function->captures;
			if (captures.count == 0)
			{
				emit_xy_bc(emitter, site, BC_CLOSURE, out, bytecode_function_id);
			}
			else
			{
				i32 memory_checkpoint = mark_slots(emitter);
				i32 first_capture_slot = memory_checkpoint;

				for (u32 i = 0; i < captures.count; ++ i)
				{
					i32 next_capture_slot = emit_expr(emitter, captures.items[i], NO_SLOT, 1);
					ASSERT(bc_slot_is_valid(next_capture_slot));
					ASSERT(first_capture_slot + (i32)i == next_capture_slot);
				}

				emit_xy_bc(emitter, site, BC_CLOSURE, first_capture_slot, bytecode_function_id);
				if (first_capture_slot != out) {
					emit_reload_bc(emitter, site, out, first_capture_slot);
				}
				restore_slots(emitter, memory_checkpoint);
			}
		}
		break;

		case IR_CAPTURE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_xy_bc(emitter, site, BC_LOADCVAL, out, expr->ir_capture);
		}
		break;

		case IR_RECURSE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_xy_bc(emitter, site, BC_CURRENT_CLOSURE, out, 0);
		}
		break;

		case IR_GLOBAL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_load_glb_bc(emitter, expr->site, out, expr->ir_global);
		}
		break;

		case IR_LOAD_LOCAL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			Ir local = expr->ir_load_local.local;
			check_ir_type(local, IR_LOCAL);
			ASSERT(bc_slot_is_valid(local->ir_local.slot));

			emit_reload_bc(emitter, expr->site, out, local->ir_local.slot);
		}
		break;

		case IR_GET_MEM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			Ir target = expr->ir_unary;
			check_ir_type(target, IR_LOAD_LOCAL);

			Ir local = target->ir_load_local.local;
			check_ir_type(local, IR_LOCAL);
			ASSERT(bc_slot_is_valid(local->ir_local.slot));

			emit_load_int_bc(emitter, expr->site, out, local->ir_local.slot);
		}
		break;

		case IR_INTEGER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_load_int_bc(emitter, expr->site, out, expr->ir_int);
		}
		break;

		case IR_NUMBER:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_load_num_bc(emitter, expr->site, out, expr->ir_num);
		}
		break;

		case IR_ATOM:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_load_atom_bc(emitter, expr->site, out, expr->atom);
		}
		break;

		case IR_NIL:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_load_nil_bc(emitter, expr->site, out);
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
			out = emit_new_table_expr(emitter, expr, out, nout);
		}
		break;

		case IR_ARRAY:
		{
			out = emit_new_array_expr(emitter, expr, out, nout);
		}
		break;

		case IR_EXPR_BLOCK:
		{
			out = do_expr_block(emitter, expr, out, nout);
		}
		break;

		case IR_MEASURE:
		{
			out = emit_unary_expr(emitter, expr, BC_MEASURE, out, nout);
		}
		break;

		case IR_NIL_OR:
		{
			out = emit_truthy_or_expr(emitter, expr, out, nout);
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

static void emit_store(BcEmitter *emitter, SourceSite site, Ir dest, Ir expr)
{
	i32 memory_checkpoint = mark_slots(emitter);
	{
		switch (dest->kind)
		{
			case IR_LOAD_LOCAL:
			{
				Ir local = dest->ir_load_local.local;
				check_ir_type(local, IR_LOCAL);

				i32 dest_slot = local->ir_local.slot;
				ASSERT(bc_slot_is_valid(dest_slot));

				i32 final_slot = emit_expr(emitter, expr, dest_slot, 1);
				ASSERT(final_slot == dest_slot);
			}
			break;

			case IR_GLOBAL:
			{
				i32 expr_slot = ensure_expr_memory(emitter, expr);
				emit_store_glb_bc(emitter, dest->site, dest->ir_global, expr_slot);
			}
			break;

			case IR_FIELD:
			{
				i32 value_slot = ensure_expr_memory(emitter, expr);
				i32 object_slot = ensure_expr_memory(emitter, dest->ir_binary.x);
				i32 field_slot = ensure_expr_memory(emitter, dest->ir_binary.y);

				emit_xyz_bc(emitter, site, BC_SETFIELD
				,	object_slot, field_slot, value_slot);
			}
			break;

			case IR_INDEX:
			{
				i32 value_slot = ensure_expr_memory(emitter, expr);
				i32 array_slot = ensure_expr_memory(emitter, dest->ir_binary.x);
				i32 index_slot = ensure_expr_memory(emitter, dest->ir_binary.y);
				emit_xyz_bc(emitter, site, BC_SETINDEX
				,	array_slot, index_slot, value_slot);
			}
			break;

			default:
			{
				ASSERT(!"invalid l-value IR");
			}
			break;
		}
	}
	restore_slots(emitter, memory_checkpoint);
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
			ASSERT(!bc_slot_is_valid(slot));
			if (stat->ir_local.expr)
			{
				slot = emit_expr(emitter, stat->ir_local.expr, NO_SLOT, 1);
				ASSERT(bc_slot_is_valid(slot));
			}
			else
			{
				slot = alloc_slot(emitter);
			}

			stat->ir_local.slot = slot;
		}
		break;

		case IR_MULTI_LOCAL:
		{
			IrArray locals = stat->ir_multi_local.locals;
			ASSERT(locals.count > 1);
			i32 first = emit_expr(emitter, stat->ir_multi_local.expr, NO_SLOT, locals.count);
			ASSERT(bc_slot_is_valid(first));
			for (u32 i = 0; i < locals.count; ++i)
			{
				Ir local = locals.items[i];
				ASSERT(local && local->kind == IR_LOCAL);
				ASSERT(!bc_slot_is_valid(local->ir_local.slot));
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
			i32 memory_checkpoint = mark_slots(emitter);
			i32 array_slot = ensure_expr_memory(emitter, stat->ir_binary.x);
			i32 value_slot = ensure_expr_memory(emitter, stat->ir_binary.y);
			emit_xy_bc(emitter, stat->site, BC_ARRAYADD, array_slot, value_slot);
			restore_slots(emitter, memory_checkpoint);
		}
		break;

		case IR_LABEL:
		{
			define_label(emitter, attach_label(emitter, stat));
		}
		break;

		case IR_JUMP:
		{
			u32 label = attach_label(emitter, stat->ir_jump.label);
			jump_to_label(emitter, stat->site, label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			u32 label = attach_label(emitter, stat->ir_jump_if_false.label);
			jump_if_expr_false(emitter, stat->ir_jump_if_false.pred, label);
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = stat->ir_block.stats;

			i32 memory_checkpoint = mark_slots(emitter);
			{
				for (u32 i = 0; i < stats.count; ++ i)
				{
					emit_stat(emitter, stats.items[i]);
				}
			}
			restore_slots(emitter, memory_checkpoint);
		}
		break;

		case IR_IF:
		{
			Ir pred = stat->ir_if.pred;
			Ir true_clause = stat->ir_if.true_clause;
			Ir else_clause = stat->ir_if.else_clause;

			u32 else_label = create_label(emitter);
			jump_if_expr_false(emitter, pred, else_label);

			{
				i32 true_memory = mark_slots(emitter);
				emit_stat(emitter, true_clause);
				restore_slots(emitter, true_memory);
			}

			if (else_clause)
			{
				u32 end_label = create_label(emitter);
				jump_to_label(emitter, stat->site, end_label);

				i32 else_memory = mark_slots(emitter);
				define_label(emitter, else_label);
				restore_slots(emitter, else_memory);
				emit_stat(emitter, else_clause);
				restore_slots(emitter, else_memory);
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
				i32 slot = ensure_expr_memory(emitter, expr);
				emit_return_bc(emitter, stat->site, slot, 1);
			}
			else
			{
				emit_return_bc(emitter, stat->site, NO_SLOT, 0);
			}
		}
		break;

		default:
		{
			i32 memory_checkpoint = mark_slots(emitter);
			emit_expr(emitter, stat, NO_SLOT, 0);
			restore_slots(emitter, memory_checkpoint);
		}
		break;
	}
}

static void define_label(BcEmitter *emitter, u32 label)
{
	BcLabel *target = get_bytecode_label(emitter, label);
	ASSERT(!target->defined);

	target->defined = true;
	target->position = emitter->bytecode_count;
}

static void jump_to_label(BcEmitter *emitter, SourceSite site, u32 label)
{
	u32 jump = emit_jump_bc(emitter, site);
	record_jump_patch(emitter, jump, label);
}
