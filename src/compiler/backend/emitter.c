//
// See Copyright Notice In elf.h
//

static inline b32 bc_slot_is_valid(BcSlot memory)
{
	return memory.slot >= 0;
}

static inline i32 unwrap_slot(BcSlot memory)
{
	ASSERT(bc_slot_is_valid(memory));
	return memory.slot;
}

static BcSlot emit_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout);
static void emit_stat(BcFunctionEmitter *emitter, Ir stat);
static void emit_function(BcFunctionEmitter *emitter, IrFunction *function);
static b32 emit_module(Compiler *compiler, elf_Arena *output_arena, Com_Module *module, IrModule ir_module);
static void define_label(BcFunctionEmitter *emitter, u32 label);
static void jump_to_label(BcFunctionEmitter *emitter, SourceSite site, u32 label);
static void jump_if_false_slot_to_label(BcFunctionEmitter *emitter, SourceSite site, BcSlot pred, u32 label);
static void jump_if_expr_true(BcFunctionEmitter *emitter, Ir expr, u32 true_label);
static void jump_if_expr_false(BcFunctionEmitter *emitter, Ir expr, u32 false_label);
static SourceMapEntry *copy_source_map(elf_Arena *arena, SourceMapEntry *entries, u32 count);

typedef enum
{
	EMISSION_ERROR_GENERIC = 0,
	EMISSION_ERROR_INTERNAL,
	EMISSION_ERROR_UNREFERENCED_ENTITY,
	EMISSION_ERROR_INVALID_LVALUE,
	EMISSION_ERROR_INVALID_EXPRESSION,
	EMISSION_ERROR_UNDECLARED_IDENTIFIER,
}
EmissionError;

static void report_emission_error(BcFunctionEmitter *emitter, EmissionError error, SourceSite site, const char *format, ...)
{
	(void)error;

	va_list args;
	va_start(args, format);
	compiler_reportv(emitter->module->compiler, COMPILER_ERROR, COMPILER_PHASE_BYTECODE,
		site, format, args);
	va_end(args);
}

static b32 source_slice_equal(SourceSite left, SourceSite right)
{
	return left.offset == right.offset &&
		left.size == right.size &&
		left.line_offset == right.line_offset &&
		left.line_index == right.line_index;
}

