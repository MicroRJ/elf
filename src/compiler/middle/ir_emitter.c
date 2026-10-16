//
// See Copyright Notice In elf.h
//

typedef struct
{
	IrFunctionEmitter *ctx;
	u32           start;
	u32           count;
}
IrFrame;

static void lower_ast_stat_to_ir(IrFrame *block, Ast stat);
static Ir lower_ast_expr_to_ir(IrFunctionEmitter *ctx, Ast expr);
static Ir lower_ast_to_ir_block(IrFunctionEmitter *ctx, Ast stat);
static Ir lower_ast_file_body_to_ir(IrFunctionEmitter *ctx, Ast stat);
static Ir lower_ast_function_to_ir(IrFunctionEmitter *ctx, Ast expr);
static void initialize_function_ir(IrFunctionEmitter *ctx, IrFunction *function);
static IrFunction *add_function_ir(IrFunctionEmitter *ctx, SourceSite site, b32 variadic, u32 param_count, Ir body);
static u32 function_index_from_ptr(IrFunctionEmitter *ctx, IrFunction *function);
static void lower_ast_function_body(IrFunctionEmitter *ctx, Ast expr, Ir ir);

static IrModuleEmitter *ir_create_module_emitter(Compiler *compiler)
{
	elf_Arena *arena = compiler->arena;
	IrModuleEmitter *emitter = elf_arena_push_zero(arena, sizeof(*emitter));
	emitter->compiler = compiler;
	emitter->arena = arena;
	emitter->function_capacity = 1024;
	emitter->module.functions = elf_arena_push_zero(arena, sizeof(*emitter->module.functions) * emitter->function_capacity);
	ASSERT(compiler->function_count <= emitter->function_capacity);
	for (u32 i = 0; i < compiler->function_count; ++i)
	{
		Com_Function *input = compiler->functions + i;
		Ast ast = input->ast;
		IrFunction *function = emitter->module.functions + i;
		function->site = ast->site;
		function->variadic = input->variadic;
		function->is_static = ast->kind == AST_FUNCTION;
		function->param_count = input->param_count;
		ASSERT(input->index == i);
	}
	emitter->module.function_count = compiler->function_count;
	if (compiler->function_count && compiler->functions[0].ast->kind == AST_FILE)
	{
		emitter->module.has_entry = true;
		emitter->module.entry_index = compiler->functions[0].index;
	}
	return emitter;
}

static IrFunctionEmitter *ir_create_function_emitter(IrModuleEmitter *module)
{
	elf_Arena *arena = module->arena;
	IrFunctionEmitter *ctx = elf_arena_push_zero(arena, sizeof(*ctx));
	ctx->module = module;
	ctx->current_source = 0;

	u32 ir_stack_size = 4096;
	ctx->ir_stack = elf_arena_push_zero(arena, sizeof(*ctx->ir_stack) * ir_stack_size);
	ctx->ir_stack_size = ir_stack_size;
	ctx->ir_stack_index = 0;

	u32 defer_stack_size = 4096;
	ctx->defer_stack = elf_arena_push_zero(arena, sizeof(*ctx->defer_stack) * defer_stack_size);
	ctx->defer_stack_size = defer_stack_size;
	ctx->defer_count = 0;
	ctx->defer_scope_start = 0;

	return ctx;
}

static u32 add_global_symbol(IrFunctionEmitter *ctx, Atom *name)
{
	for (u32 i = 0; i < ctx->module->module.global_count; ++i)
	{
		if (ctx->module->module.globals[i] == name) {
			return i;
		}
	}

	if (ctx->module->module.global_count == ctx->module->global_capacity)
	{
		u32 capacity = ctx->module->global_capacity ? ctx->module->global_capacity * 2 : 16;
		Atom **globals = elf_arena_push(ctx->module->arena, sizeof(*globals) * capacity);
		if (ctx->module->module.globals) {
			memcpy(globals, ctx->module->module.globals, sizeof(*globals) * ctx->module->module.global_count);
		}
		ctx->module->module.globals = globals;
		ctx->module->global_capacity = capacity;
	}

	u32 index = ctx->module->module.global_count++;
	ctx->module->module.globals[index] = name;
	return index;
}

typedef enum
{
	LOWERING_ERROR_GENERIC = 0,
	LOWERING_ERROR_INTERNAL,
	LOWERING_ERROR_UNREFERENCED_ENTITY,
	LOWERING_ERROR_UNDECLARED_IDENTIFIER,
}
LoweringError;

static void report_lowering_error(IrFunctionEmitter *ctx, LoweringError error, SourceSite site, const char *format, ...)
{
	(void)error;

	va_list args;
	va_start(args, format);
	compiler_reportv(ctx->module->compiler, COMPILER_ERROR, COMPILER_PHASE_LOWERING,
		site, format, args);
	va_end(args);
}

static void report_lowering_warning(IrFunctionEmitter *ctx, LoweringError error, SourceSite site, const char *format, ...)
{
	(void)error;

	va_list args;
	va_start(args, format);
	compiler_reportv(ctx->module->compiler, COMPILER_WARNING, COMPILER_PHASE_LOWERING,
		site, format, args);
	va_end(args);
}

static Ast check_ast_type(Ast ast, AstType type)
{
	if (!ast || ast_is_error(ast) || ast->kind != type) {
		return ERROR_AST;
	}

	return ast;
}

static IrFrame begin_ir_frame(IrFunctionEmitter *ctx)
{
	IrFrame block = {};
	block.ctx = ctx;
	block.start = ctx->ir_stack_index;
	block.count = 0;
	return block;
}

static void push_ir(IrFrame *stack, Ir ir)
{
	IrFunctionEmitter *ctx = stack->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == stack->start + stack->count);
	ASSERT(ctx->ir_stack_index < ctx->ir_stack_size);
	ctx->ir_stack[ctx->ir_stack_index++] = ir;
	stack->count += 1;
}

static IrArray close_ir_frame(IrFrame *frame)
{
	IrFunctionEmitter *ctx = frame->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == frame->start + frame->count);

	ctx->ir_stack_index -= frame->count;

	IrArray items = {};
	items.count = frame->count;
	items.items = elf_arena_push_copy(ctx->module->arena, sizeof(*items.items) * items.count, ctx->ir_stack + ctx->ir_stack_index);

	ASSERT(ctx->ir_stack_index == frame->start);
	frame->count = 0;
	return items;
}

static EntityScope get_entity_scope(IrFunctionEmitter *ctx)
{
	EntityScope scope = {};
	scope.previous_scope_start = ctx->scope_start;
	scope.previous_scope_depth = ctx->scope_depth;

	ctx->scope_start = ctx->scope_end;
	ctx->scope_depth += 1;
	return scope;
}

