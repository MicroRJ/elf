typedef struct
{
	LowerContext *ctx;
	u32           start;
	u32           count;
}
IRArrayBuilder;

static void lower_ast_stat_to_ir(LowerContext *ctx, IRArrayBuilder *block, AstRef stat);
static Ir lower_ast_expr_to_ir(LowerContext *ctx, AstRef expr);
static Ir lower_ast_to_ir_block(LowerContext *ctx, AstRef stat);

static LowerContext *elf_create_lower_context(elf_State *state, Arena *arena)
{
	LowerContext *ctx = arena_push_zero(arena, sizeof(*ctx));
	ctx->arena = arena;
	ctx->state = state;

	u32 max_functions = 1024;
	ctx->functions = arena_push_zero(arena, sizeof(*ctx->functions) * max_functions);
	ctx->max_functions = max_functions;

	u32 ir_stack_size = 4096;
	ctx->ir_stack = arena_push_zero(arena, sizeof(*ctx->ir_stack) * ir_stack_size);
	ctx->ir_stack_size = ir_stack_size;
	ctx->ir_stack_index = 0;

	u32 defer_stack_size = 4096;
	ctx->defer_stack = arena_push_zero(arena, sizeof(*ctx->defer_stack) * defer_stack_size);
	ctx->defer_stack_size = defer_stack_size;
	ctx->defer_count = 0;
	ctx->defer_scope_start = 0;

	return ctx;
}

typedef enum
{
	LOWERING_ERROR_GENERIC = 0,
	LOWERING_ERROR_INTERNAL,
	LOWERING_ERROR_UNREFERENCED_ENTITY,
	LOWERING_ERROR_UNDECLARED_IDENTIFIER,
}
LoweringError;

static void report_lowering_error(LowerContext *ctx, LoweringError error, SourceSite site, const char *format, ...)
{
	(void)error;

	va_list args;
	va_start(args, format);
	Scratch scratch = get_scratch();
	char *message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1);

	const char *source_name = ctx && ctx->source_name ? elf_atom_data(ctx->source_name) : "<unknown>";
	if (site.line_index) {
		log_linef(LOG_LEVEL_ERROR, "%s [%u:%llu] lowering error: %s"
		, source_name, site.line_index, source_slice_column(site), message);
	}
	else {
		log_linef(LOG_LEVEL_ERROR, "%s [?] lowering error: %s", source_name, message);
	}
	end_scratch(scratch);
}

static void report_lowering_warning(LowerContext *ctx, LoweringError error, SourceSite site, const char *format, ...)
{
	(void)error;

	va_list args;
	va_start(args, format);
	Scratch scratch = get_scratch();
	char *message = arena_pushfv(scratch.arena, format, args);
	va_end(args);
	arena_push_zero(scratch.arena, 1);

	const char *source_name = ctx && ctx->source_name ? elf_atom_data(ctx->source_name) : "<unknown>";
	if (site.line_index) {
		log_linef(LOG_LEVEL_WARNING, "%s [%u:%llu] lowering warning: %s"
		, source_name, site.line_index, source_slice_column(site), message);
	}
	else {
		log_linef(LOG_LEVEL_WARNING, "%s [?] lowering warning: %s", source_name, message);
	}

	if (source_slice_is_valid(site)) {
		print_source_slice_marker(site, (elf_StrSlice){});
	}

	end_scratch(scratch);
}

static AstRef check_ast_type(AstRef ast, AstType type)
{
	if (!ast || ast_is_error(ast) || ast->kind != type) {
		return ERROR_AST;
	}

	return ast;
}

static IRArrayBuilder begin_ir_array_builder(LowerContext *ctx)
{
	IRArrayBuilder block = {};
	block.ctx = ctx;
	block.start = ctx->ir_stack_index;
	block.count = 0;
	return block;
}

static void push_ir_block(IRArrayBuilder *block, Ir ir)
{
	LowerContext *ctx = block->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == block->start + block->count);
	ASSERT(ctx->ir_stack_index < ctx->ir_stack_size);
	ctx->ir_stack[ctx->ir_stack_index++] = ir;
	block->count += 1;
}

static IrArray end_ir_array_builder(IRArrayBuilder *block)
{
	LowerContext *ctx = block->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == block->start + block->count);

	ctx->ir_stack_index -= block->count;
	IrArray items = {};
	items.count = block->count;
	items.items = arena_push_copy(ctx->arena
	,	sizeof(*items.items) * items.count, ctx->ir_stack + ctx->ir_stack_index);

	ASSERT(ctx->ir_stack_index == block->start);
	block->count = 0;
	return items;
}

static EntityScope get_entity_scope(LowerContext *ctx)
{
	EntityScope scope = {};
	scope.previous_scope_start = ctx->scope_start;

	ctx->scope_start = ctx->scope_end;
	return scope;
}

static void set_entity_scope(LowerContext *ctx, EntityScope scope)
{
	EntityId current_start = ctx->scope_start;
	for (i32 i = ctx->scope_end - 1; i >= (i32)current_start; -- i)
	{
		if (~ctx->entities[i].tags & ENTITY_TAG_REFERENCED) {
			report_lowering_warning(ctx, LOWERING_ERROR_UNREFERENCED_ENTITY, ctx->entities[i].site, "unreferenced entity");
		}
	}

	ASSERT(current_start <= ctx->scope_end);
	ASSERT(scope.previous_scope_start <= current_start);

	ctx->scope_end = current_start;
	ctx->scope_start = scope.previous_scope_start;
}

static DeferScope get_defer_scope(LowerContext *ctx)
{
	DeferScope scope = {};
	scope.previous_defer_start = ctx->defer_scope_start;
	ctx->defer_scope_start = ctx->defer_count;
	return scope;
}