static void emit_source_map_entry(BcFunctionEmitter *emitter, SourceSite site, u32 byte_position)
{
	SourceMapBuffer *source_map = &emitter->source_map_buffer;
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

/* module builder */
#define BC_MODULE_OPERAND_COUNT_MAX (1u << 15)

static void *grow_module_array(elf_Arena *arena, void *items, u32 count, u32 *capacity, u32 required_count, u64 item_size)
{
	if (required_count <= *capacity) return items;

	u32 new_capacity = *capacity ? *capacity : 64;
	while (new_capacity < required_count)
	{
		ASSERT(new_capacity <= 0x7fffffffu);
		new_capacity *= 2;
	}

	elf_arena_align(arena, 8);
	void *new_items = elf_arena_push(arena, item_size * new_capacity);
	if (count) copy_memory(new_items, items, item_size * count);
	*capacity = new_capacity;
	return new_items;
}

static u32 add_const_int(BcModuleBuilder *module, i64 i)
{
	if (module->integer_constant_count >= BC_MODULE_OPERAND_COUNT_MAX) abort();
	module->integer_constants = grow_module_array(module->arena, module->integer_constants
	, module->integer_constant_count, &module->integer_constant_capacity
	, module->integer_constant_count + 1, sizeof(*module->integer_constants));
	u32 index = module->integer_constant_count++;
	module->integer_constants[index] = i;
	return index;
}

static u32 add_const_num(BcModuleBuilder *module, f64 i)
{
	if (module->number_constant_count >= BC_MODULE_OPERAND_COUNT_MAX) abort();
	module->number_constants = grow_module_array(module->arena, module->number_constants
	, module->number_constant_count, &module->number_constant_capacity
	, module->number_constant_count + 1, sizeof(*module->number_constants));
	u32 index = module->number_constant_count++;
	module->number_constants[index] = i;
	return index;
}

static u32 add_atom(BcModuleBuilder *module, Atom *atom)
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

#define BC_LABEL_INVALID     ((u32)-1)

/* emitters */
static void patch_jump_to_position(BcFunctionEmitter *emitter, u32 source_position, u32 target_position)
{
	BcModuleBuilder *module = emitter->module;
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

static BytecodeLabel *get_bytecode_label(BcFunctionEmitter *emitter, u32 label)
{
	ASSERT(label < ARRAY_COUNT(emitter->labels));
	return &emitter->labels[label];
}

static u32 create_label(BcFunctionEmitter *emitter)
{
	ASSERT(emitter->label_count < ARRAY_COUNT(emitter->labels));
	return emitter->label_count ++;
}

static u32 attach_label(BcFunctionEmitter *emitter, Ir ir_label)
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

static void record_jump_patch(BcFunctionEmitter *emitter, u32 bytecode_position, u32 label)
{
	ASSERT(label != BC_LABEL_INVALID);
	ASSERT(emitter->jump_patch_count < ARRAY_COUNT(emitter->jump_patches));

	BytecodeJumpPatch *patch = emitter->jump_patches + emitter->jump_patch_count++;
	patch->bytecode_position = bytecode_position;
	patch->label = label;
}

static void patch_bytecode_labels(BcFunctionEmitter *emitter)
{
	for (u32 i = 0; i < emitter->jump_patch_count; ++ i)
	{
		BytecodeJumpPatch patch = emitter->jump_patches[i];
		BytecodeLabel *label = get_bytecode_label(emitter, patch.label);
		ASSERT(label->defined);
		patch_jump_to_position(emitter, patch.bytecode_position, (u32)label->position);
	}
	emitter->jump_patch_count = 0;
}

static u32 emit_bc(BcFunctionEmitter *emitter, SourceSite site, Bytecode byte)
{
	BcModuleBuilder *module = emitter->module;
	ASSERT(module->bytecode_count < 0xffffffffu);
	module->bytecode = grow_module_array(module->arena, module->bytecode, module->bytecode_count, &module->bytecode_capacity, module->bytecode_count + 1, sizeof(*module->bytecode));
	u32 position = module->bytecode_count++;
	module->bytecode[position] = byte;
	emit_source_map_entry(emitter, site, position);
	return position;
}

static u32 emit_x_bc(BcFunctionEmitter *emitter, SourceSite site, BytecodeType k, i32 x)
{
	Bytecode byte = BC_XXX(k, x);
	return emit_bc(emitter, site, byte);
}

static u32 emit_xy_bc(BcFunctionEmitter *emitter, SourceSite site, BytecodeType k, i32 x, i32 y)
{
	Bytecode byte = BC_XYY(k,x,y);
	return emit_bc(emitter,site,byte);
}

static u32 emit_xyz_bc(BcFunctionEmitter *emitter, SourceSite site, BytecodeType k, i32 x, i32 y, i32 z)
{
	Bytecode byte = BC_XYZ(k, x, y, z);
	return emit_bc(emitter, site, byte);
}

static u32 emit_xyzw_bc(BcFunctionEmitter *emitter, SourceSite site, BytecodeType k, i32 x, i32 y, i32 z, i32 w)
{
	Bytecode byte = BC_XYZW(k, x, y, z, w);
	return emit_bc(emitter, site, byte);
}

static u32 emit_jump_bc(BcFunctionEmitter *emitter, SourceSite site)
{
	return emit_x_bc(emitter, site, BC_JUMP, 0);
}

static u32 emit_jump_if_bc(BcFunctionEmitter *emitter, SourceSite site, BytecodeType type, BcSlot pred)
{
	ASSERT(type == BC_JZ || type == BC_JNZ);
	return emit_xy_bc(emitter, site, type, 0, unwrap_slot(pred));
}

static u32 emit_load_int_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dest, i64 integer)
{
	u32 index = add_const_int(emitter->module, integer);
	return emit_xy_bc(emitter, site, BC_LOADKINT, unwrap_slot(dest), index);
}

static u32 emit_load_num_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dest, f64 number)
{
	u32 index = add_const_num(emitter->module, number);
	return emit_xy_bc(emitter, site, BC_LOADKNUM, unwrap_slot(dest), index);
}

static u32 emit_load_atom_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dest, Atom *atom)
{
	u32 index = add_atom(emitter->module, atom);
	return emit_xy_bc(emitter, site, BC_LOADKSTRING, unwrap_slot(dest), index);
}

static u32 emit_load_nil_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dest)
{
	return emit_xy_bc(emitter, site, BC_LOADNIL, unwrap_slot(dest), 0);
}

static u32 emit_load_glb_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dest, u32 global_index)
{
	return emit_xy_bc(emitter, site, BC_GETGLOBAL, unwrap_slot(dest), global_index);
}

static u32 emit_reload_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot dst, BcSlot src)
{
	return emit_xy_bc(emitter, site, BC_RELOAD, unwrap_slot(dst), unwrap_slot(src));
}