static void set_entity_scope(IrFunctionEmitter *ctx, EntityScope scope)
{
	IrEntityId current_start = ctx->scope_start;
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
	ctx->scope_depth = scope.previous_scope_depth;
}

static DeferScope get_defer_scope(IrFunctionEmitter *ctx)
{
	DeferScope scope = {};
	scope.previous_defer_start = ctx->defer_scope_start;
	ctx->defer_scope_start = ctx->defer_count;
	return scope;
}

static void set_defer_scope(IrFunctionEmitter *ctx, DeferScope scope)
{
	ctx->defer_count = ctx->defer_scope_start;
	ctx->defer_scope_start = scope.previous_defer_start;
}

static void push_defer_stat(IrFunctionEmitter *ctx, Ast stat)
{
	ASSERT(ctx->defer_count < ctx->defer_stack_size);
	ctx->defer_stack[ctx->defer_count++] = stat;
}

static void emit_defer_range(IrFunctionEmitter *ctx, IrFrame *items, u32 start)
{
	ASSERT(start <= ctx->defer_count);
	for (u32 i = ctx->defer_count; i > start; --i)
	{
		Ast deferred = ctx->defer_stack[i - 1];
		lower_ast_stat_to_ir(items, deferred);
	}
}

static IrEntity *entity_from_name(IrFunctionEmitter *ctx, Atom *name)
{
	for (i32 i = ctx->scope_end - 1; i >= 0; -- i)
	{
		IrEntity *en = & ctx->entities[i];
		if (en->name == name) {
			return en;
		}
	}
	return 0;
}

static Com_StaticSymbol *static_symbol_from_name(IrFunctionEmitter *ctx, Atom *name)
{
	return compiler_static_symbol_from_source(ctx->module->compiler, ctx->current_source, name);
}

static IrEntity *declare_entity(IrFunctionEmitter *ctx, SourceSite site, IrEntityType type, u32 tags, Atom *name)
{
	IrEntity *en = entity_from_name(ctx, name);
	const char *name_data = atom_data(name);
	if (en)
	{
		if (en->scope_start == ctx->scope_start) {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "'%s': is already declared", name_data);
		}
	}
	else if (ctx->scope_depth == 1 && ctx->function && !ctx->function->parent && static_symbol_from_name(ctx, name)) {
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "'%s': is already declared", name_data);
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

static Ir static_value_to_ir(IrFunctionEmitter *ctx, SourceSite site, Com_StaticValue value)
{
	Ir ir = ERROR_IR;
	switch (value.kind)
	{
		case COM_STATIC_NIL:      ir = create_nil_ir(ctx, site); break;
		case COM_STATIC_INTEGER:  ir = create_int_ir(ctx, site, value.integer); break;
		case COM_STATIC_NUMBER:   ir = create_num_ir(ctx, site, value.number); break;
		case COM_STATIC_STRING:   ir = create_atom_ir(ctx, site, value.string); break;
		case COM_STATIC_FUNCTION: ir = create_function_ir(ctx, site, value.function); break;
		case COM_STATIC_NAMESPACE:
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "an imported namespace cannot be used as a runtime value");
		}
		break;
		default: break;
	}
	return ir;
}

static u32 capture_entity_in_function(IrFunctionEmitter *ctx, LwScopeFunction *function_ctx, IrEntity *entity, SourceSite site)
{
	u32 capture_index = 0;
	for (LwCapture *capture = function_ctx->first_capture; capture; capture = capture->next)
	{
		if (capture->entity == entity) {
			return capture_index;
		}
		capture_index += 1;
	}

	ASSERT(function_ctx->capture_count < 255);
	ASSERT(capture_index == function_ctx->capture_count);

	Ir source = 0;
	LwScopeFunction *parent = function_ctx->parent;
	if (parent && entity->scope_start < parent->scope_start)
	{
		u32 parent_capture = capture_entity_in_function(ctx, parent, entity, site);
		source = create_capture_ir(ctx, site, parent_capture);
	}
	else
	{
		source = create_load_local_ir(ctx, site, entity->memory_ir);
	}

	LwCapture *capture = elf_arena_push_zero(ctx->module->arena, sizeof(*capture));
	capture->entity = entity;
	capture->source = source;
	if (function_ctx->last_capture) {
		function_ctx->last_capture->next = capture;
	}
	else {
		function_ctx->first_capture = capture;
	}
	function_ctx->last_capture = capture;
	function_ctx->capture_count += 1;
	entity->tags |= ENTITY_TAG_REFERENCED;
	return capture_index;
}

static Ir load_entity_ir(IrFunctionEmitter *ctx, SourceSite site, IrEntity *entity)
{
	entity->tags |= ENTITY_TAG_REFERENCED;
	if (ctx->function && entity->scope_start < ctx->function->scope_start)
	{
		u32 capture_index = capture_entity_in_function(ctx, ctx->function, entity, site);
		return create_capture_ir(ctx, site, capture_index);
	}

	return create_load_local_ir(ctx, site, entity->memory_ir);
}

static b32 mark_entity_assigned(IrFunctionEmitter *ctx, Ast destination)
{
	if (!destination || destination->kind != AST_IDENT) return true;
	IrEntity *entity = entity_from_name(ctx, destination->atom);
	if (!entity)
	{
		if (static_symbol_from_name(ctx, destination->atom))
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, destination->site, "'%s' is constant and cannot be assigned", atom_data(destination->atom));
			return false;
		}
		return true;
	}
	if (entity->tags & ENTITY_TAG_CONSTANT)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, destination->site,
			"'%s' is constant and cannot be assigned", atom_data(destination->atom));
		return false;
	}
	entity->tags |= ENTITY_TAG_ASSIGNED;
	return true;
}

static void push_loop_labels(IrFunctionEmitter *ctx, LoopLabels labels)
{
	ASSERT(ctx->loop_count < ARRAY_COUNT(ctx->loop_stack));
	ctx->loop_stack[ctx->loop_count ++] = labels;
}

static void pop_loop_labels(IrFunctionEmitter *ctx)
{
	ASSERT(ctx->loop_count > 0);
	ctx->loop_count -= 1;
}

static LoopLabels current_loop_labels(IrFunctionEmitter *ctx, SourceSite site)
{
	if (ctx->loop_count == 0)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, site, "break and continue must be inside a loop");
		LoopLabels labels = {};
		return labels;
	}
	return ctx->loop_stack[ctx->loop_count - 1];
}

