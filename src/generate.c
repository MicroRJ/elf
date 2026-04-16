

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define IMPLICIT_PARAM_INDEX 0
#define IMPLICIT_PARAM_COUNT 1

#define NO_SLOT (-1)


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static GenMemorySlot generate_ast_expr(BytecodeGen *gen, AstRef expr, GenMemorySlot slots, u32 nslots);
static void generate_ast_stat(BytecodeGen *gen, AstRef stat);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define report_generation_error(gen, error, site, format, ...) report_generation_error_(gen, error, site, temporay_format(format, __VA_ARGS__))
static void report_generation_error_(BytecodeGen *gen, GenerationError error, Source site, const char *message)
{
	printf("generation error: %s\n", message);
	ASSERT(!"Generation Error");

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void check_ast_type(AstRef ast, AstType type)
{
	ASSERT(ast);
	ASSERT(ast->kind == type);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void begin_scope(BytecodeGen *par)
{
	ASSERT(par->scope_index < COUNTOF(par->scope_stack));
	par->scope_stack[par->scope_index ++] = par->entity_index;
	par->scope ++;
}

static void close_scope(BytecodeGen *par)
{
	ASSERT(par->scope_index > 0);

	i32 new_entity_index = par->scope_stack[-- par->scope_index];
	for (i32 i = par->entity_index - 1; i >= new_entity_index; -- i)
	{
		if (~par->entities[i].tags & ENTITY_TAG_REFERENCED) {
			report_generation_error(par, GENERATION_ERROR_UNREFERENCED_ENTITY, par->entities[i].site, "warning: unreferenced entity");
		}
	}

	ASSERT(new_entity_index <= par->entity_index);

	par->entity_index = new_entity_index;
	par->scope --;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// par->entities[id].tags |= ENTITY_TAG_REFERENCED;

static Entity *identify_name(BytecodeGen *gen, char *name)
{
	for (i32 i = gen->entity_index - 1; i >= 0; -- i)
	{
		Entity *en = & gen->entities[i];
		if (text_eq(en->name, name)) {
			return en;
		}
	}
	return 0;
}

static Entity *decl_entity(BytecodeGen *gen, Source site, EntityType type, u32 tags, char *name)
{
	Entity *en = identify_name(gen, name);
	if (en)
	{
		if (en->type == ENTITY_DIRECTORY) {
			report_generation_error(gen, GENERATION_ERROR_GENERIC, site, "'%s': a directory with this name already exits", name);
		}
		// declared within the same scope
		if (en->scope == gen->scope) {
			report_generation_error(gen, GENERATION_ERROR_GENERIC, site, "'%s': is already declared", name);
		}
		// declared outside of this function
		//	else if (en->scope >= gen->function->ast_function.scope) {
		//		report_generation_error(gen, site, "'%s': this declaration shadows another one", name);
		//	}
	}


	ASSERT(gen->entity_index < MAX_ENTITIES);

	en = & gen->entities[gen->entity_index ++];
	zero_memory(en, sizeof(* en));
	en->type = type;
	en->tags = tags;
	en->name = name;
	en->site = site;
	en->scope = gen->scope;
	return en;
}

// static AstRef tree_from_identifier(Parser *parser, Source line, char *name, b32 lval)
// {
// 	AstRef v = Y_NULL;

// 	EntityId id = identify_name(parser, name);

// 	if (id != NO_ENTITY)
// 	{
// 		Entity entity = parser->entities[id];

// 		if (!lval)
// 		{
// 			if (~entity.tags & ENTITY_TAG_ASSIGNED)
// 			{
// 				parser_dialog(parser, parser->tok.line, "warning: usage of possibly unassigned variable");
// 			}
// 		}


// 		// check if we have to capture this thing
// 		AstRef enc = parser->function;

// 		if (entity.scope < enc->ast_function.scope)
// 		{
// 			i32 index = -1;

// 			// Check if we've captured this already
// 			FOR_ARRAY(i, enc->ast_function.capts)
// 			{
// 				if (enc->ast_function.capts[i] == entity.tree)
// 				{
// 					index = i;
// 					break;
// 				}
// 			}

// 			// Capture it if not
// 			if (index == -1)
// 			{
// 				index = heap_array_length(enc->ast_function.capts);
// 				heap_array_add(enc->ast_function.capts, entity.tree);
// 			}

// 			v = tree_closure_value(parser, line, index);

// 			// goto next function
// 			enc = enc->ast_function.enc;

// 			if (entity.scope < enc->ast_function.scope)
// 			{
// 				parser_dialog(parser, line, "cannot capture?");
// 			}

// 		}
// 		else
// 		{
// 			v = tree_proxy(parser, line, entity.tree);
// 		}

// 	}
// 	else
// 	{
// 		// todo: we can only do this after we implement directories
// 		// because otherwise all of our libs break!
// 		// parser_dialog(parser, line, "undeclared identifier, if this is a global use the 'global' keyword or declare a global");
// 		v = tree_global_symbol(parser, line, name);
// 	}
// 	return v;
// }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BytecodeGen *elf_create_bytecode_generator(elf_State *state, elf_Arena *arena)
{
	BytecodeGen *gen = elf_arena_push_zero(arena, sizeof(*gen));
	gen->state = state;
	gen->arena = arena;
	gen->scratch_arena = arena;
	gen->scratch_parser = elf_alloc_parser(state, arena);

	// Todo, dude ...
	u32 max_functions = 1024;
	gen->functions = elf_arena_push_zero(arena, sizeof(*gen->functions) * max_functions);
	gen->max_functions = max_functions;


	u32 bytecode_buffer_capacity = 1 << 13;
	gen->bytecode_buffer.bytecode = elf_arena_push_zero(arena, sizeof(*gen->bytecode_buffer.bytecode) * bytecode_buffer_capacity);
	gen->bytecode_buffer.capacity = bytecode_buffer_capacity;
	gen->bytecode_buffer.position = 0;
	return gen;
}

static BytecodeFunction generate_bytecode_function(BytecodeGen *gen, GenFunction function)
{
	u32 arity = function.arity;
	AstRef body = function.body;
	GenFunctionParam *params = function.params;
	b32 variadic = !! (function.tags & FUNCTION_VARIADIC);

	ASSERT(gen->bytecode_buffer.position == 0);


	ENTITY_SCOPE(gen)
	{
		MEMORY_STATE_SCOPE(gen)
		{
			decl_entity(gen, 0, ENTITY_DIRECTORY, ENTITY_TAG_CONSTANT|ENTITY_TAG_REFERENCED|ENTITY_TAG_ASSIGNED, "elf");

			for (u32 i = 0; i < function.arity; ++ i)
			{
				Entity *en = decl_entity(gen, params[i].site, ENTITY_LOCAL_DECLARATION, params[i].tags, params[i].name);
				en->slot = allocate_slot(gen);
			}
			generate_ast_stat(gen, body);
		}
	}

	emit_return_bytecode(gen, body->site, 0, 0);


	u32 function_length = gen->bytecode_buffer.position;
	u32 function_offset = dynamic_array_allocate(gen->state->bytebuf, function_length);
	copy_memory(gen->state->bytebuf + function_offset, gen->bytecode_buffer.bytecode, sizeof(* gen->bytecode_buffer.bytecode) * function_length);


	BytecodeFunction bytecode_function =
	{
		.variadic   = variadic,
		.arity      = arity,
		.offset     = function_offset,
		.length     = function_length,
		.captures   = gen->ncaptures,
		.stack_size = gen->memory_usage,
	};

	// reset bytecode buffer
	gen->bytecode_buffer.position = 0;

	return bytecode_function;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static GenFunction *add_generate_function(BytecodeGen *gen, Source site, GenFunctionTags tags, u32 arity, AstRef body)
{
	// implicit this
	ASSERT(arity >= IMPLICIT_PARAM_COUNT);

	ASSERT(gen->num_functions < gen->max_functions);
	GenFunction *function = & gen->functions[gen->num_functions ++];
	function->site = site;
	function->tags = tags;
	function->arity = arity;
	function->body = body;

	// Todo, 'this' is to be replaced with just '.'
	function->params = elf_arena_push_zero(gen->arena, sizeof(*function->params) * arity);
	function->params[0].name = "this";
	function->params[0].type = TYPE_RULE_ANYTHING;
	function->params[0].tags = ENTITY_TAG_PARAMETER|ENTITY_TAG_ASSIGNED|ENTITY_TAG_CONSTANT|ENTITY_TAG_REFERENCED;

	return function;
}

static AstRef preprocess_ast_stat_for_generation(BytecodeGen *gen, AstRef stat);
static BytecodeFunction elf_generate_ast_file(BytecodeGen *gen, AstRef file)
{
	GenFunction *gen_file_function = add_generate_function(gen, 0, FUNCTION_VARIADIC, IMPLICIT_PARAM_COUNT, 0);

	AstRef new_file_body = preprocess_ast_stat_for_generation(gen, file->ast_file.body);
	gen_file_function->site = new_file_body->site;
	gen_file_function->body = new_file_body;

	// allocate bytecode functions within the dynamic module ...
	u32 bytecode_function_offset_in_module = dynamic_array_allocate(gen->state->protos, gen->num_functions);
	BytecodeFunction *first_bytecode_function = gen->state->protos + bytecode_function_offset_in_module;

	gen->bytecode_function_offset_in_module = bytecode_function_offset_in_module;


	for (u32 i = 0; i < gen->num_functions; ++ i)
	{
		BytecodeFunction bytecode_function = generate_bytecode_function(gen, gen->functions[i]);
		first_bytecode_function[i] = bytecode_function;

		print_bytecode_function(gen->state, bytecode_function);

		ASSERT(gen->memory_state == 0);
		ASSERT(gen->memory_state_index == 0);
		gen->memory_usage = 0;
	}

	return * first_bytecode_function;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Todo, HACK!
static b32 is_field_expr_actually_a_global__hack(AstRef expr)
{
	b32 success = false;
	if (expr->kind == AST_FIELD)
	{
		AstRef x = expr->ast_binary_expr.x;

		if (x->kind == AST_IDENT)
		{
			success = text_eq(x->ast_ident_expr, "elf");
		}
		else
		{
			success = is_field_expr_actually_a_global__hack(x);
		}
	}
	return success;
}

static void field_expr_to_global_identifier(AstRef expr, char *buffer)
{
	if (expr->kind == AST_FIELD)
	{
		AstRef x = expr->ast_binary_expr.x;
		AstRef y = expr->ast_binary_expr.y;
		field_expr_to_global_identifier(x, buffer);
		strcat(buffer, ".");
		field_expr_to_global_identifier(y, buffer);
	}
	else if (expr->kind == AST_IDENT)
	{
		strcat(buffer, expr->ast_ident_expr);
	}
	else
	{
		ASSERT(!"Error");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef preprocess_ast_expr_for_generation(BytecodeGen *gen, AstRef expr)
{
	AstRef new_expr = expr;

	switch (expr->kind)
	{
		case AST_INTEGER_LITERAL:
		case AST_NUMBER_LITERAL:
		case AST_STRING_LITERAL:
		case AST_IDENT:
		{
		}
		break;

		case AST_FUNCTION:
		{
			AstRef *params = expr->ast_function.params;
			u32    nparams = expr->ast_function.nparams;
			AstRef    body = expr->ast_function.body;

			// Implicit this ...
			u32 arity = IMPLICIT_PARAM_COUNT + nparams;
			GenFunctionTags tags = 0;
			if (nparams > 0 && params[nparams - 1]->kind == AST_ELLIPSIS) {
				tags |= FUNCTION_VARIADIC;
				arity -= 1;
			}


			GenFunction *gen_function = add_generate_function(gen, expr->site, tags, arity, 0);
			gen_function->function_ast = expr;

			for (u32 i = IMPLICIT_PARAM_INDEX + 1; i < nparams; ++ i)
			{
				AstRef param = params[i];
				check_ast_type(param, AST_FUNCTION_PARAM);

				AstRef param_name = param->ast_function_param.name;
				check_ast_type(param_name, AST_IDENT);

				gen_function->params[i].name = param_name->ast_ident_expr;
				gen_function->params[i].site = param_name->site;
				gen_function->params[i].tags = ENTITY_TAG_PARAMETER;
			}

			gen_function->body = preprocess_ast_stat_for_generation(gen, body);
		}
		break;

		case AST_CALL:
		{
			AstRef func = expr->ast_call_expr.expr;
			AstRef *args = expr->ast_call_expr.args;
			u32 nargs = expr->ast_call_expr.nargs;

			AstRef new_func = preprocess_ast_expr_for_generation(gen, func);

			AstRef *new_args = elf_arena_push(gen->arena, sizeof(*new_args) * nargs);
			for (u32 i = 0; i < nargs; ++ i) {
				new_args[i] = preprocess_ast_expr_for_generation(gen, args[i]);
			}

			new_expr = create_call_ast(gen->scratch_parser, expr->site, new_func, new_args, nargs);
		}
		break;

		case AST_TUPLE:
		{
			AstRef *args = expr->ast_tuple_expr.args;
			u32 nargs = expr->ast_tuple_expr.nargs;

			AstRef *new_args = elf_arena_push(gen->arena, sizeof(*new_args) * nargs);
			for (u32 i = 0; i < nargs; ++ i) {
				new_args[i] = preprocess_ast_expr_for_generation(gen, args[i]);
			}

			new_expr = create_tuple_ast(gen->scratch_parser, expr->site, new_args, nargs);
		}
		break;

		case AST_META_FIELD:
		{
			AstRef x = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.x);
			AstRef y = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.y);
			new_expr = create_binary_expr_ast(gen->scratch_parser, expr->site, expr->kind, x, y);
		}
		break;

		case AST_FIELD:
		{
			if (is_field_expr_actually_a_global__hack(expr))
			{
				// Todo, dude use atoms!
				char name[256] = {0};
				field_expr_to_global_identifier(expr, name);

				char *copy = elf_arena_push_copy(gen->arena, strlen(name) + 1, name);

				new_expr = create_ident_ast(gen->scratch_parser, expr->site, copy);
			}
			else
			{
				AstRef x = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.x);
				AstRef y = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.y);
				new_expr = create_binary_expr_ast(gen->scratch_parser, expr->site, expr->kind, x, y);
			}
		}
		break;

		case AST_GREATER_THAN_EQ:
		{
			AstRef x = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.x);
			AstRef y = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.y);
			new_expr = create_binary_expr_ast(gen->scratch_parser, expr->site, AST_LESS_THAN_EQ, y, x);
		}
		break;

		case AST_GREATER_THAN:
		{
			AstRef x = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.x);
			AstRef y = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.y);
			new_expr = create_binary_expr_ast(gen->scratch_parser, expr->site, AST_LESS_THAN, y, x);
		}
		break;

		case AST_ASSIGN:

		case AST_LESS_THAN_EQ:
		case AST_LESS_THAN:
		case AST_EQ:
		case AST_NOT_EQ:
		case AST_DIV:
		case AST_MUL:
		case AST_MOD:
		case AST_SUB:
		case AST_ADD:
		case AST_POW:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_XOR:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		{
			AstRef x = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.x);
			AstRef y = preprocess_ast_expr_for_generation(gen, expr->ast_binary_expr.y);
			new_expr = create_binary_expr_ast(gen->scratch_parser, expr->site, expr->kind, x, y);
		}
		break;
		default:
		{
			report_generation_error(gen, GENERATION_ERROR_INTERNAL, "'%s' is not an expression", Static_StrFromAstType[expr->kind]);
			ASSERT(!"Internal Error!");
		}
		break;
	}
	return new_expr;
}