static void set_defer_scope(LowerContext *ctx, DeferScope scope)
{
	ctx->defer_count = ctx->defer_scope_start;
	ctx->defer_scope_start = scope.previous_defer_start;
}

static void push_defer_stat(LowerContext *ctx, AstRef stat)
{
	ASSERT(ctx->defer_count < ctx->defer_stack_size);
	ctx->defer_stack[ctx->defer_count++] = stat;
}

static void emit_defer_range(LowerContext *ctx, IRArrayBuilder *items, u32 start)
{
	ASSERT(start <= ctx->defer_count);
	for (u32 i = ctx->defer_count; i > start; --i)
	{
		AstRef deferred = ctx->defer_stack[i - 1];
		lower_ast_stat_to_ir(ctx, items, deferred);
	}
}

static Entity *entity_from_name(LowerContext *ctx, elf_String *name)
{
	for (i32 i = ctx->scope_end - 1; i >= 0; -- i)
	{
		Entity *en = & ctx->entities[i];
		if (en->name == name) {
			return en;
		}
	}
	return 0;
}

static Entity *declare_entity(LowerContext *ctx, SourceSite site, EntityType type, u32 tags, elf_String *name)
{
	Entity *en = entity_from_name(ctx, name);
	const char *text = elf_atom_data(name);
	if (en)
	{
		if (en->type == ENTITY_DIRECTORY) {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "'%s': a directory with this name already exists", text);
		}
		if (en->scope_start == ctx->scope_start) {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "'%s': is already declared", text);
		}
	}

	ASSERT(ctx->scope_end < MAX_ENTITIES);

	en = & ctx->entities[ctx->scope_end ++];
	zero_memory(en, sizeof(* en));
	en->type = type;
	en->tags = tags;
	en->name = name;
	en->site = site;
	en->scope_start = ctx->scope_start;
	return en;
}

static u32 capture_entity_in_function(LowerContext *ctx, FunctionLowerContext *function_ctx, Entity *entity, SourceSite site)
{
	IrFunction *function = function_ctx->function;
	for (u32 i = 0; i < function->capture_count; ++ i)
	{
		if (function->capture_entities[i] == entity) {
			return i;
		}
	}

	ASSERT(function->capture_count < 255);
	u32 capture_index = function->capture_count++;
	function->capture_entities[capture_index] = entity;

	Ir source = 0;
	FunctionLowerContext *parent = function_ctx->parent;
	if (parent && entity->scope_start < parent->scope_start)
	{
		u32 parent_capture = capture_entity_in_function(ctx, parent, entity, site);
		source = create_capture_ir(ctx, site, parent_capture);
	}
	else
	{
		source = create_load_local_ir(ctx, site, entity->memory_ir);
	}

	function->captures[capture_index] = source;
	entity->tags |= ENTITY_TAG_REFERENCED;
	return capture_index;
}

static Ir load_entity_ir(LowerContext *ctx, SourceSite site, Entity *entity)
{
	entity->tags |= ENTITY_TAG_REFERENCED;
	if (ctx->function && entity->scope_start < ctx->function->scope_start)
	{
		u32 capture_index = capture_entity_in_function(ctx, ctx->function, entity, site);
		return create_capture_ir(ctx, site, capture_index);
	}

	return create_load_local_ir(ctx, site, entity->memory_ir);
}

static void push_loop_labels(LowerContext *ctx, LoopLabels labels)
{
	ASSERT(ctx->loop_count < ARRAY_COUNT(ctx->loop_stack));
	ctx->loop_stack[ctx->loop_count ++] = labels;
}

static void pop_loop_labels(LowerContext *ctx)
{
	ASSERT(ctx->loop_count > 0);
	ctx->loop_count -= 1;
}

static LoopLabels current_loop_labels(LowerContext *ctx, SourceSite site)
{
	if (ctx->loop_count == 0)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "break and continue must be inside a loop");
		LoopLabels labels = {};
		return labels;
	}
	return ctx->loop_stack[ctx->loop_count - 1];
}

static IrFunction *add_function_ir(LowerContext *ctx, SourceSite site, b32 variadic, u32 arity, Ir body);

static IrModule elf_lower_ast_file(LowerContext *ctx, AstRef file)
{
	PROF_BLOCK("lower.file")
	{
		IrFunction *main_fn = add_function_ir(ctx, file->site, true, IMPLICIT_PARAM_COUNT, 0);

		FunctionLowerContext main_function_ctx = {};
		main_function_ctx.function = main_fn;
		main_function_ctx.scope_start = ctx->scope_start;
		main_function_ctx.parent = 0;

		FunctionLowerContext *outer_function = ctx->function;
		ctx->function = &main_function_ctx;
		main_fn->body = lower_ast_to_ir_block(ctx, file->file.body);
		ctx->function = outer_function;
	}

	IrModule module = {};
	module.functions = ctx->functions;
	module.function_count = ctx->num_functions;
	module.entry_index = 0;
	return module;
}

static IrFunction *add_function_ir(LowerContext *ctx, SourceSite site, b32 variadic, u32 arity, Ir body)
{
	ASSERT(arity >= IMPLICIT_PARAM_COUNT);

	ASSERT(ctx->num_functions < ctx->max_functions);
	IrFunction *function = & ctx->functions[ctx->num_functions ++];
	function->site = site;
	function->variadic = variadic;
	function->arity = arity;
	function->body = body;
	function->captures = arena_push_zero(ctx->arena, sizeof(*function->captures) * 255);
	function->capture_entities = arena_push_zero(ctx->arena, sizeof(*function->capture_entities) * 255);
	function->capture_count = 0;

	return function;
}

