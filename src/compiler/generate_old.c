//
// See Copyright Notice In elf.h
//

#if 0

#define NO_MEMORY (-1)
#define NO_JUMP (-0)


static void k_do_tree(Parser *parser, AstRef id);


static int to_mem(Parser *parser, AstRef id, int dst, int ndst);
static int expr_to_any_mem(Parser *parser, AstRef id);
static int instr_from_tree_kind(int kind);

#define MEMORY_STATE_SCOPE(par) for(int _i = (push_mem_state_(par), 0); _i ++ < 1; pop_mem_state_(par))

// todo: interning
static int add_const_int(elf_State *S, elf_Integer i)
{
	Index index = heap_array_grow(S->integers, 1);
	S->integers[index] = i;
	return index;
}

// todo: interning
static int add_const_num(elf_State *S, elf_Number i)
{
	Index index = heap_array_grow(S->numbers, 1);
	S->numbers[index] = i;
	return index;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MEMORY_STATE_SCOPE(par) for(int _i = (push_mem_state_(par), 0); _i ++ < 1; pop_mem_state_(par))

static int get_mem_state(Parser *par)
{
	return par->memory_state;
}

// use MEMORY_STATE_SCOPE
static void push_mem_state_(Parser *par)
{
	ASSERT(par->memory_state_index < _countof(par->memory_state_stack));
	par->memory_state_stack[par->memory_state_index ++] = get_mem_state(par);
}

// use MEMORY_STATE_SCOPE
static void pop_mem_state_(Parser *par)
{
	ASSERT(par->memory_state_index > 0);

	i32 prev_mem_state = par->memory_state;

	par->memory_state = par->memory_state_stack[-- par->memory_state_index];

	ASSERT(prev_mem_state >= par->memory_state);

	i32 i;
	for (i = prev_mem_state - 1; i >= par->memory_state; -- i)
	{
		if (par->memory_slots[i] != Y_NULL)
		{
			par->memory_slots[i]->tree_memory.mem = NO_MEMORY;
			par->memory_slots[i] = Y_NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static i32 get_expr_mem(Parser *par, AstRef id)
{
	i32 mem = -1;

	ASSERT(id);

	if (id->kind == TREE_PROXY)
	{
		id = id->x;
		ASSERT(id->kind == TREE_MEMORY);
	}

	if (id->kind == TREE_MEMORY)
	{
		mem = id->tree_memory.mem;
		// Check invalid memory references
#if defined(_DEBUG)
		if (mem != -1)
		{
			u32 o, i;
			for (i = 0, o = 0; i < par->memory_state; ++ i)
			{
				if (par->memory_slots[i] == id)
				{
					ASSERT(mem == i);
					++ o;
				}
			}
			ASSERT(o == 1);
		}
#endif
	}
	return mem;
}

static int req_mem2(Parser *par, int z)
{
	ASSERT(par->memory_state + z < _countof(par->memory_slots));

	int mem = par->memory_state;

	par->memory_state += z;

	if (par->memory_usage < par->memory_state)
	{
		par->memory_usage = par->memory_state;
	}

	return mem;
}

static int req_mem(Parser *par)
{
	return req_mem2(par, 1);
}

static int expr_to_any_mem(Parser *par, AstRef id)
{
	ASSERT(id);
	int mem = get_expr_mem(par, id);
	if (mem == NO_MEMORY)
	{
		mem = to_mem(par,id,-1,1);
	}
	ASSERT(mem != NO_MEMORY);
	return mem;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BytecodeFunction k_do_proto(Parser *parser, AstRef tree)
{
	elf_State *S = parser->R;
	ASSERT(tree->kind == AST_FUNCTION);
	ASSERT(parser->memory_state_index == 0);
	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_usage == 0);

	int bytepos = S->bytecur;
	k_do_tree(parser, tree->ast_function.body);

	ASSERT(tree->ast_function.arity >= 1);

	BytecodeFunction proto =
	{
		.bytes     = bytepos,
		.arity     = tree->ast_function.arity,
		.variadic  = tree->ast_function.variadic,
		.numbytes  = S->bytecur - bytepos,
		.ncaptures = heap_array_length(tree->ast_function.capts),
		.stacksize = parser->memory_usage,
	};

	// todo: come back to this
	// ASSERT(BYTECODE_OP(S->bytes[S->bytecur-1]) == BYTECODE_RET);

	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_state_index == 0);
	parser->memory_usage = 0;
	return proto;
}

static void k_do_store(Parser *parser, Source cur, AstRef x, AstRef y)
{
	int rx,ry,rz,op;

	if (x->kind == TREE_PROXY)
	{
		x = x->x;
	}

	Ast tx = *x;

	MEMORY_STATE_SCOPE(parser)
	{

		switch (tx.kind)
		{
			case TREE_MEMORY:
			{
				int rx = get_expr_mem(parser, x);
				ASSERT(rx != -1);
				ASSERT(x->tree_memory.rem >= 1);

				int ry = to_mem(parser, y, rx, x->tree_memory.rem);
				ASSERT(ry == rx);
			}
			break;
			case AST_FIELD:
			case AST_INDEX:
			{
				rz = expr_to_any_mem(parser, y);
				rx = expr_to_any_mem(parser, tx.x);
				ry = expr_to_any_mem(parser, tx.y);
				op = tx.kind == AST_INDEX ? BYTECODE_SETINDEX : BYTECODE_SETFIELD;
				emit_bytexyz(parser, cur, op, rx, ry, rz);
			}
			break;
			case TREE_GLOBAL:
			{
				rx = tx.expr_global;
				rz = expr_to_any_mem(parser, y);
				emit_bytexy(parser, cur, BYTECODE_SETGLOBAL, rx, rz);
			}
			break;
			case TREE_UPVALUE:
			{
				push_error(parser, ERROR_INVALID_STORE_CLOSURE_VALUE, cur, "assignment of closure value is not possible");
			}
			break;
			case AST_META_FIELD:
			{
				push_error(parser, ERROR_INVALID_STORE_METAFIELD, cur, "assignment of metafields is not possible");
			}
			break;
			default:
			{

				parser_dialog(parser, cur
				, "internal error: invalid l-value, got: %s", tree2s[tx.kind]);

				ASSERT(!"error");
			}
			break;
		}
	}
}

static int generate_call_expr(Parser *parser, AstRef tr, int dst, int nrets)
{
	ASSERT(tr->x);
	ASSERT(tr->n >= 1);

	AstRef x = tr->x;
	AstRef z = tr->x->next;
	int nargs = 1;

	int mem = get_mem_state(parser);
	MEMORY_STATE_SCOPE(parser)
	{
		i32 rx, ry;
		if (x->kind == AST_FIELD || x->kind == AST_META_FIELD)
		{
			ry = to_mem(parser, x->y, -1, 1);
			rx = to_mem(parser, x->x, -1, 1);
			emit_bytexyz(parser, tr->site, instr_from_tree_kind(x->kind), ry, rx, ry);
		}
		else
		{
			ry = to_mem(parser, x, -1, 1);
			// implicit 'this'
			rx = req_mem(parser);
			emit_bytexy(parser, tr->site, BYTECODE_RELOAD, rx, 0);
		}

		// ensure memory is contiguous
		ASSERT(ry == mem + 0);
		ASSERT(rx == mem + 1);

		u32 i = 0;
		for (AstRef zi = z; zi; zi = zi->next, ++ i)
		{
			u32 rz = to_mem(parser, zi, -1, 1);
			ASSERT(rz == mem + 2 + i);
			++ nargs;
		}
	}

	emit_bytexyz(parser, tr->site, BYTECODE_CALL, mem, nargs, nrets);

	if (nrets < 1) goto esc;

	// Todo, allocate memory for the number of expected returns
	if (dst < 0)
	{
		dst = req_mem(parser);
	}

	if (dst != mem)
	{
		if (nrets > 1)
		{
			parser_dialog(parser, tr->site, "multi-returns are not fully supported yet!");
			reporterror(parser->R, -1, "multi-returns are not fully supported");
		}

		emit_bytexy(parser, tr->site, BYTECODE_RELOAD, dst, mem);
	}

	esc:
	return dst;
}

static int make_binary_expr(Parser *par, AstRef expr, int dst, int ndst)
{
	int rx, ry;
	// inverse
	if ((expr->kind==AST_GREATER_THAN_EQ)||(expr->kind==AST_GREATER_THAN_EQ))
	{
		MEMORY_STATE_SCOPE(par)
		{
			rx=expr_to_any_mem(par,expr->y);
			ry=expr_to_any_mem(par,expr->x);
		}
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(par);
		emit_bytexyz(par,expr->site,instr_from_tree_kind(expr->kind^1),dst,rx,ry);
	}
	else
	{
		MEMORY_STATE_SCOPE(par)
		{
			rx=expr_to_any_mem(par,expr->x);
			ry=expr_to_any_mem(par,expr->y);
		}
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(par);
		emit_bytexyz(par,expr->site,instr_from_tree_kind(expr->kind),dst,rx,ry);
	}
	esc:
	return dst;
}

// make a top-level target
static void k_do_tree(Parser *parser, AstRef id)
{
	Ast tree = * id;

	switch (tree.kind)
	{
		case TREE_MEMORY:
		{
			// target has already been assigned memory, no good
			ASSERT(get_expr_mem(parser, id) == -1);

			int mem = to_mem(parser, tree.x, -1, tree.tree_memory.rem);
			ASSERT(mem != -1 && mem < parser->memory_state);

			id->tree_memory.mem = mem;

			// @debugonly
			parser->memory_slots[mem] = id;
		}
		break;

		// Todo, replace with begin / end memory region instead?
		case AST_BLOCK_STAT:
		{
			MEMORY_STATE_SCOPE(parser)
			{
				for (AstRef i = tree.x; i; i = i->next)
				{
					k_do_tree(parser, i);
				}
			}
		}
		break;

		case TREE_STORE:
		{
			k_do_store(parser, tree.site, tree.x, tree.y);
		}
		break;

		case AST_RETURN:
		{
			u32 nrets = tree.n;
			u32 mem = get_mem_state(parser);
			u32 i = mem;
			AST_FOR(c, tree.x)
			{
				i32 r = to_mem(parser, c, NO_MEMORY, 1);
				ASSERT(r == i ++);
			}

			emit_bytexy(parser, tree.site, BYTECODE_RET, mem, nrets);
		}
		break;

		case TREE_GOTO:
		{
			int jmp = emit_jump(parser, tree.site, NO_JUMP);
			id->jump = jmp;
		}
		break;

#if 0
		case TREE_WHILE_LOOP:
		{
			// todo: user should do this?
			MEMORY_STATE_SCOPE(parser)
			{
				AstRef prepred=tree.loop.prepred;
				AstRef pred=tree.loop.pred;
				AstRef prebody=tree.loop.prebody;
				AstRef body=tree.loop.body;
				AstRef probody=tree.loop.probody;

				int entry = parser->inter->bytecur;
				int continue_target = entry;

				if (prepred) k_do_tree(parser, prepred);

				jumpS js = {0};
				emit_jump_if_false(parser, &js, pred);

				if (prebody) k_do_tree(parser, prebody);

				k_do_tree(parser,body);

				if (probody) {
					continue_target = parser->inter->bytecur;
					k_do_tree(parser, probody);
				}

				emit_jump(parser, body->site, entry);

				AstRef *c=tree.loop.c;
				FOR_ARRAY(i, c)
				{
					ASSERT(c[i]->kind == TREE_GOTO);
					patch_jump2(parser, c[i]->jump, continue_target);
				}

				AstRef *b=tree.loop.b;
				FOR_ARRAY(i,b)
				{
					// ASSERT(b[i]->kind == TREE_EXIT_LOOP);
					patch_jump(parser, b[i]->jump);
				}
				patch_jumps(parser,js.f);
				free_heap_array(js.f);
				js.f = 0;
			}
		}
		break;
#endif

		// todo: attempt to enforce statically!
		case TREE_ENFORCE:
		{
			int rx = get_expr_mem(parser, tree.x);
			ASSERT(rx != -1);

			emit_bytexy(parser, tree.site, BYTECODE_ENFORCE, rx, tree.rule);
		}
		break;
		// todo: allow true clause to be nil, then just
		// invert the condition...
		case AST_IF: {
			AstRef pred,true_clause,else_clause,then_clause;
			pred=tree.tree_if_stat.pred;
			true_clause=tree.tree_if_stat.true_clause;
			else_clause=tree.tree_if_stat.else_clause;
			// then_clause=tree.tree_if_stat.then_clause;

			ASSERT(pred);
			ASSERT(true_clause);
			// todo: support when no there's no true clause,
			// if there's just else clause, we can just flip
			// the condition...
			JBuf s={};
			begin_if(parser,tree.site,&s,pred,0);

			MEMORY_STATE_SCOPE(parser) {
				k_do_tree(parser,true_clause);
			}
			if(else_clause){
				MEMORY_STATE_SCOPE(parser) {
					add_else_clause(parser,tree.site,&s);
				}
				k_do_tree(parser,else_clause);
			}
			close_if(parser,tree.site,&s);
		} break;
		default: {
			to_mem(parser,id,-1,0);
		} break;
	}
}

static int k_do_new_table(Parser *par, AstRef v, int dst, int ndst)
{
	if (ndst<1) goto esc;
	if (dst<0) dst=req_mem(par);

	emit_bytexy(par, v->site, BYTECODE_TABLE, dst, 0);

	int rx, ry;
	AST_FOR(kv, v->x)
	{
		MEMORY_STATE_SCOPE(par)
		{
			ASSERT(kv->x);
			ASSERT(kv->y);
			// Todo, have a dedicated tree kind for emptyness, or missing ? ...
			if (kv->x->kind != AST_NIL_LITERAL)
			{
				rx = expr_to_any_mem(par, kv->x);
				ry = expr_to_any_mem(par, kv->y);
				emit_bytexyz(par, v->site, BYTECODE_SETFIELD, dst, rx, ry);
			}
			else
			{
				rx = expr_to_any_mem(par, kv->y);
				emit_bytexy(par, v->site, BYTECODE_ARRAYADD, dst, rx);
			}
		}
	}

	esc:
	return dst;
}

static int to_mem(Parser *parser, AstRef id, int dst, int ndst) {
	ASSERT(id != 0);
	ASSERT(id != Y_NULL);
	ASSERT(id->site);

	elf_State *S = parser->R;
	Ast tree = * id;
	Source line = tree.site;

	int rx, ry, rz;

	switch (tree.kind)
	{
		//	case TREE_NOP:
		//	{
		//		if (ndst < 1) goto esc;
		//		if (dst < 0) dst = req_mem(parser);
		//	}
		//	break;
		case TREE_PROXY:
		{
			dst = to_mem(parser, tree.x, dst, ndst);
		}
		break;
		case TREE_MEMORY:
		{
			if (ndst < 1) goto esc;
			if (dst < 0) dst = req_mem(parser);

			int mem = get_expr_mem(parser, id);

			if (mem == -1)
			{
				parser_dialog(parser, id->site, "invalid memory state");
			}

			ASSERT(mem != -1);

			if (dst != mem)
			{
				emit_bytexy(parser, line, BYTECODE_RELOAD, dst, mem);
			}
		}
		break;

		// Todo, might as well just have one that returns the entire meta-data in the form of a table ...
		case TREE_DEBUG_GET_MEMORY:
		{
			int mem = get_expr_mem(parser,tree.x);
			if (mem == NO_MEMORY)
			{
				parser_dialog(parser, tree.x->site, "no memory assigned to this thing");
			}
			dst = to_mem(parser,tree_int(parser,tree.site,mem),dst,ndst);
		}
		break;

		case TREE_DEBUG_GET_EXPRESSION_NAME:
		{
			char *expr = tree2s[tree.x->kind];
			dst = to_mem(parser, create_str_ast(parser, tree.site, expr), dst, ndst);
		}
		break;

		case AST_INDEX:
		case AST_FIELD:
		case AST_META_FIELD:
		{
			if (ndst < 1)
			{
				goto esc;
			}
			MEMORY_STATE_SCOPE(parser)
			{
				rx = expr_to_any_mem(parser,tree.x);
				ry = expr_to_any_mem(parser,tree.y);
			}
			if (dst < 0) dst = req_mem(parser);
			emit_bytexyz(parser, line, instr_from_tree_kind(tree.kind), dst, rx, ry);
		}
		break;

		case TREE_UPVALUE: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BYTECODE_LOADCVAL,dst,tree.expr_cvalue);
		} break;


		case TREE_GLOBAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BYTECODE_LOADGLOBAL,dst,tree.expr_global);
		} break;


		case AST_NIL_LITERAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			emit_bytexy(parser,line,BYTECODE_LOADNIL,dst,0);
		} break;


		case AST_INTEGER_LITERAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			int yy=add_const_int(S,tree.expr_int);
			emit_bytexy(parser,line,BYTECODE_LOADKINT,dst,yy);
		} break;


		case AST_NUMBER_LITERAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			int yy=add_const_num(S,tree.expr_num);
			emit_bytexy(parser,line,BYTECODE_LOADKNUM,dst,yy);
		} break;


		case AST_STRING_LITERAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			// todo: add to constant pool and create new GETKSTR instruction
			// todo: also add dedicated get str field instruction, which is
			// the common case for field accesses!
			Str str = _string_new(S, tree.expr_str);

			int yy = heap_array_grow(S->globals->array, 1);
			to_str(&S->globals->array[yy], str);

			emit_bytexy(parser,line,BYTECODE_LOADGLOBAL,dst,yy);
		} break;

		case AST_TABLE:{
			dst = k_do_new_table(parser, id, dst, ndst);
		} break;

		case AST_AND: case AST_OR: {
			// todo: can we remove jumpS?
			jumpS e = {0};
			int *js;
			dst=to_mem(parser,tree_int(parser,line,0),dst,1);
			js=emit_jump_if_false(parser,&e,id);
			dst=to_mem(parser,tree_int(parser,line,1),dst,1);
			patch_jumps(parser,js);
			free_heap_array(js);
		} break;

		/* (a !! b) = (a == nil ? a : b) */
		case AST_NIL_AND: {

			dst=to_mem(parser,tree.x,dst,1);

			jumpS e = {0};
			int *j=emit_jump_if_nil(parser,tree.site,&e,tree.x);

			dst=to_mem(parser,tree.y,dst,1);

			patch_jumps(parser,j);
			free_heap_array(j);
		} break;

		/* (a ?? b) = (a != nil ? a : b) */
		case AST_NIL_OR: {

			dst=to_mem(parser,tree.x,dst,1);

			jumpS e = {0};
			int *j = emit_jump_if_not_nil(parser,tree.site,&e,tree.x);

			dst=to_mem(parser,tree.y,dst,1);

			patch_jumps(parser,j);
			free_heap_array(j);
		} break;

		case AST_FUNCTION: {

			int     proto = tree.ast_function.proto;
			AstRef *capts = tree.ast_function.capts;

			ASSERT(proto != -1);

			if (ndst < 1) goto esc;

			int mem = get_mem_state(parser);

			MEMORY_STATE_SCOPE(parser) {
				FOR_ARRAY(i, capts) {
					//
					// we can only capture things with memory,
					// this should evaluate to a reload...
					//

					// todo: we can't print where the reference actually happened because
					// we don't have a tree for it!
					int reg = get_expr_mem(parser, capts[i]);
					if (reg == NO_MEMORY) {
						push_error(parser, EC_INTERNAL_CANNOT_CAPTURE_NO_MEMORY, capts[i]->site, "can only capture things with memory");
					}

					int x = to_mem(parser, capts[i], -1, 1);
					ASSERT(x == mem + i);
				}
			}

			if (dst < 0) dst = req_mem(parser);
			emit_bytexy(parser, line, BYTECODE_CLOSURE, mem, proto);

			if (mem != dst) {
				emit_bytexy(parser, line, BYTECODE_RELOAD, dst, mem);
			}
		} break;

		case AST_CALL:
		{
			dst = generate_call_expr(parser, id, dst, ndst);
		}
		break;

		// unary trees
		case AST_LENGTH_INTRINSIC:
		case AST_BITWISE_NOT: {

			if (ndst<1) goto esc;

			MEMORY_STATE_SCOPE(parser) {
				rx = expr_to_any_mem(parser, tree.x);
			}

			if (dst<0) dst=req_mem(parser);

			emit_bytexy(parser,tree.site,instr_from_tree_kind(tree.kind),dst,rx);
		} break;


		case AST_EQ: case AST_NOT_EQ:
		case AST_GREATER_THAN_EQ: case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN: case AST_LESS_THAN:
		case AST_DIV: case AST_MUL: case AST_MOD:
		case AST_SUB: case AST_ADD: case AST_POW:
		case AST_SHIFT_LEFT: case AST_SHIFT_RIGHT:
		case AST_BITWISE_XOR:
		case AST_BITWISE_AND: case AST_BITWISE_OR: {
			dst = make_binary_expr(parser, id, dst, ndst);
		} break;

		default: {
			push_error(parser, INTERNAL_ERROR_INVALID_TREE
			,	line,	"invalid tree (%s)",	tree2s[tree.kind]);
			NO_CODE;
		}
	}
	esc:
	return dst;
}