static AstRef preprocess_ast_stat_for_generation(BytecodeGen *gen, AstRef stat)
{
	AstRef new_stat = stat;

	switch (stat->kind)
	{
		case AST_WHILE:
		{
			AstRef pred = stat->ast_while_stat.pred;
			AstRef body = stat->ast_while_stat.body;

			AstRef new_pred = preprocess_ast_expr_for_generation(gen, pred);
			AstRef new_body = preprocess_ast_stat_for_generation(gen, body);

			new_stat = create_while_ast(gen->scratch_parser, stat->site, new_pred, new_body);
		}
		break;
		case AST_BLOCK_STAT:
		{
			AstRef *stats = stat->ast_block_stat.stats;
			u32 nstats = stat->ast_block_stat.nstats;

			AstRef *new_block_stats = elf_arena_push_zero(gen->arena, sizeof(*new_block_stats) * nstats);

			for (u32 i = 0; i < nstats; ++ i)
			{
				new_block_stats[i] = preprocess_ast_stat_for_generation(gen, stats[i]);
			}

			new_stat = create_block_ast(gen->scratch_parser, stat->site, new_block_stats, nstats);
		}
		break;

		case AST_DECL_STAT:
		{

		}
		break;

		default:
		{
			new_stat = preprocess_ast_expr_for_generation(gen, stat);
		}
		break;
	}

	return new_stat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static GenMemorySlot allocate_slots(BytecodeGen *gen, u32 nslots)
{
	ASSERT(nslots != 0);
	ASSERT(gen->memory_state + nslots < _countof(gen->memory_slots));
	GenMemorySlot mem = gen->memory_state;
	gen->memory_state += nslots;
	if (gen->memory_usage < gen->memory_state) {
		gen->memory_usage = gen->memory_state;
	}
	return mem;
}

static int allocate_slot(BytecodeGen *gen)
{
	return allocate_slots(gen, 1);
}

static int gen_get_stack_ptr(BytecodeGen *par)
{
	return par->memory_state;
}

static void push_mem_state_(BytecodeGen *par)
{
	ASSERT(par->memory_state_index < _countof(par->memory_state_stack));
	par->memory_state_stack[par->memory_state_index ++] = gen_get_stack_ptr(par);
}

static void pop_mem_state_(BytecodeGen *par)
{
	ASSERT(par->memory_state_index > 0);

	i32 prev_mem_state = par->memory_state;

	par->memory_state = par->memory_state_stack[-- par->memory_state_index];

	ASSERT(prev_mem_state >= par->memory_state);

	for (i32 i = prev_mem_state - 1; i >= par->memory_state; -- i) {
		par->memory_slots[i] = Y_NULL;
	}
}

static i32 get_expr_mem(BytecodeGen *par, AstRef expr)
{
	i32 mem = -1;
	for (i32 i = 0; i < par->memory_state; ++ i)
	{
		if (par->memory_slots[i] == expr) {
			mem = i; break;
		}
	}
	return mem;
}

static int expr_to_any_mem(BytecodeGen *gen, AstRef expr)
{
	GenMemorySlot slot = NO_SLOT;
	if (expr->kind == AST_IDENT)
	{
		Entity *en = identify_name(gen, expr->ast_ident_expr);
		if (en && en->type == ENTITY_LOCAL_DECLARATION) {
			slot = en->slot;
		}
	}
	if (slot == NO_SLOT) {
		slot = generate_ast_expr(gen, expr, -1, 1);
	}
	ASSERT(slot != NO_SLOT);
	return slot;
}

static GenMemorySlot generate_call_expr(BytecodeGen *gen, AstRef ast, GenMemorySlot slots, u32 nslots)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	AstRef expr = ast->ast_call_expr.expr;
	AstRef *args = ast->ast_call_expr.args;
	u32 nargs = ast->ast_call_expr.nargs;
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	u32 mem = gen_get_stack_ptr(gen);
	MEMORY_STATE_SCOPE(gen)
	{
		u32 rx, ry;
		if (expr->kind == AST_META_FIELD)
		{
			check_ast_type(expr, AST_META_FIELD);
			check_ast_type(expr->ast_binary_expr.y, AST_IDENT);

			GenMemorySlot ry = allocate_slots(gen, 1);
			emit_load_constant_str(gen, expr->ast_binary_expr.y->site, ry, expr->ast_binary_expr.y->ast_ident_expr);

			// the object becomes 'this'
			rx = generate_ast_expr(gen, expr->ast_binary_expr.x, -1, 1);

			emit_bytexyz(gen, expr->site, BYTECODE_GETMETAFIELD, ry, rx, ry);
		}
		else if (expr->kind == AST_FIELD)
		{
			check_ast_type(expr, AST_FIELD);
			check_ast_type(expr->ast_binary_expr.y, AST_IDENT);

			GenMemorySlot ry = allocate_slots(gen, 1);
			emit_load_constant_str(gen, expr->ast_binary_expr.y->site, ry, expr->ast_binary_expr.y->ast_ident_expr);

			// the object becomes 'this'
			rx = generate_ast_expr(gen, expr->ast_binary_expr.x, -1, 1);

			emit_bytexyz(gen, expr->site, BYTECODE_GETFIELD, ry, rx, ry);
		}
		else
		{
			ry = generate_ast_expr(gen, expr, -1, 1);
			// load the current 'this' and pass it in, implicit 'this'
			rx = allocate_slot(gen);

			emit_bytexy(gen, ast->site, BYTECODE_RELOAD, rx, 0);
		}

		ASSERT(ry == mem + 0);
		ASSERT(rx == mem + 1);

		for (u32 i = 0; i < nargs; ++ i)
		{
			u32 rz = generate_ast_expr(gen, args[i], -1, 1);
			ASSERT(rz == mem + 2 + i);
		}
	}

	emit_bytexyz(gen, expr->site, BYTECODE_CALL, mem, nargs + 1, nslots);

	if (nslots < 1) goto esc;

	// Todo, allocate memory for the number of expected returns
	if (slots < 0)
	{
		slots = allocate_slot(gen);
	}

	if (slots != mem)
	{
		if (nslots > 1)
		{
			report_generation_error(gen, GENERATION_ERROR_GENERIC, expr->site, "multi-returns are not fully supported yet!");
		}

		emit_bytexy(gen, expr->site, BYTECODE_RELOAD, slots, mem);
	}

	esc:
	return slots;
}