static u32 emit_return_bc(BcFunctionEmitter *emitter, SourceSite site, BcSlot out, u32 nout)
{
	i32 slot_index = nout ? unwrap_slot(out) : 0;
	return emit_xy_bc(emitter, site, BC_RETURN, slot_index, nout);
}

static Ir check_ir_type(Ir ir, IrKind kind)
{
	ASSERT(ir);
	ASSERT(ir->kind == kind);
	return ir;
}

/* memory management */
static BcSlot mark_slots(BcFunctionEmitter *emitter)
{
	return (BcSlot){emitter->stack_top};
}

static void restore_slots(BcFunctionEmitter *emitter, BcSlot stack_top)
{
	emitter->stack_top = stack_top.slot;
}

static BcSlot alloc_slot(BcFunctionEmitter *emitter)
{
	BcSlot stack_top = {emitter->stack_top};
	emitter->stack_top += 1;
	if (emitter->stack_size < emitter->stack_top) {
		emitter->stack_size = emitter->stack_top;
	}
	return stack_top;
}

static BcSlot ensure_slot(BcFunctionEmitter *emitter, BcSlot slot)
{
	if (!bc_slot_is_valid(slot)) {
		slot = alloc_slot(emitter);
	}
	return slot;
}

/* main */
static BcModuleBuilder *create_module_builder(Compiler *compiler, u32 function_count)
{
	elf_Arena *arena = compiler->arena;
	if (function_count > BC_MODULE_OPERAND_COUNT_MAX) abort();
	elf_arena_align(arena, 8);
	BcModuleBuilder *module = elf_arena_push_zero(arena, sizeof(*module));
	module->compiler = compiler;
	module->arena = arena;

	if (function_count)
	{
		elf_arena_align(arena, 8);
		module->functions = elf_arena_push_zero(arena, sizeof(*module->functions) * function_count);
	}
	module->function_count = function_count;

	u32 bytecode_capacity = 1 << 13;
	elf_arena_align(arena, 8);
	module->bytecode = elf_arena_push_zero(arena, sizeof(*module->bytecode) * bytecode_capacity);
	module->bytecode_capacity = bytecode_capacity;
	module->bytecode_count = 0;
	return module;
}

static BcFunctionEmitter *create_function_emitter(BcModuleBuilder *module)
{
	ASSERT(module);
	elf_Arena *arena = module->arena;
	elf_arena_align(arena, 8);
	BcFunctionEmitter *emitter = elf_arena_push_zero(arena, sizeof(*emitter));
	emitter->module = module;
	u32 source_map_capacity = 1 << 13;
	elf_arena_align(arena, 8);
	emitter->source_map_buffer.entries = elf_arena_push_zero(arena, sizeof(*emitter->source_map_buffer.entries) * source_map_capacity);
	emitter->source_map_buffer.capacity = source_map_capacity;
	emitter->source_map_buffer.count = 0;
	return emitter;
}

static void emit_function(BcFunctionEmitter *emitter, IrFunction *function)
{
	ASSERT(emitter && emitter->module);
	ASSERT(function);
	BcModuleBuilder *module = emitter->module;

	PROF_BLOCK("bytecode.emit_function")
	{
		u32 bytecode_start = module->bytecode_count;
		emitter->source_map_buffer.count  =0;
		emitter->stack_size               =function->param_count;
		emitter->stack_top                =function->param_count;
		zero_memory(emitter->labels, sizeof(emitter->labels));
		emitter->label_count              = 0;
		emitter->jump_patch_count         = 0;

		Ir body = function->body;
		for (u32 i = 0; i < function->param_count; ++i)
		{
			Ir param = function->params[i];
			ASSERT(param && param->kind == IR_LOCAL);
			ASSERT(!bc_slot_is_valid(param->ir_local.slot));
			param->ir_local.slot = (BcSlot){(i32)i};
			if (param->type_rule != TRULE_NONE) {
				emit_xy_bc(emitter, param->site, BC_ENFORCE, unwrap_slot(param->ir_local.slot), param->type_rule);
			}
		}

		BcSlot memory_checkpoint = mark_slots(emitter);
		emit_stat(emitter, body);
		restore_slots(emitter, memory_checkpoint);

		if (module->bytecode_count == bytecode_start || BC_TYPE(module->bytecode[module->bytecode_count - 1]) != BC_RETURN)
		{
			emit_return_bc(emitter, body->site, NO_MEMORY, 0);
		}

		patch_bytecode_labels(emitter);

		ASSERT(module->generated_function_count < module->function_count);
		BcFunction *bc_function = module->functions + module->generated_function_count++;
		SourceMapEntry *source_map = copy_source_map(module->arena, emitter->source_map_buffer.entries, emitter->source_map_buffer.count);

		bc_function->variadic         = function->variadic;
		bc_function->arity            = function->param_count;
		bc_function->offset           = bytecode_start;
		bc_function->length           = module->bytecode_count - bytecode_start;
		bc_function->captures         = function->capture_count;
		bc_function->stack_size       = emitter->stack_size;
		bc_function->source_map       = source_map;
		bc_function->source_map_count = emitter->source_map_buffer.count;
	}
}