static u32 function_index_from_ptr(LowerContext *ctx, IrFunction *function)
{
	ASSERT(function >= ctx->functions);
	ASSERT(function < ctx->functions + ctx->num_functions);
	return (u32)(function - ctx->functions);
}

static IrKind ir_kind_from_ast_kind(AstType kind)
{
	switch (kind)
	{
		case AST_AND:              return IR_AND;
		case AST_OR:               return IR_OR;
		case AST_NIL_OR:           return IR_NIL_OR;
		case AST_ADD:              return IR_ADD;
		case AST_SUB:              return IR_SUB;
		case AST_MUL:              return IR_MUL;
		case AST_DIV:              return IR_DIV;
		case AST_POW:              return IR_POW;
		case AST_MOD:              return IR_MOD;
		case AST_EQ:               return IR_EQ;
		case AST_NOT_EQ:           return IR_NOT_EQ;
		case AST_LESS_THAN:        return IR_LESS_THAN;
		case AST_LESS_THAN_EQ:     return IR_LESS_THAN_EQ;
		case AST_SHIFT_LEFT:       return IR_SHIFT_LEFT;
		case AST_SHIFT_RIGHT:      return IR_SHIFT_RIGHT;
		case AST_BITWISE_AND:      return IR_BITWISE_AND;
		case AST_BITWISE_OR:       return IR_BITWISE_OR;
		case AST_BITWISE_XOR:      return IR_BITWISE_XOR;
		case AST_BITWISE_NOT:      return IR_BITWISE_NOT;
		default: ;
	}
	return IR_NONE;
}

static IrKind ir_kind_from_compound_assign_ast_kind(AstType kind)
{
	switch (kind)
	{
		case AST_ADD_ASSIGN: return IR_ADD;
		case AST_SUB_ASSIGN: return IR_SUB;
		case AST_MUL_ASSIGN: return IR_MUL;
		case AST_DIV_ASSIGN: return IR_DIV;
		case AST_MOD_ASSIGN: return IR_MOD;
		case AST_XOR_ASSIGN: return IR_BITWISE_XOR;
		case AST_SHL_ASSIGN: return IR_SHIFT_LEFT;
		case AST_SHR_ASSIGN: return IR_SHIFT_RIGHT;
		default:             return IR_NONE;
	}
}