static u32 bytecode_type_from_ast_expr_type(AstType type)
{
	switch (type)
	{
		case AST_ADD:               return BYTECODE_ADD;
		case AST_SUB:               return BYTECODE_SUB;
		case AST_DIV:               return BYTECODE_DIV;
		case AST_MUL:               return BYTECODE_MUL;
		case AST_POW:               return BYTECODE_POW;
		case AST_MOD:               return BYTECODE_MOD;
		case AST_NOT_EQ:            return BYTECODE_NEQ;
		case AST_EQ:                return BYTECODE_EQ;
		case AST_LESS_THAN:         return BYTECODE_LT;
		case AST_LESS_THAN_EQ:      return BYTECODE_LTEQ;
		case AST_BITWISE_NOT:       return BYTECODE_BIT_NOT;
		case AST_BITWISE_OR:        return BYTECODE_BIT_OR;
		case AST_BITWISE_AND:       return BYTECODE_BIT_AND;
		case AST_SHIFT_LEFT:        return BYTECODE_BIT_SHL;
		case AST_SHIFT_RIGHT:       return BYTECODE_BIT_SHR;
		case AST_BITWISE_XOR:       return BYTECODE_BIT_XOR;
		default: NO_CODE;
	}

	ASSERT(!"Error");
	return BYTECODE_HALT;
}