static void ir_emit_function(IrFunctionEmitter *ctx, Com_Function *input)
{
	ASSERT(input->index < ctx->module->module.function_count);
	IrFunction *function = ctx->module->module.functions + input->index;
	initialize_function_ir(ctx, function);
	ctx->current_source = input->source;
	if (input->ast->kind == AST_FILE)
	{
		Ast file = input->ast;
		ASSERT(file && file->kind == AST_FILE);

		LwScopeFunction main_function_ctx = {};
		main_function_ctx.function = function;
		main_function_ctx.scope_start = ctx->scope_start;
		main_function_ctx.parent = 0;

		LwScopeFunction *outer_function = ctx->function;
		ctx->function = &main_function_ctx;
		function->body = lower_ast_file_body_to_ir(ctx, file->file.body);
		ctx->function = outer_function;
	}
	else
	{
		ASSERT(input->ast->kind == AST_FUNCTION);
		Ir function = create_function_ir(ctx, input->ast->site, input->index);
		lower_ast_function_body(ctx, input->ast, function);
	}
}

static void initialize_function_ir(IrFunctionEmitter *ctx, IrFunction *function)
{
	ASSERT(function->param_count >= IMPLICIT_PARAM_COUNT);
	ASSERT(!function->params);

	function->params = elf_arena_push_zero(ctx->module->arena, sizeof(*function->params) * function->param_count);
	function->params[IMPLICIT_PARAM_INDEX] = create_local_ir(ctx, function->site, 0);
	function->capture_count = 0;
}

static IrFunction *add_function_ir(IrFunctionEmitter *ctx, SourceSite site, b32 variadic, u32 param_count, Ir body)
{
	ASSERT(param_count >= IMPLICIT_PARAM_COUNT);

	ASSERT(ctx->module->module.function_count < ctx->module->function_capacity);
	IrFunction *function = & ctx->module->module.functions[ctx->module->module.function_count ++];
	function->site = site;
	function->variadic = variadic;
	function->body = body;
	function->param_count = param_count;
	initialize_function_ir(ctx, function);

	return function;
}

static u32 function_index_from_ptr(IrFunctionEmitter *ctx, IrFunction *function)
{
	ASSERT(function >= ctx->module->module.functions);
	ASSERT(function < ctx->module->module.functions + ctx->module->module.function_count);
	return (u32)(function - ctx->module->module.functions);
}

static Ir create_ast_function_value_ir(IrFunctionEmitter *ctx, Ast expr)
{
	u32 param_count = IMPLICIT_PARAM_COUNT + expr->function.nparams;
	IrFunction *function = add_function_ir(ctx, expr->site, expr->function.variadic != 0, param_count, 0);
	u32 function_index = function_index_from_ptr(ctx, function);
	return create_function_ir(ctx, expr->site, function_index);
}

static TypeRule type_rule_from_atom(Atom *atom)
{
	switch (atom->id)
	{
#define TYPE_NAME_RULE(NAME, TEXT, RULE) case ATOM_TYPE_NAME_##NAME: return RULE;
		TYPE_NAME_DEFINITIONS(TYPE_NAME_RULE)
#undef TYPE_NAME_RULE
		default: return TRULE_NONE;
	}
}

static void lower_ast_function_body(IrFunctionEmitter *ctx, Ast expr, Ir ir)
{
	ASSERT(ir && ir->kind == IR_FUNCTION);
	ASSERT(ir->ir_function.index < ctx->module->module.function_count);
	IrFunction *function = ctx->module->module.functions + ir->ir_function.index;
	Ast *params = expr->function.params;
	u32 nparams = expr->function.nparams;

	EntityScope function_scope = get_entity_scope(ctx);
	LwScopeFunction function_ctx = {};
	function_ctx.parent = ctx->function;
	function_ctx.function = function;
	function_ctx.scope_start = ctx->scope_start;

	LwScopeFunction *outer_function = ctx->function;
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
		Ast param = check_ast_type(params[i], AST_FUNCTION_PARAM);
		if (ast_is_error(param))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site, "expected function parameter");
			continue;
		}

		Ast name = check_ast_type(param->param.name, AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, param->site, "expected function parameter name");
			continue;
		}

		Ir param_memory = create_local_ir(ctx, name->site, 0);
		function->params[IMPLICIT_PARAM_COUNT + i] = param_memory;
		Ast type = param->param.type;
		if (type)
		{
			type = check_ast_type(type, AST_IDENT);
			if (ast_is_error(type))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, param->site, "expected function parameter type name");
			}
			else
			{
				TypeRule rule = type_rule_from_atom(type->atom);
				if (rule == TRULE_NONE)
				{
					report_lowering_error(ctx, LOWERING_ERROR_GENERIC, type->site, "'%s' is not a known type name", atom_data(type->atom));
				}
				else
				{
					param_memory->type_rule = rule;
				}
			}
		}

		IrEntity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, ENTITY_TAG_PARAMETER, name->atom);
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
	captures.count = function_ctx.capture_count;
	if (captures.count) {
		captures.items = elf_arena_push(ctx->module->arena, sizeof(*captures.items) * captures.count);
	}
	u32 capture_index = 0;
	for (LwCapture *capture = function_ctx.first_capture; capture; capture = capture->next)
	{
		ASSERT(capture_index < captures.count);
		captures.items[capture_index++] = capture->source;
	}
	ASSERT(capture_index == captures.count);
	ir->ir_function.captures = captures;
	function->capture_count = function_ctx.capture_count;

}

