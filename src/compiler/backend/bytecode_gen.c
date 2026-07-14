static GenMemory generate_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots);
static void generate_ir_stat(BytecodeGen *gen, IR stat);
static void jump_list_push(JumpList *list, int jump);
static void jump_list_clear(JumpList *list);
static void define_bytecode_label(BytecodeGen *gen, u32 label);
static void emit_jump_to_label(BytecodeGen *gen, SourceSite site, u32 label);
static void emit_jump_if_false_to_label(BytecodeGen *gen, SourceSite site, IR pred, u32 label);
static void emit_condition_branch(BytecodeGen *gen, jumpS *js, b32 if_true, IR expr);

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

static void report_generation_error(BytecodeGen *gen, GenerationError error, SourceSite site, const char *format, ...)
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

static void check_ir_type(IR ir, IRKind kind)
{
	ASSERT(ir);
	ASSERT(ir->kind == kind);
}

static GenMemory allocate_slots(BytecodeGen *gen, i32 nslots)
{
	ASSERT(nslots != 0);
	ASSERT(gen->memory.slot + nslots < ARRAY_COUNT(gen->memory_slots));
	GenMemory mem = gen->memory;
	gen->memory.slot += nslots;
	if (gen->memory_usage.slot < gen->memory.slot) {
		gen->memory_usage = gen->memory;
	}
	return mem;
}

static GenMemory allocate_slot(BytecodeGen *gen)
{
	return allocate_slots(gen, 1);
}

static GenMemory ensure_result_slot(BytecodeGen *gen, GenMemory slot)
{
	if (!gen_memory_is_valid(slot)) {
		slot = allocate_slot(gen);
	}
	return slot;
}

static GenMemory gen_memory_save(BytecodeGen *gen)
{
	return gen->memory;
}

static void gen_memory_restore(BytecodeGen *gen, GenMemory state)
{
	i32 prev_memory = gen->memory.slot;
	gen->memory = state;

	ASSERT(prev_memory >= gen->memory.slot);

	for (i32 i = prev_memory - 1; i >= gen->memory.slot; -- i) {
		gen->memory_slots[i] = 0;
	}
}

static BytecodeGen *allocate_bytecode_function_generator(elf_State *state, Arena *arena, u32 bytecode_function_base)
{
	BytecodeGen *gen = arena_push_zero(arena, sizeof(*gen));
	gen->state = state;
	gen->arena = arena;
	gen->bytecode_function_base = bytecode_function_base;

	u32 bytecode_buffer_capacity = 1 << 13;
	gen->bytecode_buffer.bytecode = arena_push_zero(arena, sizeof(*gen->bytecode_buffer.bytecode) * bytecode_buffer_capacity);
	gen->bytecode_buffer.capacity = bytecode_buffer_capacity;
	gen->bytecode_buffer.position = 0;

	u32 source_map_capacity = 1 << 13;
	gen->source_map_buffer.entries = arena_push_zero(arena, sizeof(*gen->source_map_buffer.entries) * source_map_capacity);
	gen->source_map_buffer.capacity = source_map_capacity;
	gen->source_map_buffer.count = 0;
	return gen;
}

static void generate_bytecode_function(BytecodeGen *gen, FunctionIR function)
{
	gen->bytecode_buffer.position = 0;
	gen->source_map_buffer.count  = 0;
	gen->memory_usage             = (GenMemory) { (i32)function.arity };
	gen->memory                   = (GenMemory) { (i32)function.arity };
	zero_memory(gen->labels, sizeof(gen->labels));

	IR body = function.body;
	ASSERT(gen->bytecode_buffer.position == 0);

	GenMemory memory_checkpoint = gen_memory_save(gen);
	generate_ir_stat(gen, body);
	gen_memory_restore(gen, memory_checkpoint);

	if (gen->bytecode_buffer.position == 0 ||
		BC_TYPE(gen->bytecode_buffer.bytecode[gen->bytecode_buffer.position - 1]) != BC_RETURN)
	{
		emit_return_bytecode(gen, body->site, NO_MEMORY, 0);
	}
}