static GenMemorySlot generate_binary_ast_expr(BytecodeGen *gen, AstRef expr, GenMemorySlot slots, u32 nslots)
{
	GenMemorySlot rx, ry;
	MEMORY_STATE_SCOPE(gen)
	{
		rx = expr_to_any_mem(gen, expr->ast_binary_expr.x);
		ry = expr_to_any_mem(gen, expr->ast_binary_expr.y);
	}
	if (nslots < 1) goto esc;
	if (slots < 0) slots = allocate_slot(gen);
	emit_bytexyz(gen, expr->site, bytecode_type_from_ast_expr_type(expr->kind), slots, rx, ry);
	esc:
	return slots;
}

static GenMemorySlot generate_ast_expr(BytecodeGen *gen, AstRef expr, GenMemorySlot slots, u32 nslots)
{
	Source site = expr->site;
	switch (expr->kind)
	{
		case AST_FUNCTION:
		{
			if (nslots < 1) goto esc;

			// Todo, we could have just assigned an id to each function ast to remove this lookup entirely ...
			u32 bytecode_function_id = -1;
			for (u32 i = 0; i < gen->num_functions; ++ i)
			{
				if (expr == gen->functions[i].function_ast)
				{
					bytecode_function_id = gen->bytecode_function_offset_in_module + i;
					break;
				}
			}
			ASSERT(bytecode_function_id != -1);

			if (slots < 0) slots = allocate_slot(gen);

			u32 stack_ptr = gen_get_stack_ptr(gen);

			emit_bytexy(gen, site, BYTECODE_CLOSURE, stack_ptr, bytecode_function_id);

			if (stack_ptr != slots) {
				emit_bytexy(gen, expr->site, BYTECODE_RELOAD, slots, stack_ptr);
			}
		}
		break;

		case AST_IDENT:
		{
			if (nslots < 1) goto esc;
			if (slots < 0) slots = allocate_slot(gen);

			Entity *en = identify_name(gen, expr->ast_ident_expr);

			// Todo, dude remove this !!?
			if (en == 0)
			{
				V v;
				to_str(&v, new_string_from_data(gen->state, expr->ast_ident_expr));
				GenMemorySlot global_index = elf_table_ensure(gen->state, gen->state->globals, v);

				emit_get_global_bytecode(gen, expr->site, slots, global_index);
			}
			else if (en->type == ENTITY_LOCAL_DECLARATION)
			{
				emit_reload_bytecode(gen, expr->site, slots, en->slot);
			}
			else
			{
				report_generation_error(gen, GENERATION_ERROR_UNDECLARED_IDENTIFIER, expr->site, "'%s' is an undeclared identifier", expr->ast_ident_expr);
				ASSERT(!"Undeclared Identifier");
			}
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			if (nslots < 1) goto esc;
			if (slots < 0) slots = allocate_slot(gen);

			emit_load_constant_int(gen, expr->site, slots, expr->expr_int);
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			if (nslots < 1) goto esc;
			if (slots < 0) slots = allocate_slot(gen);

			emit_load_constant_num(gen, expr->site, slots, expr->expr_num);
		}
		break;

		case AST_STRING_LITERAL:
		{
			if (nslots < 1) goto esc;
			if (slots < 0) slots = allocate_slot(gen);

			emit_load_constant_str(gen, expr->site, slots, expr->expr_str);
		}
		break;

		case AST_CALL:
		{
			slots = generate_call_expr(gen, expr, slots, nslots);
		}
		break;

		// Todo, this is a bit of a special case, because we don't have enough context to handle tuples
		// by themselves,
		case AST_TUPLE:
		{
			u32 nargs = expr->ast_tuple_expr.nargs;
			AstRef *args = expr->ast_tuple_expr.args;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			for (u32 i = 0; i < nargs; ++ i) {
				generate_ast_expr(gen, args[i], NO_SLOT, 0);
			}
		}
		break;

		case AST_FIELD:
		{
			ASSERT(!"Error");
		}
		break;

		case AST_EQ:
		case AST_NOT_EQ:
		case AST_GREATER_THAN_EQ:
		case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN:
		case AST_LESS_THAN:
		case AST_DIV:
		case AST_MUL:
		case AST_MOD:
		case AST_SUB:
		case AST_ADD:
		case AST_POW:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_XOR:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		{
			slots = generate_binary_ast_expr(gen, expr, slots, nslots);
		}
		break;

		default:
		{

			report_generation_error(gen, GENERATION_ERROR_INVALID_EXPRESSION, expr->site
			, "invalid expression, got: %s", Static_StrFromAstType[expr->kind]);

			ASSERT(!"Error!");
		}
		break;
	}

	esc:
	return slots;
}