static SourceMapEntry *copy_source_map(elf_Arena *arena, SourceMapEntry *entries, u32 count)
{
	if (count == 0)
	{
		return 0;
	}

	elf_arena_align(arena, 8);
	SourceMapEntry *source_map = elf_arena_push(arena, sizeof(*source_map) * count);
	for (u32 i = 0; i < count; ++ i)
	{
		source_map[i] = entries[i];
	}
	return source_map;
}

static Com_String copy_compiled_string(elf_Arena *arena, const char *data, u32 size)
{
	char *copy = elf_arena_push(arena, size + 1);
	if (size) {
		copy_memory(copy, data, size);
	}
	copy[size] = 0;
	return (Com_String) {.data = copy, .size = size};
}

static Com_String copy_compiled_atom(elf_Arena *arena, Atom *atom)
{
	return copy_compiled_string(arena, atom_data(atom), atom_size(atom));
}

static u32 find_builder_atom(BcModuleBuilder *builder, Atom *atom)
{
	for (u32 i = 0; i < builder->atom_count; ++i)
	{
		if (builder->atoms[i] == atom) {
			return i;
		}
	}
	ASSERT(!"module global atom is missing from the string table");
	return 0;
}

static void finalize_module(Compiler *compiler, elf_Arena *output_arena, Com_Module *module, IrModule ir_module, BcModuleBuilder *builder)
{
	ASSERT(!module->bytecode && !module->functions);

	module->source_count = compiler->source_count;
	module->root_source = 0;
	if (module->source_count)
	{
		elf_arena_align(output_arena, 8);
		module->sources = elf_arena_push_zero(output_arena, sizeof(*module->sources) * module->source_count);
		for (u32 i = 0; i < module->source_count; ++i)
		{
			Com_Source *source = compiler->sources + i;
			Com_ModuleSource *output_source = module->sources + i;
			output_source->name = copy_compiled_atom(output_arena, source->name);
			output_source->canonical_path = copy_compiled_atom(output_arena, source->canonical_path);
			Com_String source_text = copy_compiled_string(output_arena, source->source.data, source->source.size);
			output_source->source = (SourceBuffer) {.data = source_text.data, .size = source_text.size};
		}
	}

	module->bytecode_count = builder->bytecode_count;
	if (module->bytecode_count)
	{
		elf_arena_align(output_arena, 8);
		module->bytecode = elf_arena_push_copy(output_arena, sizeof(*module->bytecode) * module->bytecode_count, builder->bytecode);
	}

	module->integer_constant_count = builder->integer_constant_count;
	if (module->integer_constant_count)
	{
		elf_arena_align(output_arena, 8);
		module->integer_constants = elf_arena_push_copy(output_arena, sizeof(*module->integer_constants) * module->integer_constant_count, builder->integer_constants);
	}

	module->number_constant_count = builder->number_constant_count;
	if (module->number_constant_count)
	{
		elf_arena_align(output_arena, 8);
		module->number_constants = elf_arena_push_copy(output_arena, sizeof(*module->number_constants) * module->number_constant_count, builder->number_constants);
	}

	module->string_count = builder->atom_count;
	if (module->string_count)
	{
		elf_arena_align(output_arena, 8);
		module->strings = elf_arena_push_zero(output_arena, sizeof(*module->strings) * module->string_count);
		for (u32 i = 0; i < module->string_count; ++i) {
			module->strings[i] = copy_compiled_atom(output_arena, builder->atoms[i]);
		}
	}

	module->global_count = ir_module.global_count;
	if (module->global_count)
	{
		module->globals = elf_arena_push_zero(output_arena, sizeof(*module->globals) * module->global_count);
		for (u32 i = 0; i < module->global_count; ++i) {
			module->globals[i].name = find_builder_atom(builder, ir_module.globals[i]);
		}
	}

	for (u32 i = 0; i < module->bytecode_count; ++i)
	{
		BytecodeType type = BC_TYPE(module->bytecode[i]);
		if (type == BC_GETGLOBAL || type == BC_SETGLOBAL) {
			module->global_relocation_count += 1;
		}
	}
	if (module->global_relocation_count)
	{
		module->global_relocations = elf_arena_push_zero(output_arena, sizeof(*module->global_relocations) * module->global_relocation_count);
		u32 relocation_index = 0;
		for (u32 i = 0; i < module->bytecode_count; ++i)
		{
			Bytecode bytecode = module->bytecode[i];
			BytecodeType type = BC_TYPE(bytecode);
			if (type == BC_GETGLOBAL || type == BC_SETGLOBAL)
			{
				u32 global_index = type == BC_GETGLOBAL ? (u32)BC_ARGY(bytecode) : (u32)BC_ARGX(bytecode);
				ASSERT(global_index < module->global_count);
				module->global_relocations[relocation_index++] = (Com_GlobalRelocation) {.bytecode_index = i, .global_index = global_index};
			}
		}
		ASSERT(relocation_index == module->global_relocation_count);
	}

	module->function_count = builder->function_count;
	if (module->function_count)
	{
		elf_arena_align(output_arena, 8);
		module->functions = elf_arena_push_copy(output_arena, sizeof(*module->functions) * module->function_count, builder->functions);
	}
	for (u32 i = 0; i < module->function_count; ++i)
	{
		BcFunction *function = module->functions + i;
		if (function->source_map_count)
		{
			elf_arena_align(output_arena, 8);
			function->source_map = elf_arena_push_copy(output_arena, sizeof(*function->source_map) * function->source_map_count, function->source_map);
		}
	}

	module->has_entry = ir_module.has_entry;
	module->entry_index = ir_module.entry_index;
}