static Ir lower_ast_function_to_ir(IrFunctionEmitter *ctx, Ast expr)
{
	Ir ir = create_ast_function_value_ir(ctx, expr);
	lower_ast_function_body(ctx, expr, ir);
	return ir;
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

static IrCallType call_type_from_callee(IrFunctionEmitter *ctx, Ir callee)
{
	IrCallType type = IR_CALL_DYNAMIC;
	if (callee && callee->kind == IR_FUNCTION && callee->ir_function.index < ctx->module->module.function_count)
	{
		IrFunction *function = ctx->module->module.functions + callee->ir_function.index;
		if (function->is_static && function->capture_count == 0) {
			type = IR_CALL_DIRECT;
		}
	}
	return type;
}

static b32 ast_is_call(Ast expr)
{
	return expr && expr->kind == AST_CALL;
}

static IrArray lower_direct_call_arguments(IrFunctionEmitter *ctx, Ast call, Ir callee, b32 *failed)
{
	ASSERT(call && call->kind == AST_CALL);
	ASSERT(callee && callee->kind == IR_FUNCTION);
	ASSERT(callee->ir_function.index < ctx->module->compiler->function_count);
	ASSERT(callee->ir_function.index < ctx->module->module.function_count);

	Com_Function *function = ctx->module->compiler->functions + callee->ir_function.index;
	IrFunction *ir_function = ctx->module->module.functions + callee->ir_function.index;
	ASSERT(function->param_count >= IMPLICIT_PARAM_COUNT);
	u32 declared_param_count = function->param_count - IMPLICIT_PARAM_COUNT;
	u32 argument_count = call->call.nargs;
	b32 has_carg = call->call.carg != 0;
	if (has_carg && ir_function->variadic)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, call->call.carg->site, "trailing closures cannot call variadic functions");
		*failed = true;
		return (IrArray) {};
	}
	b32 reserve_final_parameter = has_carg && declared_param_count > 0;
	u32 bind_param_count = declared_param_count - reserve_final_parameter;
	u32 capacity = argument_count + has_carg;
	u32 minimum_capacity = declared_param_count;
	if (capacity < minimum_capacity) {
		capacity = minimum_capacity;
	}
	IrArray result = {};
	if (capacity) {
		result.items = elf_arena_push(ctx->module->arena, sizeof(*result.items) * capacity);
	}
	if (has_carg && declared_param_count == 0)
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, call->call.carg->site, "trailing closure requires a final function parameter");
		*failed = true;
	}

	u32 argument_index = 0;
	u32 parameter_index = 0;
	while (argument_index < argument_count && call->call.args[argument_index]->kind != AST_NAMED_ARGUMENT)
	{
		Ast argument = call->call.args[argument_index];
		if (parameter_index >= bind_param_count && !ir_function->variadic)
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "too many arguments for static function");
			*failed = true;
		}
		else
		{
			Ir argument_ir = lower_ast_expr_to_ir(ctx, argument);
			result.items[result.count++] = argument_ir;
			if (!argument_ir || argument_ir == ERROR_IR) {
				*failed = true;
			}
		}
		argument_index += 1;
		parameter_index += 1;
	}

	while (parameter_index < bind_param_count)
	{
		Com_FunctionParameter *parameter = function->params + parameter_index;
		Ast argument = argument_index < argument_count ? call->call.args[argument_index] : 0;
		if (argument && argument->kind != AST_NAMED_ARGUMENT)
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "positional arguments cannot follow named arguments");
			*failed = true;
			argument_index += 1;
		}
		else if (argument && argument->named_argument.name->atom == parameter->name)
		{
			Ir argument_ir = lower_ast_expr_to_ir(ctx, argument->named_argument.expr);
			result.items[result.count++] = argument_ir;
			if (!argument_ir || argument_ir == ERROR_IR) {
				*failed = true;
			}
			argument_index += 1;
			parameter_index += 1;
		}
		else if (parameter->has_default)
		{
			Ir argument_ir = static_value_to_ir(ctx, parameter->site, parameter->default_value);
			result.items[result.count++] = argument_ir;
			if (!argument_ir || argument_ir == ERROR_IR) {
				*failed = true;
			}
			parameter_index += 1;
		}
		else
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, call->site, "required parameter '%s' was not provided", atom_data(parameter->name));
			*failed = true;
			parameter_index += 1;
		}
	}

	if (argument_index < argument_count)
	{
		Ast argument = call->call.args[argument_index];
		if (reserve_final_parameter && argument->kind == AST_NAMED_ARGUMENT && argument->named_argument.name->atom == function->params[declared_param_count - 1].name) {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "final parameter '%s' is already supplied by the trailing closure", atom_data(argument->named_argument.name->atom));
		}
		else if (argument->kind == AST_NAMED_ARGUMENT) {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "named argument '%s' is unknown, duplicated, or out of order", atom_data(argument->named_argument.name->atom));
		}
		else {
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "too many arguments for static function");
		}
		*failed = true;
	}
	if (has_carg && declared_param_count > 0)
	{
		Ir argument_ir = lower_ast_expr_to_ir(ctx, call->call.carg);
		result.items[result.count++] = argument_ir;
		if (!argument_ir || argument_ir == ERROR_IR) {
			*failed = true;
		}
	}

	return result;
}