static Ir lower_ast_expr_to_ir(LowerContext *ctx, AstRef expr)
{
	if (!expr || ast_is_error(expr)) {
		return ERROR_IR;
	}

	Ir ir = 0;

	switch (expr->kind)
	{
		case AST_INTEGER_LITERAL:
		{
			ir = create_int_ir(ctx, expr->site, expr->integer_value);
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			ir = create_num_ir(ctx, expr->site, expr->number_value);
		}
		break;

		case AST_STRING_LITERAL:
		{
			ir = create_atom_ir(ctx, expr->site, expr->atom);
		}
		break;

		case AST_NIL_LITERAL:
		{
			ir = create_nil_ir(ctx, expr->site);
		}
		break;

		case AST_IDENT:
		{
			elf_String *ident = expr->atom;
			const char *ident_text = elf_atom_data(ident);

			Entity *en = entity_from_name(ctx, ident);

			if (en == 0)
			{
				elf_Value value = value_from_atom(expr->atom);
				u32 global_index = elf_table_ensure(ctx->state, ctx->state->globals, value);

				ir = create_load_global_ir(ctx, expr->site, global_index);
			}
			else if (en->type == ENTITY_LOCAL_DECLARATION)
			{
				ir = load_entity_ir(ctx, expr->site, en);
			}
			else
			{
				report_lowering_error(ctx, LOWERING_ERROR_UNDECLARED_IDENTIFIER, expr->site, "'%s' is an undeclared identifier", ident_text);
				ASSERT(!"Undeclared Identifier");
			}
		}
		break;

		case AST_META_FIELD:
		{
			AstRef x = expr->binary.x;
			AstRef y = expr->binary.y;

			Ir x_ir = lower_ast_expr_to_ir(ctx, x);
			Ir y_ir = lower_ast_expr_to_ir(ctx, y);

			ir = create_binary_ir(ctx, expr->site, IR_META_FIELD, x_ir, y_ir);
		}
		break;

		case AST_INDEX:
		{
			AstRef x = expr->binary.x;
			AstRef y = expr->binary.y;

			Ir x_ir = lower_ast_expr_to_ir(ctx, x);
			Ir y_ir = lower_ast_expr_to_ir(ctx, y);

			ir = create_index_ir(ctx, expr->site, x_ir, y_ir);
		}
		break;

		case AST_LENGTH_INTRINSIC:
		{
			Ir expr_ir = lower_ast_expr_to_ir(ctx, expr->unary);
			ir = create_length_intrinsic_ir(ctx, expr->site, expr_ir);
		}
		break;

		case AST_GET_MEM:
		{
			Ir expr_ir = lower_ast_expr_to_ir(ctx, expr->unary);
			if (!expr_ir || expr_ir->kind != IR_LOAD_LOCAL)
			{
				report_lowering_error(ctx, LOWERING_ERROR_GENERIC, expr->site
				, "#get_mem expects a local or parameter expression");
				ir = ERROR_IR;
			}
			else
			{
				ir = create_get_mem_ir(ctx, expr->site, expr_ir);
			}
		}
		break;

		case AST_FIELD:
		{
			AstRef x = expr->binary.x;
			AstRef y = expr->binary.y;

			Ir x_ir = lower_ast_expr_to_ir(ctx, x);
			Ir y_ir = lower_ast_expr_to_ir(ctx, y);

			ir = create_binary_ir(ctx, expr->site, IR_FIELD, x_ir, y_ir);
		}
		break;

		case AST_CALL:
		{
			AstRef func = expr->call.expr;
			AstRef *args = expr->call.args;
			u32 nargs = expr->call.nargs;

			Ir ir_func = lower_ast_expr_to_ir(ctx, func);

			Ir *ir_args = arena_push(ctx->arena, sizeof(*ir_args) * nargs);
			for (u32 i = 0; i < nargs; ++ i) {
				ir_args[i] = lower_ast_expr_to_ir(ctx, args[i]);
			}

			ir = create_call_ir(ctx, expr->site, ir_func, (IrArray) { .items = ir_args, .count = nargs });
		}
		break;

		case AST_FUNCTION:
		{
			AstRef *params = expr->function.params;
			u32 nparams = expr->function.nparams;
			u32 arity = IMPLICIT_PARAM_COUNT + nparams;
			b32 variadic = expr->function.variadic != 0;

			IrFunction *function = add_function_ir(ctx, expr->site, variadic, arity, 0);
			u32 function_index = function_index_from_ptr(ctx, function);

			EntityScope function_scope = get_entity_scope(ctx);
			FunctionLowerContext function_ctx = {};
			function_ctx.parent = ctx->function;
			function_ctx.function = function;
			function_ctx.scope_start = ctx->scope_start;

			FunctionLowerContext *outer_function = ctx->function;
			u32 outer_defer_count = ctx->defer_count;
			u32 outer_defer_scope_start = ctx->defer_scope_start;
			u32 outer_function_defer_start = ctx->function_defer_start;
			u32 outer_loop_count = ctx->loop_count;
			ctx->function = &function_ctx;
			ctx->defer_scope_start = outer_defer_count;
			ctx->function_defer_start = outer_defer_count;
			ctx->loop_count = 0;

			for (u32 i = 0; i < nparams; ++i)
			{
				AstRef param = check_ast_type(params[i], AST_FUNCTION_PARAM);
				if (ast_is_error(param)) {
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site, "expected function parameter");
					continue;
				}

				AstRef name = check_ast_type(param->param.name, AST_IDENT);
				if (ast_is_error(name)) {
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, param->site, "expected function parameter name");
					continue;
				}

				Ir param_memory = create_local_ir(ctx, name->site, 0);
				param_memory->ir_local.slot = (BcSlot){(i32)(IMPLICIT_PARAM_COUNT + i)};

				Entity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, ENTITY_TAG_PARAMETER, name->atom);
				entity->memory_ir = param_memory;
			}

			function->body = lower_ast_to_ir_block(ctx, expr->function.body);

			ctx->function = outer_function;
			ctx->loop_count = outer_loop_count;
			ctx->defer_count = outer_defer_count;
			ctx->defer_scope_start = outer_defer_scope_start;
			ctx->function_defer_start = outer_function_defer_start;
			set_entity_scope(ctx, function_scope);

			IrArray captures = {};
			captures.count = function->capture_count;
			captures.items = arena_push_copy(ctx->arena
			,	sizeof(*captures.items) * captures.count, function->captures);

			ir = create_function_ir(ctx, expr->site, function_index);
			ir->ir_function.captures = captures;
		}
		break;

		case AST_RECURSE:
		{
			ir = create_recurse_ir(ctx, expr->site);
		}
		break;

		case AST_TABLE:
		{
			AstRef *args = expr->table.args;
			u32 nargs = expr->table.nargs;

			IRArrayBuilder table_items = begin_ir_array_builder(ctx);
			Ir table_local = create_local_ir(ctx, expr->site, create_table_ir(ctx, expr->site));
			push_ir_block(&table_items, table_local);

			for (u32 i = 0; i < nargs; ++ i)
			{
				AstRef entry = check_ast_type(args[i], AST_TABLE_ENTRY);
				if (ast_is_error(entry))
				{
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, args[i]->site, "expected table entry");
					push_ir_block(&table_items, ERROR_IR);
					continue;
				}

				AstRef key = entry->table_entry.key;
				AstRef value = entry->table_entry.value;
				Ir table = create_load_local_ir(ctx, entry->site, table_local);
				Ir ir_value = lower_ast_expr_to_ir(ctx, value);
				if (key)
				{
					Ir ir_key = lower_ast_expr_to_ir(ctx, key);
					Ir field = create_binary_ir(ctx, entry->site, IR_FIELD, table, ir_key);
					push_ir_block(&table_items, create_store_ir(ctx, entry->site, field, ir_value));
				}
				else
				{
					push_ir_block(&table_items, create_array_add_ir(ctx, entry->site, table, ir_value));
				}
			}

			Ir value = create_load_local_ir(ctx, expr->site, table_local);
			IrArray stats = end_ir_array_builder(&table_items);
			ir = create_expr_block_ir(ctx, expr->site, stats, value);
		}
		break;

		case AST_TABLE_ENTRY:
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site, "table entries are only valid inside table literals");
			ir = ERROR_IR;
		}
		break;

		case AST_GREATER_THAN_EQ:
		{
			Ir x = lower_ast_expr_to_ir(ctx, expr->binary.x);
			Ir y = lower_ast_expr_to_ir(ctx, expr->binary.y);
			ir = create_binary_ir(ctx, expr->site, IR_LESS_THAN_EQ, y, x);
		}
		break;

		case AST_GREATER_THAN:
		{
			Ir x = lower_ast_expr_to_ir(ctx, expr->binary.x);
			Ir y = lower_ast_expr_to_ir(ctx, expr->binary.y);
			ir = create_binary_ir(ctx, expr->site, IR_LESS_THAN, y, x);
		}
		break;

		case AST_AND:
		case AST_OR:
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_POW:
		case AST_MOD:
		case AST_EQ:
		case AST_NOT_EQ:
		case AST_LESS_THAN:
		case AST_LESS_THAN_EQ:
		case AST_NIL_OR:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		case AST_BITWISE_XOR:
		case AST_BITWISE_NOT:
		{
			Ir x = lower_ast_expr_to_ir(ctx, expr->binary.x);
			Ir y = lower_ast_expr_to_ir(ctx, expr->binary.y);
			IrKind ir_kind = ir_kind_from_ast_kind(expr->kind);
			ir = create_binary_ir(ctx, expr->site, ir_kind, x, y);
		}
		break;
		default:
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			ir = ERROR_IR;
		}
		break;
	}

	return ir;
}