static b32 emit_module(Compiler *compiler, elf_Arena *output_arena, Com_Module *module, IrModule ir_module)
{
	ASSERT(ir_module.functions || ir_module.function_count == 0);
	ASSERT(!ir_module.has_entry || ir_module.entry_index < ir_module.function_count);

	BcModuleBuilder *builder = create_module_builder(compiler, ir_module.function_count);
	BcFunctionEmitter *emitter = create_function_emitter(builder);
	PROF_BLOCK("compiler.emit")
	{
		for (u32 i = 0; i < ir_module.function_count; ++ i)
		{
			IrFunction *ir_function = ir_module.functions + i;
			emit_function(emitter, ir_function);
		}
	}
	ASSERT(builder->generated_function_count == builder->function_count);
	if (compiler->report.error_count) {
		return false;
	}
	for (u32 i = 0; i < ir_module.global_count; ++i)
	{
		add_atom(builder, ir_module.globals[i]);
	}
	finalize_module(compiler, output_arena, module, ir_module, builder);
	return true;
}

static BcSlot ensure_expr_memory(BcFunctionEmitter *emitter, Ir expr)
{
	BcSlot slot = NO_MEMORY;
	if (expr->kind == IR_LOAD_LOCAL) {
		Ir local = expr->ir_load_local.local;
		check_ir_type(local, IR_LOCAL);
		slot = local->ir_local.slot;
	}
	else {
		slot = emit_expr(emitter, expr, NO_MEMORY, 1);
	}

	ASSERT(bc_slot_is_valid(slot));
	return slot;
}

static BcSlot emit_new_table_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_x_bc(emitter, expr->site, BC_TABLE, unwrap_slot(out));
	return out;
}

static BcSlot emit_new_array_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
{
	if (nout < 1)
	{
		return out;
	}

	out = ensure_slot(emitter, out);
	emit_x_bc(emitter, expr->site, BC_ARRAY, unwrap_slot(out));
	return out;
}

static BcSlot finish_call(BcFunctionEmitter *emitter, Ir ir, BcSlot call_slot, BcSlot out, u32 nout)
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

	if (out.slot != call_slot.slot)
	{
		if (nout > 1) {
			report_emission_error(emitter, EMISSION_ERROR_GENERIC, ir->site, "multi-returns are not fully supported yet!");
		}

		emit_reload_bc(emitter, ir->site, out, call_slot);
	}

	return out;
}