static Ir lower_ast_expr_to_ir(IrFunctionEmitter *ctx, Ast expr)
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

		case AST_INTERPOLATED_STRING:
		{
			Ast *parts = expr->interpolated_string.args;
			u32 nparts = expr->interpolated_string.nargs;
			if (nparts < 3 || (nparts & 1) == 0)
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site,
					"interpolated string must contain alternating text and expression parts");
				ir = ERROR_IR;
				break;
			}

			ir = 0;
			for (u32 i = 0; i < nparts; ++ i)
			{
				Ast part = parts[i];
				if (!part || ((i & 1) == 0 && part->kind != AST_STRING_LITERAL))
				{
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, expr->site, "invalid interpolated string part at index %u", i);
					ir = ERROR_IR;
					break;
				}

				Ir part_ir = lower_ast_expr_to_ir(ctx, part);
				if (!part_ir || part_ir == ERROR_IR)
				{
					ir = ERROR_IR;
					break;
				}

				ir = ir ? ir_new_binary(ctx, part->site, IR_ADD, ir, part_ir) : part_ir;
			}
		}
		break;

		case AST_NIL_LITERAL:
		{
			ir = create_nil_ir(ctx, expr->site);
		}
		break;

		case AST_IDENT:
		{
			Atom *ident = expr->atom;

			IrEntity *en = entity_from_name(ctx, ident);

			if (en && en->type == ENTITY_LOCAL_DECLARATION)
			{
				ir = load_entity_ir(ctx, expr->site, en);
			}
			else if (en)
			{
				report_lowering_error(ctx, LOWERING_ERROR_UNDECLARED_IDENTIFIER, expr->site, "'%s' is an undeclared identifier", atom_data(ident));
				ir = ERROR_IR;
			}
			else
			{
				Com_StaticSymbol *symbol = static_symbol_from_name(ctx, ident);
				if (symbol)
				{
					symbol->referenced = true;
					ir = static_value_to_ir(ctx, expr->site, symbol->value);
				}
				else
				{
					u32 global_index = add_global_symbol(ctx, expr->atom);
					ir = create_load_global_ir(ctx, expr->site, global_index);
				}
			}
		}
		break;

		case AST_META_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = lower_ast_expr_to_ir(ctx, x);
			Ir y_ir = lower_ast_expr_to_ir(ctx, y);

			ir = ir_new_binary(ctx, expr->site, IR_META_FIELD, x_ir, y_ir);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = lower_ast_expr_to_ir(ctx, x);
			Ir y_ir = lower_ast_expr_to_ir(ctx, y);

			ir = create_index_ir(ctx, expr->site, x_ir, y_ir);
		}
		break;

		case AST_MEASURE:
		{
			Ir expr_ir = lower_ast_expr_to_ir(ctx, expr->unary);
			ir = create_measure_ir(ctx, expr->site, expr_ir);
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
			Com_StaticValue value = {};
			Com_StaticLookup lookup = compiler_lookup_static_expr(ctx->module->compiler, ctx->current_source, expr, &value);
			if (lookup == COM_STATIC_LOOKUP_VALUE) {
				ir = static_value_to_ir(ctx, expr->site, value);
			}
			else if (lookup == COM_STATIC_LOOKUP_ERROR) {
				ir = ERROR_IR;
			}
			else
			{
				Ast x = expr->binary.x;
				Ast y = expr->binary.y;
				Ir x_ir = lower_ast_expr_to_ir(ctx, x);
				Ir y_ir = lower_ast_expr_to_ir(ctx, y);
				ir = ir_new_binary(ctx, expr->site, IR_FIELD, x_ir, y_ir);
			}
		}
		break;

		case AST_CALL:
		{
			u32 nargs = expr->call.nargs;

			Ir ir_func = lower_ast_expr_to_ir(ctx, expr->call.expr);
			IrCallType call_type = call_type_from_callee(ctx, ir_func);
			b32 failed = !ir_func || ir_func == ERROR_IR;
			IrArray ir_args = {};
			if (expr->call.carg && call_type != IR_CALL_DIRECT)
			{
				report_lowering_error(ctx, LOWERING_ERROR_GENERIC, expr->site, "trailing closures require a statically known function");
				failed = true;
			}
			else if (call_type == IR_CALL_DIRECT)
			{
				ir_args = lower_direct_call_arguments(ctx, expr, ir_func, &failed);
			}
			else
			{
				if (nargs) {
					ir_args.items = elf_arena_push(ctx->module->arena, sizeof(*ir_args.items) * nargs);
				}
				for (u32 i = 0; i < nargs; ++i)
				{
					Ast argument = expr->call.args[i];
					if (argument->kind == AST_NAMED_ARGUMENT)
					{
						report_lowering_error(ctx, LOWERING_ERROR_GENERIC, argument->site, "named arguments require a statically known function");
						failed = true;
					}
					else
					{
						Ir argument_ir = lower_ast_expr_to_ir(ctx, argument);
						ir_args.items[ir_args.count++] = argument_ir;
						if (!argument_ir || argument_ir == ERROR_IR) {
							failed = true;
						}
					}
				}
			}

			ir = failed ? ERROR_IR : create_call_ir(ctx, expr->site, call_type, ir_func, ir_args);
		}
		break;

		case AST_FUNCTION:
		{
			ir = lower_ast_function_to_ir(ctx, expr);
		}
		break;

		case AST_RECURSE:
		{
			ASSERT(ctx->function && ctx->function->function);
			IrFunction *function = ctx->function->function;
			if (function->is_static) {
				u32 function_index = function_index_from_ptr(ctx, function);
				ir = create_function_ir(ctx, expr->site, function_index);
			}
			else {
				ir = create_recurse_ir(ctx, expr->site);
			}
		}
		break;

		case AST_TABLE:
		{
			Ast *args = expr->table.args;
			u32 nargs = expr->table.nargs;

			IrFrame table_items = begin_ir_frame(ctx);
			Ir table_local = create_local_ir(ctx, expr->site, create_table_ir(ctx, expr->site));
			push_ir(&table_items, table_local);

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast entry = check_ast_type(args[i], AST_TABLE_ENTRY);
				if (ast_is_error(entry))
				{
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, args[i]->site, "expected table entry");
					push_ir(&table_items, ERROR_IR);
					continue;
				}

				Ast key = entry->table_entry.key;
				Ast value = entry->table_entry.value;
				Ir table = create_load_local_ir(ctx, entry->site, table_local);
				Ir ir_value = lower_ast_expr_to_ir(ctx, value);
				if (!key)
				{
					report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, entry->site, "expected keyed table entry");
					push_ir(&table_items, ERROR_IR);
					continue;
				}
				Ir ir_key = lower_ast_expr_to_ir(ctx, key);
				Ir field = ir_new_binary(ctx, entry->site, IR_FIELD, table, ir_key);
				push_ir(&table_items, create_store_ir(ctx, entry->site, field, ir_value));
			}

			Ir value = create_load_local_ir(ctx, expr->site, table_local);
			IrArray stats = close_ir_frame(&table_items);
			ir = create_expr_block_ir(ctx, expr->site, stats, value);
		}
		break;

		case AST_ARRAY:
		{
			IrFrame array_items = begin_ir_frame(ctx);
			Ir array_local = create_local_ir(ctx, expr->site, create_array_ir(ctx, expr->site));
			push_ir(&array_items, array_local);

			for (u32 i = 0; i < expr->array.nargs; ++i)
			{
				Ir array = create_load_local_ir(ctx, expr->site, array_local);
				Ir value = lower_ast_expr_to_ir(ctx, expr->array.args[i]);
				push_ir(&array_items, create_array_add_ir(ctx, expr->array.args[i]->site, array, value));
			}

			Ir value = create_load_local_ir(ctx, expr->site, array_local);
			IrArray stats = close_ir_frame(&array_items);
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
			ir = ir_new_binary(ctx, expr->site, IR_LESS_THAN_EQ, y, x);
		}
		break;

		case AST_GREATER_THAN:
		{
			Ir x = lower_ast_expr_to_ir(ctx, expr->binary.x);
			Ir y = lower_ast_expr_to_ir(ctx, expr->binary.y);
			ir = ir_new_binary(ctx, expr->site, IR_LESS_THAN, y, x);
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
			ir = ir_new_binary(ctx, expr->site, ir_kind, x, y);
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

static Ir lower_ast_block(IrFunctionEmitter *ctx, Ast stat, b32 is_file_body)
{
	if (!stat || ast_is_error(stat)) {
		return ERROR_IR;
	}

	IrFrame block_items = begin_ir_frame(ctx);
	if (stat->kind == AST_BLOCK_STAT)
	{
		EntityScope scope = get_entity_scope(ctx);
		DeferScope defer_scope = get_defer_scope(ctx);
		for (u32 i = 0; i < stat->block.nstats; ++ i)
		{
			Ast item = stat->block.stats[i];
			if (!is_file_body || item->kind != AST_STATIC_DECL) {
				lower_ast_stat_to_ir(&block_items, item);
			}
		}
		emit_defer_range(ctx, &block_items, ctx->defer_scope_start);
		set_defer_scope(ctx, defer_scope);
		set_entity_scope(ctx, scope);
	}
	else
	{
		lower_ast_stat_to_ir(&block_items, stat);
	}

	ASSERT(ctx->ir_stack_index == block_items.start + block_items.count);
	IrArray instr = close_ir_frame(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, instr);
	return ir;
}

static Ir lower_ast_to_ir_block(IrFunctionEmitter *ctx, Ast stat)
{
	return lower_ast_block(ctx, stat, false);
}

static Ir lower_ast_file_body_to_ir(IrFunctionEmitter *ctx, Ast stat)
{
	ASSERT(stat && stat->kind == AST_BLOCK_STAT);
	return lower_ast_block(ctx, stat, true);
}

static Ir lower_ast_lvalue_to_ir_once(IrFunctionEmitter *ctx, IrFrame *items, Ast expr)
{
	Ir ir = 0;

	switch (expr->kind)
	{
		case AST_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = lower_ast_expr_to_ir(ctx, x);
			Ir receiver_memory = create_local_ir(ctx, x->site, receiver);
			push_ir(items, receiver_memory);
			Ir receiver_load = create_load_local_ir(ctx, x->site, receiver_memory);

			Ir key_load = lower_ast_expr_to_ir(ctx, y);
			if (y->kind != AST_STRING_LITERAL)
			{
				Ir key_memory = create_local_ir(ctx, y->site, key_load);
				push_ir(items, key_memory);
				key_load = create_load_local_ir(ctx, y->site, key_memory);
			}

			ir = ir_new_binary(ctx, expr->site, IR_FIELD, receiver_load, key_load);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = lower_ast_expr_to_ir(ctx, x);
			Ir receiver_memory = create_local_ir(ctx, x->site, receiver);
			push_ir(items, receiver_memory);
			Ir receiver_load = create_load_local_ir(ctx, x->site, receiver_memory);

			Ir key = lower_ast_expr_to_ir(ctx, y);
			Ir key_memory = create_local_ir(ctx, y->site, key);
			push_ir(items, key_memory);
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

static u32 lower_local_decl_tuples_to_ir(IrFunctionEmitter *ctx, IrFrame *items, Ast name_tuple, Ast expr_tuple, u32 entity_tags)
{
	u32 nvars = name_tuple->tuple.nargs;

	for (u32 i = 0; i < expr_tuple->tuple.nargs; ++ i)
	{
		Ast expr = expr_tuple->tuple.args[i];
		if (expr && (expr->kind == AST_RANGE || expr->kind == AST_RANGE_INDEX))
		{
			const char *message = "range expressions cannot be used as declaration values; use a range for loop";
			if (entity_tags & ENTITY_TAG_FORLOOP) {
				message = "range for loops require one range expression";
			}

			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, expr->site, message);
			push_ir(items, ERROR_IR);
			return 1;
		}
	}

	if (nvars > 1 && expr_tuple->tuple.nargs == 1 && ast_is_call(expr_tuple->tuple.args[0]))
	{
		Ir expr = lower_ast_expr_to_ir(ctx, expr_tuple->tuple.args[0]);
		Ir *locals = elf_arena_push(ctx->module->arena, sizeof(*locals) * nvars);
		for (u32 i = 0; i < nvars; ++i)
		{
			Ast name = check_ast_type(name_tuple->tuple.args[i], AST_IDENT);
			if (ast_is_error(name))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected declaration name");
				push_ir(items, ERROR_IR);
				return 1;
			}
			Ir local = create_local_ir(ctx, name->site, 0);
			locals[i] = local;
			IrEntity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, entity_tags, name->atom);
			entity->memory_ir = local;
		}
		push_ir(items, create_multi_local_ir(ctx, expr_tuple->site, expr, (IrArray){ .items = locals, .count = nvars }));
		return nvars;
	}

	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = check_ast_type(name_tuple->tuple.args[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected declaration name");
			push_ir(items, ERROR_IR);
			return 1;
		}

		Ir value = 0;
		if (i < expr_tuple->tuple.nargs)
		{
			Ast expr = expr_tuple->tuple.args[i];
			value = lower_ast_expr_to_ir(ctx, expr);
		}
		else
		{
			value = create_nil_ir(ctx, name->site);
		}

		Ir local = create_local_ir(ctx, name->site, value);
		push_ir(items, local);

		IrEntity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION, entity_tags, name->atom);
		entity->memory_ir = local;
	}

	return nvars;
}