static Ir lower_ast_to_ir_block(LowerContext *ctx, AstRef stat)
{
	if (!stat || ast_is_error(stat)) {
		return ERROR_IR;
	}

	IRArrayBuilder block_items = begin_ir_array_builder(ctx);
	if (stat->kind == AST_BLOCK_STAT)
	{
		EntityScope scope = get_entity_scope(ctx);
		DeferScope defer_scope = get_defer_scope(ctx);
		for (u32 i = 0; i < stat->block.nstats; ++ i)
		{
			lower_ast_stat_to_ir(ctx, &block_items, stat->block.stats[i]);
		}
		emit_defer_range(ctx, &block_items, ctx->defer_scope_start);
		set_defer_scope(ctx, defer_scope);
		set_entity_scope(ctx, scope);
	}
	else
	{
		lower_ast_stat_to_ir(ctx, &block_items, stat);
	}

	ASSERT(ctx->ir_stack_index == block_items.start + block_items.count);
	IrArray instr = end_ir_array_builder(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, instr);
	return ir;
}

static Ir lower_ast_lvalue_to_ir_once(LowerContext *ctx, IRArrayBuilder *items, AstRef expr)
{
	Ir ir = 0;

	switch (expr->kind)
	{
		case AST_FIELD:
		{
			AstRef x = expr->binary.x;
			AstRef y = expr->binary.y;

			Ir receiver = lower_ast_expr_to_ir(ctx, x);
			Ir receiver_memory = create_local_ir(ctx, x->site, receiver);
			push_ir_block(items, receiver_memory);
			Ir receiver_load = create_load_local_ir(ctx, x->site, receiver_memory);

			Ir key_load = lower_ast_expr_to_ir(ctx, y);
			if (y->kind != AST_STRING_LITERAL)
			{
				Ir key_memory = create_local_ir(ctx, y->site, key_load);
				push_ir_block(items, key_memory);
				key_load = create_load_local_ir(ctx, y->site, key_memory);
			}

			ir = create_binary_ir(ctx, expr->site, IR_FIELD, receiver_load, key_load);
		}
		break;

		case AST_INDEX:
		{
			AstRef x = expr->binary.x;
			AstRef y = expr->binary.y;

			Ir receiver = lower_ast_expr_to_ir(ctx, x);
			Ir receiver_memory = create_local_ir(ctx, x->site, receiver);
			push_ir_block(items, receiver_memory);
			Ir receiver_load = create_load_local_ir(ctx, x->site, receiver_memory);

			Ir key = lower_ast_expr_to_ir(ctx, y);
			Ir key_memory = create_local_ir(ctx, y->site, key);
			push_ir_block(items, key_memory);
			Ir key_load = create_load_local_ir(ctx, y->site, key_memory);

			ir = create_index_ir(ctx, expr->site, receiver_load, key_load);
		}
		break;

		default:
		{
			ir = lower_ast_expr_to_ir(ctx, expr);
		}
		break;
	}

	return ir;
}

static u32 lower_local_decl_tuples_to_ir(LowerContext *ctx, IRArrayBuilder *items, AstRef name_tuple, AstRef expr_tuple, u32 entity_tags)
{
	u32 nvars = name_tuple->tuple.nargs;

	for (u32 i = 0; i < expr_tuple->tuple.nargs; ++ i)
	{
		AstRef expr = expr_tuple->tuple.args[i];
		if (expr && (expr->kind == AST_RANGE || expr->kind == AST_RANGE_INDEX))
		{
			const char *message = "range expressions cannot be used as declaration values; use them as the single step in a for loop";
			if (entity_tags & ENTITY_TAG_FORLOOP) {
				message = "range for steps must be a single expression; write `for name := start ... end ?` or `for name := values[start ... end] ?`";
			}

			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, expr->site, message);
			push_ir_block(items, ERROR_IR);
			return 1;
		}
	}

	for (u32 i = 0; i < nvars; ++ i)
	{
		AstRef name = check_ast_type(name_tuple->tuple.args[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected declaration name");
			push_ir_block(items, ERROR_IR);
			return 1;
		}

		Ir value = 0;
		if (i < expr_tuple->tuple.nargs)
		{
			AstRef expr = expr_tuple->tuple.args[i];
			value = lower_ast_expr_to_ir(ctx, expr);
		}
		else
		{
			value = create_nil_ir(ctx, name->site);
		}

		Ir local = create_local_ir(ctx, name->site, value);
		push_ir_block(items, local);

		Entity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, entity_tags, name->atom);
		entity->memory_ir = local;
	}

	return nvars;
}