static BcSlot emit_dynamic_call(BcFunctionEmitter *emitter, Ir ir, BcSlot out, u32 nout)
{
	ASSERT(ir->ir_call.type == IR_CALL_DYNAMIC);
	Ir expr = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;

	BcSlot stack_top = mark_slots(emitter);
	{
		BcSlot this_slot;
		BcSlot expr_slot;
		if (expr->kind == IR_META_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			expr_slot = emit_expr(emitter, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_MEMORY, 1);

			emit_xyz_bc(emitter, expr->site, BC_GETMETAFIELD
			,	unwrap_slot(expr_slot), unwrap_slot(this_slot), unwrap_slot(expr_slot));
		}
		else if (expr->kind == IR_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			expr_slot = emit_expr(emitter, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = emit_expr(emitter, expr->ir_binary.x, NO_MEMORY, 1);

			emit_xyz_bc(emitter, expr->site, BC_GETFIELD
			,	unwrap_slot(expr_slot), unwrap_slot(this_slot), unwrap_slot(expr_slot));
		}
		else
		{
			expr_slot = emit_expr(emitter, expr, NO_MEMORY, 1);
			this_slot = alloc_slot(emitter);

			emit_xy_bc(emitter, ir->site, BC_RELOAD, unwrap_slot(this_slot), 0);
		}

		ASSERT(expr_slot.slot == stack_top.slot + 0);
		ASSERT(this_slot.slot == stack_top.slot + 1);

		for (u32 i = 0; i < nargs; ++ i)
		{
			BcSlot arg_slot = emit_expr(emitter, args.items[i], NO_MEMORY, 1);
			ASSERT(arg_slot.slot == stack_top.slot + 2 + (i32)i);
		}
	}
	restore_slots(emitter, stack_top);

	emit_xyz_bc(emitter, expr->site, BC_DYNAMIC_CALL, unwrap_slot(stack_top), nargs + 1, nout);
	return finish_call(emitter, ir, stack_top, out, nout);
}

static BcSlot emit_direct_call(BcFunctionEmitter *emitter, Ir ir, BcSlot out, u32 nout)
{
	Ir function = ir->ir_call.expr;
	IrArray args = ir->ir_call.args;
	u32 nargs = args.count;
	ASSERT(ir->ir_call.type == IR_CALL_DIRECT);
	ASSERT(function->kind == IR_FUNCTION);
	ASSERT(function->ir_function.index < emitter->module->function_count);
	ASSERT(nout <= 0xff);

	BcSlot stack_top = mark_slots(emitter);
	{
		BcSlot call_slot = alloc_slot(emitter);
		ASSERT(call_slot.slot == stack_top.slot);
		BcSlot this_slot = alloc_slot(emitter);
		ASSERT(this_slot.slot == stack_top.slot + 1);
		emit_xy_bc(emitter, ir->site, BC_RELOAD, unwrap_slot(this_slot), 0);

		for (u32 i = 0; i < nargs; ++i)
		{
			BcSlot arg_slot = emit_expr(emitter, args.items[i], NO_MEMORY, 1);
			ASSERT(arg_slot.slot == stack_top.slot + 2 + (i32)i);
		}
	}
	restore_slots(emitter, stack_top);

	emit_xyzw_bc(emitter, ir->site, BC_CALL, unwrap_slot(stack_top), function->ir_function.index, nargs + 1, nout);
	return finish_call(emitter, ir, stack_top, out, nout);
}

static BytecodeType bc_op(IrKind type)
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

static BcSlot emit_binary_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	BcSlot memory_checkpoint = mark_slots(emitter);
	BcSlot left_slot = ensure_expr_memory(emitter, expr->ir_binary.x);
	BcSlot right_slot = ensure_expr_memory(emitter, expr->ir_binary.y);
	restore_slots(emitter, memory_checkpoint);

	out = ensure_slot(emitter, out);
	emit_xyz_bc(emitter, expr->site, bc_op(expr->kind)
	,	unwrap_slot(out), unwrap_slot(left_slot), unwrap_slot(right_slot));
	return out;
}

static BcSlot emit_unary_expr(BcFunctionEmitter *emitter, Ir expr, BytecodeType bytecode_type, BcSlot out, u32 nout)
{
	if (nout < 1) {
		return out;
	}

	BcSlot memory_checkpoint = mark_slots(emitter);
	BcSlot value_slot = ensure_expr_memory(emitter, expr->ir_unary);
	restore_slots(emitter, memory_checkpoint);

	out = ensure_slot(emitter, out);
	emit_xy_bc(emitter, expr->site, bytecode_type, unwrap_slot(out), unwrap_slot(value_slot));
	return out;
}

static BcSlot emit_truthy_or_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
{
	BcSlot memory_checkpoint;
	BcSlot temp_slot;
	u32 done_label;

	if (nout < 1) return out;

	out = emit_expr(emitter, expr->ir_binary.x, out, 1);
	memory_checkpoint = mark_slots(emitter);
	temp_slot = alloc_slot(emitter);
	emit_load_nil_bc(emitter, expr->site, temp_slot);
	emit_xyz_bc(emitter, expr->site, BC_EQ,	unwrap_slot(temp_slot), unwrap_slot(temp_slot), unwrap_slot(out));
	done_label = create_label(emitter);
	jump_if_false_slot_to_label(emitter, expr->site, temp_slot, done_label);
	restore_slots(emitter, memory_checkpoint);
	emit_expr(emitter, expr->ir_binary.y, out, 1);
	define_label(emitter, done_label);
	return out;
}