static GenMemory expr_to_any_mem(BytecodeGen *gen, IR expr)
{
	GenMemory slot = NO_MEMORY;
	if (expr->kind == IR_LOAD_LOCAL) {
		IR local = expr->ir_load_local.local;
		check_ir_type(local, IR_LOCAL);
		slot = local->ir_local.slot;
	}
	else {
		slot = generate_ir_expr(gen, expr, NO_MEMORY, 1);
	}

	ASSERT(gen_memory_is_valid(slot));
	return slot;
}

static GenMemory generate_call_expr(BytecodeGen *gen, IR ir, GenMemory slots, u32 nslots)
{
	IR expr = ir->ir_call.expr;
	IR_Array args = ir->ir_call.args;
	u32 nargs = args.count;

	GenMemory call_slot = gen->memory;
	GenMemory memory_checkpoint = gen_memory_save(gen);
	{
		GenMemory this_slot;
		GenMemory callee_slot;
		if (expr->kind == IR_META_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			callee_slot = generate_ir_expr(gen, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = generate_ir_expr(gen, expr->ir_binary.x, NO_MEMORY, 1);

			emit_bytexyz(gen, expr->site, BC_GETMETAFIELD
			,	gen_memory_index(callee_slot), gen_memory_index(this_slot), gen_memory_index(callee_slot));
		}
		else if (expr->kind == IR_FIELD)
		{
			check_ir_type(expr->ir_binary.y, IR_ATOM);

			callee_slot = generate_ir_expr(gen, expr->ir_binary.y, NO_MEMORY, 1);
			this_slot = generate_ir_expr(gen, expr->ir_binary.x, NO_MEMORY, 1);

			emit_bytexyz(gen, expr->site, BC_GETFIELD
			,	gen_memory_index(callee_slot), gen_memory_index(this_slot), gen_memory_index(callee_slot));
		}
		else
		{
			callee_slot = generate_ir_expr(gen, expr, NO_MEMORY, 1);
			this_slot = allocate_slot(gen);

			emit_bytexy(gen, ir->site, BC_RELOAD, gen_memory_index(this_slot), 0);
		}

		ASSERT(callee_slot.slot == call_slot.slot + 0);
		ASSERT(this_slot.slot == call_slot.slot + 1);

		for (u32 i = 0; i < nargs; ++ i)
		{
			GenMemory arg_slot = generate_ir_expr(gen, args.items[i], NO_MEMORY, 1);
			ASSERT(arg_slot.slot == call_slot.slot + 2 + (i32)i);
		}
	}
	gen_memory_restore(gen, memory_checkpoint);

	emit_bytexyz(gen, expr->site, BC_CALL, gen_memory_index(call_slot), nargs + 1, nslots);

	if (nslots < 1) {
		return slots;
	}

	slots = ensure_result_slot(gen, slots);

	if (slots.slot != call_slot.slot)
	{
		if (nslots > 1)
		{
			report_generation_error(gen, GENERATION_ERROR_GENERIC, expr->site, "multi-returns are not fully supported yet!");
		}

		emit_reload_bytecode(gen, expr->site, slots, call_slot);
	}

	return slots;
}

static u32 bytecode_type_from_ir_kind(IRKind type)
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
		default: NO_CODE;
	}

	ASSERT(!"Error");
	return BC_HALT;
}

static GenMemory generate_binary_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	GenMemory memory_checkpoint = gen_memory_save(gen);
	GenMemory left_slot = expr_to_any_mem(gen, expr->ir_binary.x);
	GenMemory right_slot = expr_to_any_mem(gen, expr->ir_binary.y);
	gen_memory_restore(gen, memory_checkpoint);

	slots = ensure_result_slot(gen, slots);
	emit_bytexyz(gen, expr->site, bytecode_type_from_ir_kind(expr->kind)
	,	gen_memory_index(slots), gen_memory_index(left_slot), gen_memory_index(right_slot));
	return slots;
}

static GenMemory generate_unary_ir_expr(BytecodeGen *gen, IR expr, u32 bytecode_type, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	GenMemory memory_checkpoint = gen_memory_save(gen);
	GenMemory value_slot = expr_to_any_mem(gen, expr->ir_unary);
	gen_memory_restore(gen, memory_checkpoint);

	slots = ensure_result_slot(gen, slots);
	emit_bytexy(gen, expr->site, bytecode_type, gen_memory_index(slots), gen_memory_index(value_slot));
	return slots;
}