static Ir lower_ast_for_range_step_to_ir(LowerContext *ctx, AstRef stat, AstRef name_tuple, AstRef range_expr)
{
	AstRef collection_expr = 0;
	AstRef range = range_expr;
	if (range_expr && range_expr->kind == AST_RANGE_INDEX)
	{
		collection_expr = range_expr->binary.x;
		range = range_expr->binary.y;
	}

	if (!range || range->kind != AST_RANGE)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, stat->site, "range for loops require a range expression");
		return ERROR_IR;
	}

	EntityScope scope = get_entity_scope(ctx);

	u32 nvars = name_tuple->tuple.nargs;
	AstRef first_name = check_ast_type(name_tuple->tuple.args[0], AST_IDENT);
	if (ast_is_error(first_name))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
		return ERROR_IR;
	}

	Ir range_start = lower_ast_expr_to_ir(ctx, range->binary.x);
	Ir range_end = lower_ast_expr_to_ir(ctx, range->binary.y);

	Ir iterator_local = create_local_ir(ctx, range->binary.x->site, range_start);
	Ir range_end_local = create_local_ir(ctx, range->binary.y->site, range_end);
	Ir collection_local = 0;
	if (collection_expr)
	{
		Ir collection = lower_ast_expr_to_ir(ctx, collection_expr);
		collection_local = create_local_ir(ctx, collection_expr->site, collection);
	}

	Ir *var_locals = arena_push(ctx->arena, sizeof(*var_locals) * nvars);
	for (u32 i = 0; i < nvars; ++ i)
	{
		AstRef name = check_ast_type(name_tuple->tuple.args[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
			set_entity_scope(ctx, scope);
			return ERROR_IR;
		}

		Ir local = create_local_ir(ctx, name->site, 0);
		var_locals[i] = local;

		Entity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, ENTITY_TAG_FORLOOP, name->atom);
		entity->memory_ir = local;
	}

	Ir start_label = create_label_ir(ctx, stat->site);
	Ir continue_label = create_label_ir(ctx, first_name->site);
	Ir break_label = create_label_ir(ctx, stat->site);

	LoopLabels loop_labels = {};
	loop_labels.continue_label = continue_label;
	loop_labels.break_label = break_label;
	loop_labels.defer_start = ctx->defer_count;
	push_loop_labels(ctx, loop_labels);
	Ir body = lower_ast_to_ir_block(ctx, stat->for_stat.body);
	pop_loop_labels(ctx);

	Ir load_iterator_local = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir load_range_end_local = create_load_local_ir(ctx, range_end->site, range_end_local);
	Ir iterator_less_than_range_end = create_binary_ir(ctx, range->site, IR_LESS_THAN, load_iterator_local, load_range_end_local);
	Ir exit_jump = create_jump_if_false_ir(ctx, range->site, iterator_less_than_range_end, break_label);

	IRArrayBuilder loop_items = begin_ir_array_builder(ctx);
	push_ir_block(&loop_items, start_label);
	push_ir_block(&loop_items, exit_jump);
	for (u32 i = 0; i < nvars; ++ i)
	{
		AstRef name = name_tuple->tuple.args[i];
		Ir iterator_load = create_load_local_ir(ctx, name->site, iterator_local);
		Ir value = iterator_load;
		if (i != 0)
		{
			Ir offset = create_int_ir(ctx, name->site, i);
			value = create_binary_ir(ctx, name->site, IR_ADD, iterator_load, offset);
		}

		if (collection_local)
		{
			Ir collection = create_load_local_ir(ctx, name->site, collection_local);
			value = create_index_ir(ctx, name->site, collection, value);
		}

		Ir var_dest = create_load_local_ir(ctx, name->site, var_locals[i]);
		push_ir_block(&loop_items, create_store_ir(ctx, name->site, var_dest, value));
	}

	Ir iterator_load_for_add = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir stride = create_int_ir(ctx, first_name->site, nvars);
	Ir next_value = create_binary_ir(ctx, first_name->site, IR_ADD, iterator_load_for_add, stride);
	Ir iterator_dest = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir increment = create_store_ir(ctx, first_name->site, iterator_dest, next_value);

	push_ir_block(&loop_items, body);
	push_ir_block(&loop_items, continue_label);
	push_ir_block(&loop_items, increment);
	push_ir_block(&loop_items, create_jump_ir(ctx, first_name->site, start_label));
	push_ir_block(&loop_items, break_label);
	IrArray loop_stats = end_ir_array_builder(&loop_items);
	Ir loop = create_block_ir(ctx, stat->site, loop_stats);

	IRArrayBuilder block_items = begin_ir_array_builder(ctx);
	push_ir_block(&block_items, iterator_local);
	push_ir_block(&block_items, range_end_local);
	if (collection_local) {
		push_ir_block(&block_items, collection_local);
	}
	for (u32 i = 0; i < nvars; ++ i)
	{
		push_ir_block(&block_items, var_locals[i]);
	}
	push_ir_block(&block_items, loop);

	IrArray stats = end_ir_array_builder(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, stats);

	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_ast_for_value_step_to_ir(LowerContext *ctx, AstRef stat, AstRef name_tuple, AstRef expr_tuple)
{
	if (expr_tuple->tuple.nargs != name_tuple->tuple.nargs)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, stat->site, "for value steps require one value per variable");
		return ERROR_IR;
	}

	EntityScope scope = get_entity_scope(ctx);

	IRArrayBuilder block_items = begin_ir_array_builder(ctx);
	u32 nlocals = lower_local_decl_tuples_to_ir(ctx, &block_items, name_tuple, expr_tuple, ENTITY_TAG_FORLOOP);

	Ir body = lower_ast_to_ir_block(ctx, stat->for_stat.body);
	push_ir_block(&block_items, body);

	ASSERT(block_items.count == nlocals + 1);
	IrArray stats = end_ir_array_builder(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, stats);
	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_ast_for_step_to_ir(LowerContext *ctx, AstRef stat, AstRef name_tuple, AstRef expr_tuple)
{
	if (expr_tuple->tuple.nargs == 1)
	{
		AstRef expr = expr_tuple->tuple.args[0];
		if (expr && (expr->kind == AST_RANGE || expr->kind == AST_RANGE_INDEX))
		{
			return lower_ast_for_range_step_to_ir(ctx, stat, name_tuple, expr);
		}
	}

	return lower_ast_for_value_step_to_ir(ctx, stat, name_tuple, expr_tuple);
}