static Ir lower_ast_for_range_expr_to_ir(IrFunctionEmitter *ctx, Ast stat, Ast name_tuple,
	Ast range_expr, u32 entity_tags)
{
	Ast collection_expr = 0;
	Ast range = range_expr;
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
	Ast first_name = check_ast_type(name_tuple->tuple.args[0], AST_IDENT);
	if (ast_is_error(first_name))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
		return ERROR_IR;
	}

	Ir collection_local = 0;
	if (collection_expr)
	{
		Ir collection = lower_ast_expr_to_ir(ctx, collection_expr);
		collection_local = create_local_ir(ctx, collection_expr->site, collection);
	}

	Ast range_start_expr = range->binary.x;
	Ast range_end_expr = range->binary.y;
	SourceSite range_start_site = range_start_expr ? range_start_expr->site : range->site;
	SourceSite range_end_site = range_end_expr ? range_end_expr->site : range->site;

	Ir range_start = range_start_expr
		? lower_ast_expr_to_ir(ctx, range_start_expr)
		: create_int_ir(ctx, range_start_site, 0);
	Ir range_end = 0;
	if (range_end_expr) {
		range_end = lower_ast_expr_to_ir(ctx, range_end_expr);
	}
	else if (collection_local)
	{
		Ir collection = create_load_local_ir(ctx, range_end_site, collection_local);
		range_end = create_measure_ir(ctx, range_end_site, collection);
	}
	else
	{
		report_lowering_error(ctx, LOWERING_ERROR_GENERIC, range->site,
			"an omitted range end requires an indexed collection");
		set_entity_scope(ctx, scope);
		return ERROR_IR;
	}

	Ir iterator_local = create_local_ir(ctx, range_start_site, range_start);
	Ir range_end_local = create_local_ir(ctx, range_end_site, range_end);

	Ir *var_locals = elf_arena_push(ctx->module->arena, sizeof(*var_locals) * nvars);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = check_ast_type(name_tuple->tuple.args[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
			set_entity_scope(ctx, scope);
			return ERROR_IR;
		}

		Ir local = create_local_ir(ctx, name->site, 0);
		var_locals[i] = local;

		IrEntity *entity = declare_entity(ctx, name->site, ENTITY_LOCAL_DECLARATION,
			ENTITY_TAG_FORLOOP | entity_tags, name->atom);
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
	Ir body = lower_ast_to_ir_block(ctx, stat->for_range_stat.body);
	pop_loop_labels(ctx);

	Ir load_iterator_local = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir load_range_end_local = create_load_local_ir(ctx, range_end_site, range_end_local);
	Ir iterator_less_than_range_end = ir_new_binary(ctx, range->site, IR_LESS_THAN, load_iterator_local, load_range_end_local);
	Ir exit_jump = create_jump_if_false_ir(ctx, range->site, iterator_less_than_range_end, break_label);

	IrFrame loop_items = begin_ir_frame(ctx);
	push_ir(&loop_items, start_label);
	push_ir(&loop_items, exit_jump);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = name_tuple->tuple.args[i];
		Ir iterator_load = create_load_local_ir(ctx, name->site, iterator_local);
		Ir value = iterator_load;
		if (i != 0)
		{
			Ir offset = create_int_ir(ctx, name->site, i);
			value = ir_new_binary(ctx, name->site, IR_ADD, iterator_load, offset);
		}

		if (collection_local)
		{
			Ir collection = create_load_local_ir(ctx, name->site, collection_local);
			value = create_index_ir(ctx, name->site, collection, value);
		}

		Ir var_dest = create_load_local_ir(ctx, name->site, var_locals[i]);
		push_ir(&loop_items, create_store_ir(ctx, name->site, var_dest, value));
	}

	Ir iterator_load_for_add = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir stride = create_int_ir(ctx, first_name->site, nvars);
	Ir next_value = ir_new_binary(ctx, first_name->site, IR_ADD, iterator_load_for_add, stride);
	Ir iterator_dest = create_load_local_ir(ctx, first_name->site, iterator_local);
	Ir increment = create_store_ir(ctx, first_name->site, iterator_dest, next_value);

	push_ir(&loop_items, body);
	push_ir(&loop_items, continue_label);
	push_ir(&loop_items, increment);
	push_ir(&loop_items, create_jump_ir(ctx, first_name->site, start_label));
	push_ir(&loop_items, break_label);
	IrArray loop_stats = close_ir_frame(&loop_items);
	Ir loop = create_block_ir(ctx, stat->site, loop_stats);

	IrFrame block_items = begin_ir_frame(ctx);
	if (collection_local) {
		push_ir(&block_items, collection_local);
	}
	push_ir(&block_items, iterator_local);
	push_ir(&block_items, range_end_local);
	for (u32 i = 0; i < nvars; ++ i)
	{
		push_ir(&block_items, var_locals[i]);
	}
	push_ir(&block_items, loop);

	IrArray stats = close_ir_frame(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, stats);

	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_ast_for_range_to_ir(IrFunctionEmitter *ctx, Ast stat)
{
	Ast decl = check_ast_type(stat->for_range_stat.decl, AST_DECL_STAT);
	if (ast_is_error(decl))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	Ast name_tuple = check_ast_type(decl->decl.name, AST_TUPLE);
	Ast expr_tuple = check_ast_type(decl->decl.expr, AST_TUPLE);
	if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	if (name_tuple->tuple.nargs == 0 || expr_tuple->tuple.nargs != 1)
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid range for-loop arity");
		return ERROR_IR;
	}

	Ast range = expr_tuple->tuple.args[0];
	if (!range || (range->kind != AST_RANGE && range->kind != AST_RANGE_INDEX))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid range for-loop expression");
		return ERROR_IR;
	}

	u32 entity_tags = decl->decl.tags & AST_DECL_TAG_CONSTANT ? ENTITY_TAG_CONSTANT : 0;
	return lower_ast_for_range_expr_to_ir(ctx, stat, name_tuple, range, entity_tags);
}

