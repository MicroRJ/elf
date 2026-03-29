


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static MemorySlot generate_ast_expr(BytecodeGen *gen, AstRef expr, MemorySlot slots, u32 nslots);
static void generate_ast_stat(BytecodeGen *gen, AstRef stat);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define NO_SLOT (-1)

typedef enum
{
	GENERATION_ERROR_GENERIC = 0,
	ERROR_UNREFERENCED_ENTITY,
	GENERATION_ERROR_INVALID_LVALUE,
	GENERATION_ERROR_INVALID_EXPRESSION,
}
GenerationError;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define report_generation_error(gen, error, site, format, ...) report_generation_error_(gen, error, site, temppf(format, __VA_ARGS__))
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
			report_generation_error(par, ERROR_UNREFERENCED_ENTITY, par->entities[i].site, "warning: unreferenced entity");
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

//	static void add_this_param(Parser *parser, Source line) {
//		AstRef y = tree_nop(parser, line);
//
//		AstRef x = tree_memory(parser, line, y);
//		PushBlockStat(parser, x);
//
//		decl_entity(parser, line
//		, ENTITY_BIT_PARAMETER|ENTITY_TAG_ASSIGNED|ENTITY_TAG_CONSTANT|ENTITY_TAG_REFERENCED
//		, "this", x);
//	}


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

// static AstRef nametorval(Parser *parser, Source line, char *name) {
// 	return tree_from_identifier(parser, line, name, false);
// }

// static AstRef nametolval(Parser *parser, Source line, char *name) {
// 	return tree_from_identifier(parser, line, name, true);
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
	return gen;
}

static BytecodeFunction elf_generate_ast_function(BytecodeGen *gen, AstRef ast)
{
	u32 bytepos = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	u32 arity = ast->ast_function.arity + 1;
	AstRef body = ast->ast_function.body;
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	generate_ast_stat(gen, body);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	BytecodeFunction bytecode_function =
	{
		.bytes     = bytepos,
		.arity     = arity,
		.variadic  = ast->ast_function.variadic,
		.numbytes  = gen->state->bytecur - bytepos,
		.ncaptures = gen->ncaptures,
		.stacksize = gen->memory_usage,
	};
	return bytecode_function;
}