static Ir lower_ast_for_to_ir(LowerContext *ctx, AstRef stat)
{
	AstRef decl = check_ast_type(stat->for_stat.decl, AST_DECL_STAT);
	if (ast_is_error(decl))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid for-loop declaration");
		return ERROR_IR;
	}

	AstRef name_tuple = check_ast_type(decl->decl.name, AST_TUPLE);
	AstRef for_steps = check_ast_type(decl->decl.expr, AST_FOR_STEPS);
	if (ast_is_error(name_tuple) || ast_is_error(for_steps))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid for-loop declaration");
		return ERROR_IR;
	}

	if (name_tuple->tuple.nargs == 0 || for_steps->for_steps.nargs == 0)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, stat->site, "for loops require variables and at least one step");
		return ERROR_IR;
	}

	IRArrayBuilder step_items = begin_ir_array_builder(ctx);
	for (u32 i = 0; i < for_steps->for_steps.nargs; ++ i)
	{
		AstRef expr_tuple = check_ast_type(for_steps->for_steps.args[i], AST_TUPLE);
		if (ast_is_error(expr_tuple))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, for_steps->site, "expected for-loop step tuple");
			push_ir_block(&step_items, ERROR_IR);
			continue;
		}

		push_ir_block(&step_items, lower_ast_for_step_to_ir(ctx, stat, name_tuple, expr_tuple));
	}

	IrArray steps = end_ir_array_builder(&step_items);
	return create_block_ir(ctx, stat->site, steps);
}

static Ir lower_compound_assign_to_ir(LowerContext *ctx, IRArrayBuilder *items, AstRef stat)
{
	AstRef dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
	AstRef expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
	if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid compound assignment");
		return ERROR_IR;
	}

	if (dest_tuple->tuple.nargs != 1 || expr_tuple->tuple.nargs != 1)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, stat->site, "compound assignment requires one destination and one value");
		return ERROR_IR;
	}

	IrKind op = ir_kind_from_compound_assign_ast_kind(stat->kind);
	ASSERT(op != IR_NONE);

	AstRef dest_ast = dest_tuple->tuple.args[0];
	AstRef expr_ast = expr_tuple->tuple.args[0];

	Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
	Ir right_ir = lower_ast_expr_to_ir(ctx, expr_ast);
	Ir value_ir = create_binary_ir(ctx, stat->site, op, dest_ir, right_ir);
	return create_store_ir(ctx, stat->site, dest_ir, value_ir);
}