static BcSlot emit_logical_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
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

static void jump_if_true_slot_to_label(BcFunctionEmitter *emitter, SourceSite site, BcSlot pred, u32 label)
{
	u32 jump = emit_jump_if_bc(emitter, site, BC_JNZ, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_false_slot_to_label(BcFunctionEmitter *emitter, SourceSite site, BcSlot pred, u32 label)
{
	u32 jump = emit_jump_if_bc(emitter, site, BC_JZ, pred);
	record_jump_patch(emitter, jump, label);
}

static void jump_if_expr_false(BcFunctionEmitter *emitter, Ir expr, u32 false_label)
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
			BcSlot memory_checkpoint = mark_slots(emitter);
			BcSlot pred_slot = ensure_expr_memory(emitter, expr);
			jump_if_false_slot_to_label(emitter, expr->site, pred_slot, false_label);
			restore_slots(emitter, memory_checkpoint);
		}
		break;
	}
}

static void jump_if_expr_true(BcFunctionEmitter *emitter, Ir expr, u32 true_label)
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
			BcSlot memory_checkpoint = mark_slots(emitter);
			BcSlot pred_slot = ensure_expr_memory(emitter, expr);
			jump_if_true_slot_to_label(emitter, expr->site, pred_slot, true_label);
			restore_slots(emitter, memory_checkpoint);
		}
		break;
	}
}

static BcSlot do_expr_block(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
{
	IrArray stats = expr->ir_expr_block.stats;
	Ir value = expr->ir_expr_block.value;

	if (nout < 1)
	{
		BcSlot memory_checkpoint = mark_slots(emitter);
		for (u32 i = 0; i < stats.count; ++ i) {
			emit_stat(emitter, stats.items[i]);
		}
		restore_slots(emitter, memory_checkpoint);
		return out;
	}

	out = ensure_slot(emitter, out);
	BcSlot memory_checkpoint = mark_slots(emitter);
	for (u32 i = 0; i < stats.count; ++ i) {
		emit_stat(emitter, stats.items[i]);
	}
	BcSlot final_slot = emit_expr(emitter, value, out, 1);
	ASSERT(final_slot.slot == out.slot);
	restore_slots(emitter, memory_checkpoint);
	return out;
}

static BcSlot emit_expr(BcFunctionEmitter *emitter, Ir expr, BcSlot out, u32 nout)
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

			u32 bytecode_function_id = expr->ir_function.index;
			ASSERT(bytecode_function_id < emitter->module->function_count);

			IrArray captures = expr->ir_function.captures;
			if (captures.count == 0)
			{
				emit_xy_bc(emitter, site, BC_CLOSURE, unwrap_slot(out), bytecode_function_id);
			}
			else
			{
				BcSlot memory_checkpoint = mark_slots(emitter);
				BcSlot first_capture_slot = memory_checkpoint;

				for (u32 i = 0; i < captures.count; ++ i)
				{
					BcSlot next_capture_slot = emit_expr(emitter, captures.items[i], NO_MEMORY, 1);
					ASSERT(bc_slot_is_valid(next_capture_slot));
					ASSERT(first_capture_slot.slot + i == next_capture_slot.slot);
				}

				emit_xy_bc(emitter, site, BC_CLOSURE, unwrap_slot(first_capture_slot), bytecode_function_id);
				if (first_capture_slot.slot != out.slot) {
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
			emit_xy_bc(emitter, site, BC_LOADCVAL, unwrap_slot(out), expr->ir_capture);
		}
		break;

		case IR_RECURSE:
		{
			if (nout < 1) {
				return out;
			}

			out = ensure_slot(emitter, out);
			emit_xy_bc(emitter, site, BC_CURRENT_CLOSURE, unwrap_slot(out), 0);
		}
		break;

		case IR_LOAD_GLOBAL:
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

			emit_load_int_bc(emitter, expr->site, out, unwrap_slot(local->ir_local.slot));
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

		case IR_LENGTH_INTRINSIC:
		{
			out = emit_unary_expr(emitter, expr, BC_GETLENGTH, out, nout);
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
			report_emission_error(emitter, EMISSION_ERROR_INVALID_EXPRESSION, expr->site
			, "invalid expression, got: %s", ir_kind_name(expr->kind));

			ASSERT(!"Error!");
		}
		break;
	}

	return out;
}