static void generate_ast_assign_stat(BytecodeGen *gen, Source site, AstRef dest, AstRef expr)
{
	MEMORY_STATE_SCOPE(gen)
	{
		switch (dest->kind)
		{
			// i, j, k = 1, ...  SAME AS i = 1, j = 1, k = 1
			case AST_TUPLE:
			{
				check_ast_type(expr, AST_TUPLE);

				u32 nargs = dest->ast_tuple_expr.nargs;
				for (u32 i = 0; i < nargs; ++ i)
				{
					AstRef dest1 = dest->ast_tuple_expr.args[i];
					u32 slots;
					if (i < expr->ast_tuple_expr.nargs)
					{
						AstRef expr1 = expr->ast_tuple_expr.args[i];
						generate_ast_assign_stat(gen, dest->site, dest1, expr1);
					}
					else
					{
						ASSERT(!"Error");
					}
				}

			}
			break;

			case AST_IDENT:
			{
				Entity *en = identify_name(gen, dest->ast_ident_expr);

				// Todo, dude remove this !!?
				if (en == 0)
				{
					V v;
					to_str(&v, new_string_from_data(gen->state, dest->ast_ident_expr));
					GenMemorySlot rx = elf_table_ensure(gen->state, gen->state->globals, v);


					GenMemorySlot ry = expr_to_any_mem(gen, expr);
					emit_bytexy(gen, dest->site, BYTECODE_SETGLOBAL, rx, ry);
				}
				else if (en->type == ENTITY_LOCAL_DECLARATION)
				{
					GenMemorySlot rx = en->slot;
					ASSERT(rx != NO_SLOT);

					GenMemorySlot ry = generate_ast_expr(gen, expr, rx, 1);
					ASSERT(ry == rx);
				}
				else
				{
					ASSERT(!"Undeclared Identifier");
				}
			}
			break;

			case AST_FIELD:
			{
				AstRef field_left = dest->ast_binary_expr.x;
				AstRef field_name = dest->ast_binary_expr.y;
				check_ast_type(field_name, AST_IDENT);

				GenMemorySlot rz = expr_to_any_mem(gen, expr);
				GenMemorySlot rx = expr_to_any_mem(gen, field_left);

				GenMemorySlot ry = allocate_slots(gen, 1);
				emit_load_constant_str(gen, field_name->site, ry, field_name->ast_ident_expr);

				emit_bytexyz(gen, site, BYTECODE_SETFIELD, rx, ry, rz);
			}
			break;

			case AST_INDEX:
			{
				GenMemorySlot rz = expr_to_any_mem(gen, expr);
				GenMemorySlot rx = expr_to_any_mem(gen, dest->ast_binary_expr.x);
				GenMemorySlot ry = expr_to_any_mem(gen, dest->ast_binary_expr.y);
				emit_bytexyz(gen, site, BYTECODE_SETINDEX, rx, ry, rz);
			}
			break;
			//	case TREE_UPVALUE:
			//	{
			//		push_error(gen, ERROR_INVALID_STORE_CLOSURE_VALUE, cur, "assignment of closure value is not possible");
			//	}
			//	break;
			//	case AST_META_FIELD:
			//	{
			//		push_error(gen, ERROR_INVALID_STORE_METAFIELD, cur, "assignment of metafields is not possible");
			//	}
			//	break;
			default:
			{
				report_generation_error(gen, GENERATION_ERROR_INVALID_LVALUE, dest->site
				, "invalid l-value, got: %s", Static_StrFromAstType[dest->kind]);
			}
			break;
		}
	}
}