static Ir lower_ast_for_to_ir(IrFunctionEmitter *ctx, Ast stat)
{
	Ast init = check_ast_type(stat->for_stat.init, AST_DECL_STAT);
	if (ast_is_error(init))
	{
		report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid C-style for-loop initializer");
		return ERROR_IR;
	}

	EntityScope scope = get_entity_scope(ctx);
	IrFrame block_items = begin_ir_frame(ctx);
	lower_ast_stat_to_ir(&block_items, init);

	Ir condition_label = create_label_ir(ctx, stat->site);
	Ir continue_label = create_label_ir(ctx, stat->for_stat.step->site);
	Ir break_label = create_label_ir(ctx, stat->site);

	LoopLabels loop_labels = {};
	loop_labels.continue_label = continue_label;
	loop_labels.break_label = break_label;
	loop_labels.defer_start = ctx->defer_count;
	push_loop_labels(ctx, loop_labels);
	Ir body = lower_ast_to_ir_block(ctx, stat->for_stat.body);
	pop_loop_labels(ctx);

	Ir pred = lower_ast_expr_to_ir(ctx, stat->for_stat.pred);
	Ir exit_jump = create_jump_if_false_ir(ctx, stat->for_stat.pred->site, pred, break_label);
	Ir step = lower_ast_to_ir_block(ctx, stat->for_stat.step);

	IrFrame loop_items = begin_ir_frame(ctx);
	push_ir(&loop_items, condition_label);
	push_ir(&loop_items, exit_jump);
	push_ir(&loop_items, body);
	push_ir(&loop_items, continue_label);
	push_ir(&loop_items, step);
	push_ir(&loop_items, create_jump_ir(ctx, stat->site, condition_label));
	push_ir(&loop_items, break_label);
	IrArray loop_stats = close_ir_frame(&loop_items);
	push_ir(&block_items, create_block_ir(ctx, stat->site, loop_stats));

	IrArray stats = close_ir_frame(&block_items);
	Ir ir = create_block_ir(ctx, stat->site, stats);
	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_compound_assign_to_ir(IrFunctionEmitter *ctx, IrFrame *items, Ast stat)
{
	Ast dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
	Ast expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
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

	Ast dest_ast = dest_tuple->tuple.args[0];
	Ast expr_ast = expr_tuple->tuple.args[0];
	if (!mark_entity_assigned(ctx, dest_ast)) return ERROR_IR;

	Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
	Ir right_ir = lower_ast_expr_to_ir(ctx, expr_ast);
	Ir value_ir = ir_new_binary(ctx, stat->site, op, dest_ir, right_ir);
	return create_store_ir(ctx, stat->site, dest_ir, value_ir);
}

static void lower_ast_stat_to_ir(IrFrame *items, Ast stat)
{
	IrFunctionEmitter *ctx = items->ctx;

	if (!stat || ast_is_error(stat))
	{
		push_ir(items, ERROR_IR);
		return;
	}

	switch (stat->kind)
	{
		case AST_IF:
		{
			Ast pred = stat->if_stat.pred;
			Ast true_clause = stat->if_stat.true_clause;
			Ast else_clause = stat->if_stat.else_clause;

			Ir pred_ir = lower_ast_expr_to_ir(ctx, pred);
			Ir true_ir = lower_ast_to_ir_block(ctx, true_clause);
			Ir else_ir = 0;
			if (else_clause) {
				else_ir = lower_ast_to_ir_block(ctx, else_clause);
			}

			Ir ir = create_if_ir(ctx, stat->site, pred_ir, true_ir, else_ir);
			push_ir(items, ir);
		}
		break;

		case AST_WHILE:
		{
			Ast pred = stat->while_stat.pred;
			Ast body = stat->while_stat.body;

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

			IrFrame loop_items = begin_ir_frame(ctx);
			push_ir(&loop_items, start_label);
			push_ir(&loop_items, jump_if_false);
			push_ir(&loop_items, body_ir);
			push_ir(&loop_items, create_jump_ir(ctx, stat->site, start_label));
			push_ir(&loop_items, break_label);

			IrArray loop_stats = close_ir_frame(&loop_items);
			Ir ir = create_block_ir(ctx, stat->site, loop_stats);
			push_ir(items, ir);
		}
		break;

		case AST_FOR:
		{
			Ir ir = lower_ast_for_to_ir(ctx, stat);
			push_ir(items, ir);
		}
		break;

		case AST_FOR_RANGE:
		{
			Ir ir = lower_ast_for_range_to_ir(ctx, stat);
			push_ir(items, ir);
		}
		break;

		case AST_BLOCK_STAT:
		{
			push_ir(items, lower_ast_to_ir_block(ctx, stat));
		}
		break;

		case AST_STATIC_DECL:
		{
			report_lowering_error(ctx, LOWERING_ERROR_GENERIC, stat->site, "static declaration must be at file level");
			push_ir(items, ERROR_IR);
		}
		break;

		case AST_DECL_STAT:
		{
			Ast name_tuple = check_ast_type(stat->decl.name, AST_TUPLE);
			Ast expr_tuple = check_ast_type(stat->decl.expr, AST_TUPLE);
			if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid declaration");
				push_ir(items, ERROR_IR);
				break;
			}

			u32 entity_tags = stat->decl.tags & AST_DECL_TAG_CONSTANT ? ENTITY_TAG_CONSTANT : 0;
			lower_local_decl_tuples_to_ir(ctx, items, name_tuple, expr_tuple, entity_tags);
		}
		break;

		case AST_ASSIGN:
		{
			Ast dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid assignment");
				push_ir(items, ERROR_IR);
				break;
			}

			u32 nargs = dest_tuple->tuple.nargs;
			if (nargs == 1)
			{
				Ast dest_ast = dest_tuple->tuple.args[0];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					break;
				}
				Ast expr_ast = expr_tuple->tuple.nargs ? expr_tuple->tuple.args[0] : 0;

				Ir dest_ir = lower_ast_expr_to_ir(ctx, dest_ast);
				Ir expr_ir = expr_ast ? lower_ast_expr_to_ir(ctx, expr_ast) : create_nil_ir(ctx, dest_ast->site);
				Ir ir = create_store_ir(ctx, stat->site, dest_ir, expr_ir);
				push_ir(items, ir);
				break;
			}

			// TODO(RJ): we don't need multi-local! could be done with the same mechanism!
			Ir *values = elf_arena_push(ctx->module->arena, sizeof(*values) * nargs);
			if (expr_tuple->tuple.nargs == 1 && ast_is_call(expr_tuple->tuple.args[0]))
			{
				Ast expr_ast = expr_tuple->tuple.args[0];
				Ir expr_ir = lower_ast_expr_to_ir(ctx, expr_ast);
				for (u32 i = 0; i < nargs; ++i) {
					values[i] = create_local_ir(ctx, expr_ast->site, 0);
				}
				push_ir(items, create_multi_local_ir(ctx, expr_ast->site, expr_ir, (IrArray){ .items = values, .count = nargs }));
			}
			else
			{
				for (u32 i = 0; i < nargs; ++ i)
				{
					SourceSite site = stat->site;
					Ir expr_ir;
					if (i < expr_tuple->tuple.nargs) {
						Ast expr_ast = expr_tuple->tuple.args[i];
						site = expr_ast->site;
						expr_ir = lower_ast_expr_to_ir(ctx, expr_ast);
					}
					else {
						expr_ir = create_nil_ir(ctx, site);
					}

					values[i] = create_local_ir(ctx, site, expr_ir);
					push_ir(items, values[i]);
				}
			}

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast dest_ast = dest_tuple->tuple.args[i];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					continue;
				}
				Ir dest_ir = lower_ast_expr_to_ir(ctx, dest_ast);
				Ir value_ir = create_load_local_ir(ctx, values[i]->site, values[i]);
				Ir ir = create_store_ir(ctx, stat->site, dest_ir, value_ir);
				push_ir(items, ir);
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
			push_ir(items, ir);
		}
		break;

		case AST_NIL_ASSIGN:
		{
			Ast dest_tuple = check_ast_type(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = check_ast_type(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, LOWERING_ERROR_INTERNAL, stat->site, "invalid nil assignment");
				push_ir(items, ERROR_IR);
				break;
			}

			for (u32 i = 0; i < dest_tuple->tuple.nargs; ++ i)
			{
				if (i >= expr_tuple->tuple.nargs) {
					ASSERT(!"Error");
				}

				Ast dest_ast = dest_tuple->tuple.args[i];
				Ast expr_ast = expr_tuple->tuple.args[i];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					continue;
				}

				Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
				Ir nil_ir = create_nil_ir(ctx, dest_ast->site);
				Ir pred_ir = ir_new_binary(ctx, stat->site, IR_EQ, dest_ir, nil_ir);
				Ir value_ir = lower_ast_expr_to_ir(ctx, expr_ast);
				Ir store_ir = create_store_ir(ctx, stat->site, dest_ir, value_ir);

				IrFrame true_items = begin_ir_frame(ctx);
				push_ir(&true_items, store_ir);
				IrArray true_stats = close_ir_frame(&true_items);
				Ir true_clause = create_block_ir(ctx, stat->site, true_stats);
				Ir ir = create_if_ir(ctx, stat->site, pred_ir, true_clause, 0);
				push_ir(items, ir);
			}
		}
		break;

		case AST_RETURN:
		{
			Ast expr = stat->return_stat.expr;
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
				push_ir(items, return_value);
				expr_ir = create_load_local_ir(ctx, stat->site, return_value);
			}
			emit_defer_range(ctx, items, ctx->function_defer_start);
			Ir ir = create_return_ir(ctx, stat->site, expr_ir);
			push_ir(items, ir);
		}
		break;

		case AST_BREAK:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = create_jump_ir(ctx, stat->site, loop_labels.break_label);
			push_ir(items, ir);
		}
		break;

		case AST_CONTINUE:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = create_jump_ir(ctx, stat->site, loop_labels.continue_label);
			push_ir(items, ir);
		}
		break;

		case AST_DEFER_STAT:
		{
			push_defer_stat(ctx, stat->defer_stat.body);
		}
		break;

		case AST_TUPLE:
		{
			Ast *args = stat->tuple.args;
			u32 nargs = stat->tuple.nargs;

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ir ir = lower_ast_expr_to_ir(ctx, args[i]);
				push_ir(items, ir);
			}
		}
		break;

		default:
		{
			Ir ir = lower_ast_expr_to_ir(ctx, stat);
			push_ir(items, ir);
		}
		break;
	}
}