static void emit_store(BcFunctionEmitter *emitter, SourceSite site, Ir dest, Ir expr)
{
	BcSlot memory_checkpoint = mark_slots(emitter);
	{
		switch (dest->kind)
		{
			case IR_LOAD_LOCAL:
			{
				Ir local = dest->ir_load_local.local;
				check_ir_type(local, IR_LOCAL);

				BcSlot dest_slot = local->ir_local.slot;
				ASSERT(bc_slot_is_valid(dest_slot));

				BcSlot final_slot = emit_expr(emitter, expr, dest_slot, 1);
				ASSERT(final_slot.slot == dest_slot.slot);
			}
			break;

			case IR_LOAD_GLOBAL:
			{
				u32 dest_slot = dest->ir_global;

				BcSlot expr_slot = ensure_expr_memory(emitter, expr);
				emit_xy_bc(emitter, dest->site, BC_SETGLOBAL, dest_slot, unwrap_slot(expr_slot));
			}
			break;

			case IR_FIELD:
			{
				BcSlot value_slot = ensure_expr_memory(emitter, expr);
				BcSlot object_slot = ensure_expr_memory(emitter, dest->ir_binary.x);
				BcSlot field_slot = ensure_expr_memory(emitter, dest->ir_binary.y);

				emit_xyz_bc(emitter, site, BC_SETFIELD
				,	unwrap_slot(object_slot), unwrap_slot(field_slot), unwrap_slot(value_slot));
			}
			break;

			case IR_INDEX:
			{
				BcSlot value_slot = ensure_expr_memory(emitter, expr);
				BcSlot array_slot = ensure_expr_memory(emitter, dest->ir_binary.x);
				BcSlot index_slot = ensure_expr_memory(emitter, dest->ir_binary.y);
				emit_xyz_bc(emitter, site, BC_SETINDEX
				,	unwrap_slot(array_slot), unwrap_slot(index_slot), unwrap_slot(value_slot));
			}
			break;

			default:
			{
				report_emission_error(emitter, EMISSION_ERROR_INVALID_LVALUE, dest->site
				, "invalid l-value, got: %s", ir_kind_name(dest->kind));
			}
			break;
		}
	}
	restore_slots(emitter, memory_checkpoint);
}

static void emit_stat(BcFunctionEmitter *emitter, Ir stat)
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
				slot = emit_expr(emitter, stat->ir_local.expr, NO_MEMORY, 1);
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
			BcSlot first = emit_expr(emitter, stat->ir_multi_local.expr, NO_MEMORY, locals.count);
			ASSERT(bc_slot_is_valid(first));
			for (u32 i = 0; i < locals.count; ++i)
			{
				Ir local = locals.items[i];
				ASSERT(local && local->kind == IR_LOCAL);
				ASSERT(!bc_slot_is_valid(local->ir_local.slot));
				local->ir_local.slot = (BcSlot){ first.slot + (i32)i };
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
			BcSlot memory_checkpoint = mark_slots(emitter);
			BcSlot array_slot = ensure_expr_memory(emitter, stat->ir_binary.x);
			BcSlot value_slot = ensure_expr_memory(emitter, stat->ir_binary.y);
			emit_xy_bc(emitter, stat->site, BC_ARRAYADD, unwrap_slot(array_slot), unwrap_slot(value_slot));
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

			BcSlot memory_checkpoint = mark_slots(emitter);
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
				BcSlot true_memory = mark_slots(emitter);
				emit_stat(emitter, true_clause);
				restore_slots(emitter, true_memory);
			}

			if (else_clause)
			{
				u32 end_label = create_label(emitter);
				jump_to_label(emitter, stat->site, end_label);

				BcSlot else_memory = mark_slots(emitter);
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
				BcSlot slot = ensure_expr_memory(emitter, expr);
				emit_return_bc(emitter, stat->site, slot, 1);
			}
			else
			{
				emit_return_bc(emitter, stat->site, NO_MEMORY, 0);
			}
		}
		break;

		default:
		{
			BcSlot memory_checkpoint = mark_slots(emitter);
			emit_expr(emitter, stat, NO_MEMORY, 0);
			restore_slots(emitter, memory_checkpoint);
		}
		break;
	}
}

static void define_label(BcFunctionEmitter *emitter, u32 label)
{
	BytecodeLabel *target = get_bytecode_label(emitter, label);
	ASSERT(!target->defined);

	target->defined = true;
	target->position = emitter->module->bytecode_count;
}

static void jump_to_label(BcFunctionEmitter *emitter, SourceSite site, u32 label)
{
	u32 jump = emit_jump_bc(emitter, site);
	record_jump_patch(emitter, jump, label);
}