static BytecodeFunction elf_generate_ast_file(BytecodeGen *gen, AstRef ast)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	AstRef *functions = ast->ast_file.functions;
	u32 nfunctions = ast->ast_file.nfunctions;
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	begin_scope(gen);
	decl_entity(gen, 0, ENTITY_DIRECTORY, ENTITY_TAG_CONSTANT|ENTITY_TAG_REFERENCED|ENTITY_TAG_ASSIGNED, "elf");


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	gen->nentries = nfunctions;
	gen->entries = elf_arena_push_zero(gen->scratch_arena, sizeof(*gen->entries) * nfunctions);

	// Todo, ...
	u32 first_function_id = heap_array_grow(gen->state->protos, nfunctions);

	for (u32 i = 1; i < nfunctions; ++ i)
	{
		gen->entries[i].ast = functions[i];
		gen->entries[i].function_id = first_function_id + i;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	for (u32 i = 0; i < nfunctions; ++ i)
	{
		BytecodeFunction bytecode_function = elf_generate_ast_function(gen, functions[i]);
		gen->state->protos[first_function_id + i] = bytecode_function;

		ASSERT(gen->memory_state == 0);
		ASSERT(gen->memory_state_index == 0);
		gen->memory_usage = 0;
	}

	close_scope(gen);
	return gen->state->protos[first_function_id];
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
		strcpy(buffer, ".");
		field_expr_to_global_identifier(y, buffer);
	}
	else if (expr->kind == AST_IDENT)
	{
		strcpy(buffer, expr->ast_ident_expr);
	}
	else
	{
		ASSERT(!"Error");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef rewrite_ast_expr(BytecodeGen *gen, AstRef expr)
{
	AstRef rewr = expr;
	switch (expr->kind)
	{
		case AST_FIELD:
		{
			if (is_field_expr_actually_a_global__hack(expr))
			{
				char name[256];
				field_expr_to_global_identifier(expr, name);

				rewr = create_ident_ast(gen->scratch_parser, expr->site, name);
			}
		}
		break;
		case AST_IDENT:
		{
		}
		break;
		default:
		{
		}
		break;
	}
	return rewr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static MemorySlot allocate_slots(BytecodeGen *gen, u32 nslots)
{
	ASSERT(nslots != 0);
	ASSERT(gen->memory_state + nslots < _countof(gen->memory_slots));
	MemorySlot mem = gen->memory_state;
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

static int get_mem_state(BytecodeGen *par)
{
	return par->memory_state;
}

static void push_mem_state_(BytecodeGen *par)
{
	ASSERT(par->memory_state_index < _countof(par->memory_state_stack));
	par->memory_state_stack[par->memory_state_index ++] = get_mem_state(par);
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
	MemorySlot slot = NO_SLOT;
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

static MemorySlot generate_call_expr(BytecodeGen *gen, AstRef ast, MemorySlot slots, u32 nslots)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	AstRef expr = ast->ast_call_expr.expr;
	AstRef *args = ast->ast_call_expr.args;
	u32 nargs = ast->ast_call_expr.nargs;
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	u32 mem = get_mem_state(gen);
	MEMORY_STATE_SCOPE(gen)
	{
		u32 rx, ry;
		if (expr->kind == AST_META_FIELD)
		{
			check_ast_type(expr, AST_META_FIELD);
			check_ast_type(expr->ast_binary_expr.y, AST_IDENT);

			MemorySlot ry = allocate_slots(gen, 1);
			emit_load_constant_str(gen, expr->ast_binary_expr.y->site, ry, expr->ast_binary_expr.y->ast_ident_expr);

			// the object becomes 'this'
			rx = generate_ast_expr(gen, expr->ast_binary_expr.x, -1, 1);

			emit_bytexyz(gen, expr->site, BYTECODE_GETMETAFIELD, ry, rx, ry);
		}
		else if (expr->kind == AST_FIELD)
		{
			check_ast_type(expr, AST_FIELD);
			check_ast_type(expr->ast_binary_expr.y, AST_IDENT);

			MemorySlot ry = allocate_slots(gen, 1);
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

static MemorySlot generate_binary_ast_expr(BytecodeGen *gen, AstRef expr, MemorySlot slots, u32 nslots)
{
	MemorySlot rx, ry;
	if (expr->kind == AST_GREATER_THAN_EQ)
	{
		MEMORY_STATE_SCOPE(gen)
		{
			rx = expr_to_any_mem(gen, expr->ast_binary_expr.y);
			ry = expr_to_any_mem(gen, expr->ast_binary_expr.x);
		}
		if (nslots < 1) goto esc;
		if (slots < 0) slots = allocate_slot(gen);

		emit_bytexyz(gen, expr->site, BYTECODE_LTEQ, slots, rx, ry);
	}
	else if (expr->kind == AST_GREATER_THAN_EQ)
	{
		MEMORY_STATE_SCOPE(gen)
		{
			rx = expr_to_any_mem(gen, expr->ast_binary_expr.y);
			ry = expr_to_any_mem(gen, expr->ast_binary_expr.x);
		}
		if (nslots < 1) goto esc;
		if (slots < 0) slots = allocate_slot(gen);

		emit_bytexyz(gen, expr->site, BYTECODE_LT, slots, rx, ry);
	}
	else
	{
		MEMORY_STATE_SCOPE(gen)
		{
			rx = expr_to_any_mem(gen, expr->ast_binary_expr.x);
			ry = expr_to_any_mem(gen, expr->ast_binary_expr.y);
		}
		if (nslots < 1) goto esc;
		if (slots < 0) slots = allocate_slot(gen);
		emit_bytexyz(gen, expr->site, bytecode_type_from_ast_expr_type(expr->kind), slots, rx, ry);
	}

	esc:
	return slots;
}

static MemorySlot generate_ast_expr(BytecodeGen *gen, AstRef expr, MemorySlot slots, u32 nslots)
{
	Source site = expr->site;
	switch (expr->kind)
	{
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
		case AST_TUPLE_EXPR:
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
			if (is_field_expr_actually_a_global__hack(expr))
			{
				if (nslots < 1) goto esc;
				if (slots < 0) slots = allocate_slots(gen, 1);

				char name[256];
				field_expr_to_global_identifier(expr, name);

				V v;
				to_str(&v, _string_new(gen->state, name));
				u32 global_index = elf_table_ensure(gen->state, gen->state->globals, v);

				emit_bytexy(gen, expr->site, BYTECODE_LOADGLOBAL, slots, global_index);
			}
			else
			{
				ASSERT(!"Error");
			}
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
			, "invalid expression, got: %s", tree2s[expr->kind]);

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
			case AST_TUPLE_EXPR:
			{
				check_ast_type(expr, AST_TUPLE_EXPR);

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
					to_str(&v, _string_new(gen->state, dest->ast_ident_expr));
					MemorySlot rx = elf_table_ensure(gen->state, gen->state->globals, v);

					MemorySlot ry = expr_to_any_mem(gen, expr);
					emit_bytexy(gen, dest->site, BYTECODE_SETGLOBAL, rx, ry);
				}
				else if (en->type == ENTITY_LOCAL_DECLARATION)
				{
					MemorySlot rx = en->slot;
					ASSERT(rx != NO_SLOT);

					MemorySlot ry = generate_ast_expr(gen, expr, rx, 1);
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

				if (is_field_expr_actually_a_global__hack(dest))
				{
					char name[1024];
					field_expr_to_global_identifier(dest, name);

					V v;
					to_str(&v, _string_new(gen->state, name));
					Index rx = elf_table_ensure(gen->state, gen->state->globals, v);

					MemorySlot ry = expr_to_any_mem(gen, expr);
					emit_bytexy(gen, dest->site, BYTECODE_SETGLOBAL, rx, ry);
				}
				else
				{
					AstRef x = dest->ast_binary_expr.x;
					AstRef y = dest->ast_binary_expr.y;
					check_ast_type(y, AST_IDENT);

					MemorySlot rz = expr_to_any_mem(gen, expr);

					MemorySlot rx = expr_to_any_mem(gen, x);
					MemorySlot ry = allocate_slots(gen, 1);
					emit_load_constant_str(gen, y->site, ry, y->ast_ident_expr);

					// MemorySlot ry = expr_to_any_mem(gen, dest->ast_binary_expr.y);
					emit_bytexyz(gen, site, BYTECODE_SETFIELD, rx, ry, rz);
				}
			}
			break;

			case AST_INDEX:
			{
				MemorySlot rz = expr_to_any_mem(gen, expr);
				MemorySlot rx = expr_to_any_mem(gen, dest->ast_binary_expr.x);
				MemorySlot ry = expr_to_any_mem(gen, dest->ast_binary_expr.y);
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
				, "invalid l-value, got: %s", tree2s[dest->kind]);
			}
			break;
		}
	}
}

static void generate_ast_stat(BytecodeGen *gen, AstRef stat)
{
	switch (stat->kind)
	{
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

		case AST_ASSIGN_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef dest = stat->ast_assign_stat.x;
			AstRef expr = stat->ast_assign_stat.y;
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

			check_ast_type(name_tuple, AST_TUPLE_EXPR);
			check_ast_type(expr_tuple, AST_TUPLE_EXPR);

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