static GenMemory generate_index_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	GenMemory memory_checkpoint = gen_memory_save(gen);
	GenMemory array_slot = expr_to_any_mem(gen, expr->ir_binary.x);
	GenMemory index_slot = expr_to_any_mem(gen, expr->ir_binary.y);
	gen_memory_restore(gen, memory_checkpoint);

	slots = ensure_result_slot(gen, slots);
	emit_bytexyz(gen, expr->site, BC_GETINDEX
	,	gen_memory_index(slots), gen_memory_index(array_slot), gen_memory_index(index_slot));
	return slots;
}

static GenMemory generate_field_ir_expr(BytecodeGen *gen, IR expr, u32 bytecode_type, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	GenMemory memory_checkpoint = gen_memory_save(gen);
	GenMemory object_slot = expr_to_any_mem(gen, expr->ir_binary.x);
	GenMemory field_slot = expr_to_any_mem(gen, expr->ir_binary.y);
	gen_memory_restore(gen, memory_checkpoint);

	slots = ensure_result_slot(gen, slots);
	emit_bytexyz(gen, expr->site, bytecode_type
	,	gen_memory_index(slots), gen_memory_index(object_slot), gen_memory_index(field_slot));
	return slots;
}

static GenMemory generate_nil_or_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	slots = ensure_result_slot(gen, slots);
	generate_ir_expr(gen, expr->ir_binary.x, slots, 1);

	GenMemory memory_checkpoint = gen_memory_save(gen);
	GenMemory nil_slot = allocate_slot(gen);
	GenMemory eq_nil_slot = allocate_slot(gen);
	emit_load_nil(gen, expr->site, nil_slot);
	emit_bytexyz(gen, expr->site, BC_EQ
	,	gen_memory_index(eq_nil_slot), gen_memory_index(slots), gen_memory_index(nil_slot));

	u32 skip_right = emit_bytexy(gen, expr->site, BC_JZ, NO_JUMP, gen_memory_index(eq_nil_slot));
	gen_memory_restore(gen, memory_checkpoint);

	generate_ir_expr(gen, expr->ir_binary.y, slots, 1);

	patch_jump(gen, skip_right);
	return slots;
}

static GenMemory generate_table_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	if (nslots < 1) {
		return slots;
	}

	slots = ensure_result_slot(gen, slots);
	emit_bytexy(gen, expr->site, BC_TABLE, gen_memory_index(slots), 0);
	return slots;
}

static GenMemory generate_expr_block_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	IR_Array stats = expr->ir_expr_block.stats;
	IR value = expr->ir_expr_block.value;

	if (nslots < 1)
	{
		GenMemory memory_checkpoint = gen_memory_save(gen);
		for (u32 i = 0; i < stats.count; ++ i) {
			generate_ir_stat(gen, stats.items[i]);
		}
		gen_memory_restore(gen, memory_checkpoint);
		return slots;
	}

	slots = ensure_result_slot(gen, slots);
	GenMemory memory_checkpoint = gen_memory_save(gen);
	for (u32 i = 0; i < stats.count; ++ i) {
		generate_ir_stat(gen, stats.items[i]);
	}
	GenMemory final_slot = generate_ir_expr(gen, value, slots, 1);
	ASSERT(final_slot.slot == slots.slot);
	gen_memory_restore(gen, memory_checkpoint);
	return slots;
}