static void lower_ast_stat_to_ir(LowerContext *ctx, IRArrayBuilder *items, AstRef stat)
{
	if (!stat || ast_is_error(stat))
	{
		push_ir_block(items, ERROR_IR);
		return;
	}

	switch (stat->kind)
	{
		case AST_IF:
		{
			AstRef pred = stat->if_stat.pred;
			AstRef true_clause = stat->if_stat.true_clause;
			AstRef else_clause = stat->if_stat.else_clause;

			Ir pred_ir = lower_ast_expr_to_ir(ctx, pred);
			Ir true_ir = lower_ast_to_ir_block(ctx, true_clause);
			Ir else_ir = 0;
			if (else_clause) {
				else_ir = lower_ast_to_ir_block(ctx, else_clause);
			}

			Ir ir = create_if_ir(ctx, stat->site, pred_ir, true_ir, else_ir);
			push_ir_block(items, ir);
		}
		break;

		case AST_WHILE:
		{
			AstRef pred = stat->while_stat.pred;
			AstRef body = stat->while_stat.body;

			Ir start_label = create_label_ir(ctx, stat->site);
			Ir break_label = create_label_ir(ctx, stat->site);

			LoopLabels loop_labels = {};
			loop_labels.continue_label = start_label;
			loop_labels.break_label = break_label;
			loop_labels.defer_start = ctx->defer_count;
			push_loop_labels(ctx, loop_labels);
			Ir body_ir = lower_ast_to_ir_block(ctx, body);
			pop_loop_labels(ctx);

			Ir pred_ir = lower_ast_expr_to_ir(ctx, pred);
			Ir jump_if_false = create_jump_if_false_ir(ctx, pred->site, pred_ir, break_label);

			IRArrayBuilder loop_items = begin_ir_array_builder(ctx);
			push_ir_block(&loop_items, start_label);
			push_ir_block(&loop_items, jump_if_false);
			push_ir_block(&loop_items, body_ir);
			push_ir_block(&loop_items, create_jump_ir(ctx, stat->site, start_label));
			push_ir_block(&loop_items, break_label);

			IrArray loop_stats = end_ir_array_builder(&loop_items);
			Ir ir = create_block_ir(ctx, stat->site, loop_stats);
			push_ir_block(items, ir);
		}
		break;

		case AST_FOR:
		{
			Ir ir = lower_ast_for_to_ir(ctx, stat);
			push_ir_block(items, ir);
		}
		break;

		case AST_BLOCK_STAT:
		{
			AstRef *stats = stat->block.stats;
			u32 nstats = stat->block.nstats;

			IRArrayBuilder block_items = begin_ir_array_builder(ctx);

			EntityScope scope = get_entity_scope(ctx);
			DeferScope defer_scope = get_defer_scope(ctx);
			for (u32 i = 0; i < nstats; ++ i)
			{
				lower_ast_stat_to_ir(ctx, &block_items, stats[i]);
			}
			emit_defer_range(ctx, &block_items, ctx->defer_scope_start);
			set_defer_scope(ctx, defer_scope);
			set_entity_scope(ctx, scope);

			ASSERT(ctx->ir_stack_index == block_items.start + block_items.count);
			IrArray ir_stats = end_ir_array_builder(&block_items);
			Ir ir = create_block_ir(ctx, stat->site, ir_stats);
			push_ir_block(items, ir);
		}
		break;

		case AST_DECL_STAT:
		{
			AstRef name_tuple = check_ast_type(stat->decl.name, AST_TUPLE);
			AstRef expr_tuple = check_ast_type(stat->decl.expr, AST_TUPLE);
			if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid declaration");
				push_ir_block(items, ERROR_IR);
				break;
			}

			lower_local_decl_tuples_to_ir(
				ctx,
				items,
				name_tuple,
				expr_tuple,
				0);
		}
		break;

		case AST_ASSIGN:
		{
			AstRef dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
			AstRef expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid assignment");
				push_ir_block(items, ERROR_IR);
				break;
			}

			u32 nargs = dest_tuple->tuple.nargs;
			if (nargs == 1)
			{
				AstRef dest_ast = dest_tuple->tuple.args[0];
				AstRef expr_ast = expr_tuple->tuple.nargs ? expr_tuple->tuple.args[0] : 0;

				Ir dest_ir = lower_ast_expr_to_ir(ctx, dest_ast);
				Ir expr_ir = expr_ast ? lower_ast_expr_to_ir(ctx, expr_ast) : create_nil_ir(ctx, dest_ast->site);
				Ir ir = create_store_ir(ctx, stat->site, dest_ir, expr_ir);
				push_ir_block(items, ir);
				break;
			}

			Ir *values = arena_push(ctx->arena, sizeof(*values) * nargs);
			for (u32 i = 0; i < nargs; ++ i)
			{
				SourceSite site = stat->site;
				Ir expr_ir;
				if (i < expr_tuple->tuple.nargs) {
					AstRef expr_ast = expr_tuple->tuple.args[i];
					site = expr_ast->site;
					expr_ir = lower_ast_expr_to_ir(ctx, expr_ast);
				}
				else {
					expr_ir = create_nil_ir(ctx, site);
				}

				values[i] = create_local_ir(ctx, site, expr_ir);
				push_ir_block(items, values[i]);
			}

			for (u32 i = 0; i < nargs; ++ i)
			{
				AstRef dest_ast = dest_tuple->tuple.args[i];
				Ir dest_ir = lower_ast_expr_to_ir(ctx, dest_ast);
				Ir value_ir = create_load_local_ir(ctx, values[i]->site, values[i]);
				Ir ir = create_store_ir(ctx, stat->site, dest_ir, value_ir);
				push_ir_block(items, ir);
			}
		}
		break;

		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_MOD_ASSIGN:
		case AST_XOR_ASSIGN:
		case AST_SHL_ASSIGN:
		case AST_SHR_ASSIGN:
		{
			Ir ir = lower_compound_assign_to_ir(ctx, items, stat);
			push_ir_block(items, ir);
		}
		break;

		case AST_NIL_ASSIGN:
		{
			AstRef dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
			AstRef expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid nil assignment");
				push_ir_block(items, ERROR_IR);
				break;
			}

			for (u32 i = 0; i < dest_tuple->tuple.nargs; ++ i)
			{
				if (i >= expr_tuple->tuple.nargs) {
					ASSERT(!"Error");
				}

				AstRef dest_ast = dest_tuple->tuple.args[i];
				AstRef expr_ast = expr_tuple->tuple.args[i];

				Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
				Ir nil_ir = create_nil_ir(ctx, dest_ast->site);
				Ir pred_ir = create_binary_ir(ctx, stat->site, IR_EQ, dest_ir, nil_ir);
				Ir value_ir = lower_ast_expr_to_ir(ctx, expr_ast);
				Ir store_ir = create_store_ir(ctx, stat->site, dest_ir, value_ir);

				IRArrayBuilder true_items = begin_ir_array_builder(ctx);
				push_ir_block(&true_items, store_ir);
				IrArray true_stats = end_ir_array_builder(&true_items);
				Ir true_clause = create_block_ir(ctx, stat->site, true_stats);
				Ir ir = create_if_ir(ctx, stat->site, pred_ir, true_clause, 0);
				push_ir_block(items, ir);
			}
		}
		break;

		case AST_RETURN:
		{
			AstRef expr = stat->return_stat.expr;
			Ir expr_ir = 0;
			if (expr)
			{
				if (expr->kind == AST_TUPLE && expr->tuple.nargs == 1) {
					expr = expr->tuple.args[0];
				}
				expr_ir = lower_ast_expr_to_ir(ctx, expr);
			}
			if (expr_ir && ctx->defer_count > 0)
			{
				Ir return_value = create_local_ir(ctx, stat->site, expr_ir);
				push_ir_block(items, return_value);
				expr_ir = create_load_local_ir(ctx, stat->site, return_value);
			}
			emit_defer_range(ctx, items, ctx->function_defer_start);
			Ir ir = create_return_ir(ctx, stat->site, expr_ir);
			push_ir_block(items, ir);
		}
		break;

		case AST_BREAK:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = create_jump_ir(ctx, stat->site, loop_labels.break_label);
			push_ir_block(items, ir);
		}
		break;

		case AST_CONTINUE:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = create_jump_ir(ctx, stat->site, loop_labels.continue_label);
			push_ir_block(items, ir);
		}
		break;

		case AST_DEFER_STAT:
		{
			push_defer_stat(ctx, stat->defer_stat.body);
		}
		break;

		case AST_TUPLE:
		{
			AstRef *args = stat->tuple.args;
			u32 nargs = stat->tuple.nargs;

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ir ir = lower_ast_expr_to_ir(ctx, args[i]);
				push_ir_block(items, ir);
			}
		}
		break;

		default:
		{
			Ir ir = lower_ast_expr_to_ir(ctx, stat);
			push_ir_block(items, ir);
		}
		break;
	}
}