static int instr_from_tree_kind(AstType kind)
{
	switch (kind)
	{
		case AST_FIELD: 	return BYTECODE_GETFIELD;
		case AST_INDEX:        return BYTECODE_GETINDEX;
		case AST_META_FIELD:    return BYTECODE_GETMETAFIELD;
		case AST_LENGTH_INTRINSIC:       return BYTECODE_GETLENGTH;
		case AST_CALL:         return BYTECODE_CALL;
		case AST_ADD:          return BYTECODE_ADD;
		case AST_SUB:          return BYTECODE_SUB;
		case AST_DIV:          return BYTECODE_DIV;
		case AST_MUL:          return BYTECODE_MUL;
		case AST_POW:          return BYTECODE_POW;
		case AST_MOD:          return BYTECODE_MOD;
		case AST_NOT_EQ:          return BYTECODE_NEQ;
		case AST_EQ:           return BYTECODE_EQ;
		case AST_LESS_THAN:           return BYTECODE_LT;
		case AST_LESS_THAN_EQ:         return BYTECODE_LTEQ;
		case AST_BITWISE_NOT:      return BYTECODE_BIT_NOT;
		case AST_BITWISE_OR:       return BYTECODE_BIT_OR;
		case AST_BITWISE_AND:      return BYTECODE_BIT_AND;
		case AST_SHIFT_LEFT:      return BYTECODE_BIT_SHL;
		case AST_SHIFT_RIGHT:      return BYTECODE_BIT_SHR;
		case AST_BITWISE_XOR:      return BYTECODE_BIT_XOR;
		default: NO_CODE;
	}
	return BYTECODE_HALT;
}