static GenMemory generate_ir_expr(BytecodeGen *gen, IR expr, GenMemory slots, u32 nslots)
{
	SourceSite site = expr->site;
	switch (expr->kind)
	{
		case IR_ERROR:
		{
			return slots;
		}

		case IR_FUNCTION:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			u32 bytecode_function_id = gen->bytecode_function_base + expr->ir_function;
			emit_bytexy(gen, site, BC_CLOSURE, gen_memory_index(slots), bytecode_function_id);
		}
		break;

		case IR_RECURSE:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_bytexy(gen, site, BC_CURRENT_CLOSURE, gen_memory_index(slots), 0);
		}
		break;

		case IR_LOAD_GLOBAL:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_get_global(gen, expr->site, slots, expr->ir_global);
		}
		break;

		case IR_LOAD_LOCAL:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			IR local = expr->ir_load_local.local;
			check_ir_type(local, IR_LOCAL);
			ASSERT(gen_memory_is_valid(local->ir_local.slot));

			emit_reload_bytecode(gen, expr->site, slots, local->ir_local.slot);
		}
		break;

		case IR_GET_MEM:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			IR target = expr->ir_unary;
			check_ir_type(target, IR_LOAD_LOCAL);

			IR local = target->ir_load_local.local;
			check_ir_type(local, IR_LOCAL);
			ASSERT(gen_memory_is_valid(local->ir_local.slot));

			emit_load_constant_int(gen, expr->site, slots, gen_memory_index(local->ir_local.slot));
		}
		break;

		case IR_INTEGER:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_load_constant_int(gen, expr->site, slots, expr->ir_int);
		}
		break;

		case IR_NUMBER:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_load_constant_num(gen, expr->site, slots, expr->ir_num);
		}
		break;

		case IR_ATOM:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_load_constant_atom(gen, expr->site, slots, expr->atom);
		}
		break;

		case IR_NIL:
		{
			if (nslots < 1) {
				return slots;
			}

			slots = ensure_result_slot(gen, slots);
			emit_load_nil(gen, expr->site, slots);
		}
		break;

		case IR_CALL:
		{
			slots = generate_call_expr(gen, expr, slots, nslots);
		}
		break;

		case IR_INDEX:
		{
			slots = generate_index_ir_expr(gen, expr, slots, nslots);
		}
		break;

		case IR_FIELD:
		{
			slots = generate_field_ir_expr(gen, expr, BC_GETFIELD, slots, nslots);
		}
		break;

		case IR_META_FIELD:
		{
			slots = generate_field_ir_expr(gen, expr, BC_GETMETAFIELD, slots, nslots);
		}
		break;

		case IR_TABLE:
		{
			slots = generate_table_ir_expr(gen, expr, slots, nslots);
		}
		break;

		case IR_EXPR_BLOCK:
		{
			slots = generate_expr_block_ir_expr(gen, expr, slots, nslots);
		}
		break;

		case IR_LENGTH_INTRINSIC:
		{
			slots = generate_unary_ir_expr(gen, expr, BC_GETLENGTH, slots, nslots);
		}
		break;

		case IR_NIL_OR:
		{
			slots = generate_nil_or_ir_expr(gen, expr, slots, nslots);
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
		case IR_BITWISE_XOR:
		case IR_BITWISE_AND:
		case IR_BITWISE_OR:
		{
			slots = generate_binary_ir_expr(gen, expr, slots, nslots);
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

	return slots;
}

static void generate_store(BytecodeGen *gen, SourceSite site, IR dest, IR expr)
{
	GenMemory memory_checkpoint = gen_memory_save(gen);
	{
		switch (dest->kind)
		{
			case IR_LOAD_LOCAL:
			{
				IR local = dest->ir_load_local.local;
				check_ir_type(local, IR_LOCAL);

				GenMemory dest_slot = local->ir_local.slot;
				ASSERT(gen_memory_is_valid(dest_slot));

				GenMemory final_slot = generate_ir_expr(gen, expr, dest_slot, 1);
				ASSERT(final_slot.slot == dest_slot.slot);
			}
			break;

			case IR_LOAD_GLOBAL:
			{
				u32 dest_slot = dest->ir_global;

				GenMemory expr_slot = expr_to_any_mem(gen, expr);
				emit_bytexy(gen, dest->site, BC_SETGLOBAL, dest_slot, gen_memory_index(expr_slot));
			}
			break;

			case IR_FIELD:
			{
				GenMemory value_slot = expr_to_any_mem(gen, expr);
				GenMemory object_slot = expr_to_any_mem(gen, dest->ir_binary.x);
				GenMemory field_slot = expr_to_any_mem(gen, dest->ir_binary.y);

				emit_bytexyz(gen, site, BC_SETFIELD
				,	gen_memory_index(object_slot), gen_memory_index(field_slot), gen_memory_index(value_slot));
			}
			break;

			case IR_INDEX:
			{
				GenMemory value_slot = expr_to_any_mem(gen, expr);
				GenMemory array_slot = expr_to_any_mem(gen, dest->ir_binary.x);
				GenMemory index_slot = expr_to_any_mem(gen, dest->ir_binary.y);
				emit_bytexyz(gen, site, BC_SETINDEX
				,	gen_memory_index(array_slot), gen_memory_index(index_slot), gen_memory_index(value_slot));
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
	gen_memory_restore(gen, memory_checkpoint);
}

static void generate_ir_stat(BytecodeGen *gen, IR stat)
{
	switch (stat->kind)
	{
		case IR_ERROR:
		{
			return;
		}

		case IR_LOCAL:
		{
			GenMemory slot = stat->ir_local.slot;
			if (gen_memory_is_valid(slot))
			{
				if (stat->ir_local.expr) {
					GenMemory final_slot = generate_ir_expr(gen, stat->ir_local.expr, slot, 1);
					ASSERT(final_slot.slot == slot.slot);
				}
			}
			else if (stat->ir_local.expr)
			{
				slot = generate_ir_expr(gen, stat->ir_local.expr, NO_MEMORY, 1);
				ASSERT(gen_memory_is_valid(slot));
			}
			else
			{
				slot = allocate_slot(gen);
			}

			stat->ir_local.slot = slot;
			gen->memory_slots[gen_memory_index(slot)] = stat;
		}
		break;

		case IR_STORE:
		{
			generate_store(gen, stat->site, stat->ir_binary.x, stat->ir_binary.y);
		}
		break;

		case IR_ARRAY_ADD:
		{
			GenMemory memory_checkpoint = gen_memory_save(gen);
			GenMemory table_slot = expr_to_any_mem(gen, stat->ir_binary.x);
			GenMemory value_slot = expr_to_any_mem(gen, stat->ir_binary.y);
			emit_bytexy(gen, stat->site, BC_ARRAYADD, gen_memory_index(table_slot), gen_memory_index(value_slot));
			gen_memory_restore(gen, memory_checkpoint);
		}
		break;

		case IR_LABEL:
		{
			define_bytecode_label(gen, stat->ir_label);
		}
		break;

		case IR_JUMP:
		{
			emit_jump_to_label(gen, stat->site, stat->ir_label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			emit_jump_if_false_to_label(gen, stat->site, stat->ir_jump_if_false.pred, stat->ir_jump_if_false.label);
		}
		break;

		case IR_BLOCK:
		{
			IR_Array stats = stat->ir_block.stats;

			GenMemory memory_checkpoint = gen_memory_save(gen);
			{
				for (u32 i = 0; i < stats.count; ++ i)
				{
					generate_ir_stat(gen, stats.items[i]);
				}
			}
			gen_memory_restore(gen, memory_checkpoint);
		}
		break;

		case IR_IF:
		{
			IR pred = stat->ir_if.pred;
			IR true_clause = stat->ir_if.true_clause;
			IR else_clause = stat->ir_if.else_clause;

			JBuf s = {};
			begin_if(gen, stat->site, &s, pred, 0);

			GenMemory true_memory = gen_memory_save(gen);
			generate_ir_stat(gen, true_clause);
			gen_memory_restore(gen, true_memory);

			if (else_clause)
			{
				GenMemory else_memory = gen_memory_save(gen);
				ASSERT(s.jz.count != 0);

				int else_exit = emit_jump(gen, stat->site, -1);
				jump_list_push(&s.j, else_exit);

				patch_jumps(gen, &s.jz);
				jump_list_clear(&s.jz);

				gen_memory_restore(gen, else_memory);
				generate_ir_stat(gen, else_clause);
			}

			close_if(gen, stat->site, &s);
		}
		break;

		case IR_RETURN:
		{
			IR expr = stat->ir_return.expr;
			if (expr)
			{
				GenMemory slot = expr_to_any_mem(gen, expr);
				emit_return_bytecode(gen, stat->site, slot, 1);
			}
			else
			{
				emit_return_bytecode(gen, stat->site, NO_MEMORY, 0);
			}
		}
		break;

		default:
		{
			GenMemory memory_checkpoint = gen_memory_save(gen);
			generate_ir_expr(gen, stat, NO_MEMORY, 0);
			gen_memory_restore(gen, memory_checkpoint);
		}
		break;
	}
}

static void jump_list_push(JumpList *list, int jump)
{
	ASSERT(list->count < ARRAY_COUNT(list->jumps));
	list->jumps[list->count ++] = jump;
}

static void jump_list_clear(JumpList *list)
{
	list->count = 0;
}

static BytecodeLabel *get_bytecode_label(BytecodeGen *gen, u32 label)
{
	ASSERT(label < ARRAY_COUNT(gen->labels));
	return &gen->labels[label];
}

static void define_bytecode_label(BytecodeGen *gen, u32 label)
{
	BytecodeLabel *target = get_bytecode_label(gen, label);
	ASSERT(!target->defined);

	target->defined = true;
	target->position = gen->bytecode_buffer.position;
	patch_jumps2(gen, &target->pending, target->position);
	jump_list_clear(&target->pending);
}

static void patch_or_defer_label_jump(BytecodeGen *gen, u32 label, int jump)
{
	BytecodeLabel *target = get_bytecode_label(gen, label);
	if (target->defined) {
		patch_jump2(gen, jump, target->position);
	}
	else {
		jump_list_push(&target->pending, jump);
	}
}

static void emit_jump_to_label(BytecodeGen *gen, SourceSite site, u32 label)
{
	int jump = emit_jump(gen, site, -1);
	patch_or_defer_label_jump(gen, label, jump);
}

static void emit_jump_if_false_to_label(BytecodeGen *gen, SourceSite site, IR pred, u32 label)
{
	jumpS jumps = {};
	emit_condition_branch(gen, &jumps, 0, pred);

	patch_jumps(gen, &jumps.t);
	jump_list_clear(&jumps.t);

	for (u32 i = 0; i < jumps.f.count; ++ i) {
		patch_or_defer_label_jump(gen, label, jumps.f.jumps[i]);
	}
	jump_list_clear(&jumps.f);
}

static void emit_condition_branch(BytecodeGen *gen, jumpS *js, b32 if_true, IR expr)
{
	switch (expr->kind)
	{
		case IR_AND:
		{
			emit_condition_branch(gen, js, 0, expr->ir_binary.x);
			patch_jumps(gen, &js->t);
			jump_list_clear(&js->t);
			emit_condition_branch(gen, js, if_true, expr->ir_binary.y);
		}
		break;

		case IR_OR:
		{
			emit_condition_branch(gen, js, 1, expr->ir_binary.x);
			patch_jumps(gen, &js->f);
			jump_list_clear(&js->f);
			emit_condition_branch(gen, js, if_true, expr->ir_binary.y);
		}
		break;

		default:
		{
			GenMemory memory_checkpoint = gen_memory_save(gen);
			{
				GenMemory pred_slot = expr_to_any_mem(gen, expr);
				if (if_true)
				{
					u32 jmp = emit_bytexy(gen, expr->site, BC_JNZ, NO_JUMP, gen_memory_index(pred_slot));
					jump_list_push(&js->t, jmp);
				}
				else
				{
					u32 jmp = emit_bytexy(gen, expr->site, BC_JZ, NO_JUMP, gen_memory_index(pred_slot));
					jump_list_push(&js->f, jmp);
				}
			}
			gen_memory_restore(gen, memory_checkpoint);
		} break;
	}
}

static void begin_if(BytecodeGen *gen, SourceSite site, JBuf *jumps, IR pred, int if_true)
{
	jumpS js = {0};
	emit_condition_branch(gen, &js, if_true, pred);

	if (if_true)
	{
		ASSERT(js.t.count != 0);
		patch_jumps(gen, &js.f);
		jump_list_clear(&js.f);
		jumps->jz = js.t;
	}
	else
	{
		ASSERT(js.f.count != 0);
		patch_jumps(gen, &js.t);
		jump_list_clear(&js.t);
		jumps->jz = js.f;
	}
}

static void close_if(BytecodeGen *gen, SourceSite site, JBuf *jumps)
{
	if (jumps->jz.count != 0) {
		patch_jumps(gen, &jumps->jz);
		jump_list_clear(&jumps->jz);
	}
	if (jumps->j.count != 0) {
		patch_jumps(gen, &jumps->j);
		jump_list_clear(&jumps->j);
	}
}