static void generate_ast_stat(BytecodeGen *gen, AstRef stat)
{
	switch (stat->kind)
	{
		case AST_WHILE:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef pred = stat->ast_while_stat.pred;
			AstRef body = stat->ast_while_stat.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			MEMORY_STATE_SCOPE(gen)
			{
				u32 entry = gen->bytecode_buffer.position;

				jumpS js = {0};
				emit_jump_if_false(gen, &js, pred);

				generate_ast_stat(gen, body);

				emit_jump(gen, body->site, entry);

				patch_jumps(gen, js.f);
				free_heap_array(js.f);
				js.f = 0;
			}
		}
		break;

		case AST_BLOCK_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef *stats = stat->ast_block_stat.stats;
			u32 nstats = stat->ast_block_stat.nstats;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			ENTITY_SCOPE(gen)
			{
				MEMORY_STATE_SCOPE(gen)
				{
					for (u32 i = 0; i < nstats; ++ i)
					{
						generate_ast_stat(gen, stats[i]);
					}
				}
			}
		}
		break;

		case AST_ASSIGN:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef dest = stat->ast_binary_expr.x;
			AstRef expr = stat->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			generate_ast_assign_stat(gen, stat->site, dest, expr);
		}
		break;

		case AST_DECL_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef name_tuple = stat->ast_decl_stat.name;
			AstRef expr_tuple = stat->ast_decl_stat.expr;
			AstRef type = stat->ast_decl_stat.type;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			check_ast_type(name_tuple, AST_TUPLE);
			check_ast_type(expr_tuple, AST_TUPLE);

			u32 nslots = name_tuple->ast_tuple_expr.nargs;

			// Todo, also we need to handle ellipsis
			// i, j, k := 1, ...  SAME AS i := 1, j := 1, k := 1
			for (u32 i = 0; i < nslots; ++ i)
			{
				AstRef name = name_tuple->ast_tuple_expr.args[i];
				check_ast_type(name, AST_IDENT);

				u32 slots;
				if (i < expr_tuple->ast_tuple_expr.nargs)
				{
					AstRef expr = expr_tuple->ast_tuple_expr.args[i];
					slots = generate_ast_expr(gen, expr, -1, nslots - i);
				}
				else
				{
					// Todo, warn excess
					slots = allocate_slot(gen);
					emit_load_nil(gen, name->site, slots);
				}

				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				// ~ declare entity
				Entity *en = decl_entity(gen, name->site, ENTITY_LOCAL_DECLARATION, 0, name->ast_ident_expr);
				en->slot = slots;
				en->tree = name;
			}
		}
		break;
		default:
		{
			MEMORY_STATE_SCOPE(gen)
			{
				generate_ast_expr(gen, stat, NO_SLOT, 0);
			}
		}
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int emit_branch_if(BytecodeGen *gen, jumpS *js, b32 if_true, AstRef expr)
{
	int jmp;
	switch (expr->kind)
	{
		case AST_AND:
		{
			emit_jump_if_false(gen, js, expr->ast_binary_expr.x);
			jmp = emit_branch_if(gen, js, if_true, expr->ast_binary_expr.y);
		}
		break;
		case AST_OR:
		{
			emit_jump_if_true(gen, js, expr->ast_binary_expr.x);
			jmp = emit_branch_if(gen, js, if_true, expr->ast_binary_expr.y);
		}
		break;
		default:
		{
			u32 mem;
			MEMORY_STATE_SCOPE(gen)
			{
				mem = expr_to_any_mem(gen,expr);
			}
			if (if_true)
			{
				jmp=emit_bytexy(gen,expr->site,BYTECODE_JNZ,NO_JUMP,mem);
				heap_array_add(js->t,jmp);
			}
			else
			{
				jmp=emit_bytexy(gen,expr->site,BYTECODE_JZ,NO_JUMP,mem);
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
	// return emit_jump_if_false(parser,js,create_binary_expr_ast(parser,site,AST_EQ,id,create_nil_ast(parser,site)));
	return 0;
}

// todo: dedicated instructions?
static inline int *emit_jump_if_nil(BytecodeGen *parser, Source site, jumpS *js, AstRef id)
{
	__debugbreak();
	// return emit_jump_if_true(parser,js,create_binary_expr_ast(parser,site,AST_EQ,id,create_nil_ast(parser,site)));
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