#if 0
void desugar_range_expr_epilogue(Parser *fs, AstRef x) {
	Ast node = get_tree(fs,x);
	switch (node.kind) {
		case IR_INDEX: case IR_FIELD: {
			desugar_range_expr_epilogue(fs,node.x);
		} break;
		case IR_RANGE_INDEX: {
			close_range_loop(fs,NO_LINE);
			parser_close_block(fs);
			desugar_range_expr_epilogue(fs,node.x);
		} break;
		default: ;
	}
}
AstRef desugar_range_expr(Parser *fs, AstRef x, int flags) {
	Ast node;
	Source line;
	AstRef xx;
	AstRef array,index,value;

	node=get_target_node(fs,TREEID(x));
	switch (node.kind) {
		case IR_INDEX: case IR_FIELD: {
			xx=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);
			return elf_new_binary_expr_tree(fs,node.site,node.kind,xx,node.y);
		}
		case IR_RANGE_INDEX: {
			ASSERT(get_tree_kind(fs,node.y) == IR_RANGE);

			line=node.site;
			array=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);

			int array_reg,index_reg,value_reg;
			int block;

			block = parser_begin_block(fs,BLOCK_LOOP);

			array_reg=any_reg_deprecated(fs,array);
			index_reg=reg_alloc_deprecated(fs);

			array = tree_local(fs,line,array_reg);
			index = tree_local(fs,line,index_reg);
			value = tree_index(fs,line,array,index);

			AstRef lo,hi;
			lo=get_tree(fs,node.y).x;
			hi=get_tree(fs,node.y).y;
			if (lo==Y_NULL) lo=tree_int(fs,line,0);
			if (hi==Y_NULL) hi=ELF_NewMetaCallTree(fs,line,array,0,"length");

			begin_range_loop(fs,line,index,lo,hi);
			value_reg=any_reg_deprecated(fs,value);

			/* todo: all these should be nodes instead */
			get_block(fs,block)->loop.array_register=array_reg;
			get_block(fs,block)->loop.value_register=value_reg;
			return value;
		}

		default: ;
	}
	return x;
	// Todo: come back to this later
	__debugbreak();
	return Y_NULL;
}
#endif

#endif