//
// See Copyright Notice In elf.h
//


#define NO_SLOT (-1)
#define NO_JUMP (-0)


static void k_do_tree(Parser *parser, TreeId id);

static int emit_branch_if_false(Parser *fs, jumpS *js, TreeId id);
static int emit_branch_if_true(Parser *fs, jumpS *js, TreeId id);
static int *emit_jump_if_true(Parser *fs, jumpS *js, TreeId id);
static int *emit_jump_if_false(Parser *fs, jumpS *js, TreeId id);

static void begin_if(Parser *fs, Source line, JBuf *s, TreeId x, int z);
static void add_elif_clause(Parser *fs, Source line, JBuf *s, TreeId x);
static void add_else_clause(Parser *fs, Source line, JBuf *s);
static void add_then_clause(Parser *fs, Source line, JBuf *s);
static void close_if(Parser *fs, Source line, JBuf *s);

static int emit_jump(Parser *parser, Source line, int dst);
static int emit_byte(Parser *parser, Source line, Bytec byte);
static int emit_bytex(Parser *parser, Source line, int k, int x);
static int emit_bytexy(Parser *parser, Source line, int k, int x, int y);
static int emit_bytexyz(Parser *parser, Source line, int k, int x, int y, int z);
static void patch_jump2(Parser *parser, int src, int dst);
static void patch_jumps2(Parser *parser, BCPos *js, BCPos j);
static void patch_jump(Parser *parser, BCPos i);
static void patch_jumps(Parser *parser, BCPos *js);

static int to_mem(Parser *parser, TreeId id, int dst, int ndst);
static int to_any_mem(Parser *parser, TreeId id);
static int instr_from_tree_kind(int kind);
static int *emit_jump_if_not_nil(Parser *parser, Source line, jumpS *js, TreeId id);
static int *emit_jump_if_nil(Parser *parser, Source line, jumpS *js, TreeId id);

#define SCOPE_MEM_STATE(par) for(int _i = (push_mem_state_(par), 0); _i ++ < 1; pop_mem_state_(par))

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

static int get_mem_state(Parser *par)
{
	return par->memory_state;
}

// use SCOPE_MEM_STATE
static void push_mem_state_(Parser *par)
{
	ASSERT(par->memory_state_index < _countof(par->memory_state_stack));
	par->memory_state_stack[par->memory_state_index ++] = get_mem_state(par);
}

// use SCOPE_MEM_STATE
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
			par->memory_slots[i]->tree_memory.mem = NO_SLOT;
			par->memory_slots[i] = Y_NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static i32 get_mem(Parser *par, TreeId id)
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

static int to_any_mem(Parser *par, TreeId id)
{
	ASSERT(id);
	int mem = get_mem(par, id);
	if (mem == NO_SLOT)
	{
		mem = to_mem(par,id,-1,1);
	}
	ASSERT(mem != NO_SLOT);
	return mem;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Proto k_do_proto(Parser *parser, TreeId tree)
{
	elf_State *S = parser->R;
	ASSERT(tree->kind == TREE_FUNCTION);
	ASSERT(parser->memory_state_index == 0);
	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_usage == 0);

	int bytepos = S->bytecur;
	k_do_tree(parser, tree->tree_funexpr.body);

	ASSERT(tree->tree_funexpr.arity >= 1);

	Proto proto =
	{
		.bytes     = bytepos,
		.arity     = tree->tree_funexpr.arity,
		.variadic  = tree->tree_funexpr.variadic,
		.numbytes  = S->bytecur - bytepos,
		.ncaptures = heap_array_length(tree->tree_funexpr.capts),
		.stacksize = parser->memory_usage,
	};

	// todo: come back to this
	// ASSERT(BC_OP(S->bytes[S->bytecur-1]) == BC_RET);

	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_state_index == 0);
	parser->memory_usage = 0;
	return proto;
}

static void k_do_store(Parser *parser, Source cur, TreeId x, TreeId y)
{
	int rx,ry,rz,op;

	if (x->kind == TREE_PROXY)
	{
		x = x->x;
	}

	Tree tx = *x;
	ASSERT(tx.type != NT_NON);

	SCOPE_MEM_STATE(parser)
	{

		switch (tx.kind)
		{
			case TREE_MEMORY:
			{
				int rx = get_mem(parser, x);
				ASSERT(rx != -1);
				ASSERT(x->tree_memory.rem >= 1);

				int ry = to_mem(parser, y, rx, x->tree_memory.rem);
				ASSERT(ry == rx);
			}
			break;
			case TREE_TABLE_FIELD:
			case TREE_INDEX:
			{
				rz = to_any_mem(parser, y);
				rx = to_any_mem(parser, tx.x);
				ry = to_any_mem(parser, tx.y);
				op = tx.kind == TREE_INDEX ? BC_SETINDEX : BC_SETFIELD;
				emit_bytexyz(parser, cur, op, rx, ry, rz);
			}
			break;
			case TREE_GLOBAL:
			{
				rx = tx.expr_global;
				rz = to_any_mem(parser, y);
				emit_bytexy(parser, cur, BC_SETGLOBAL, rx, rz);
			}
			break;
			case TREE_UPVALUE:
			{
				push_error(parser, ERROR_INVALID_STORE_CLOSURE_VALUE, cur, "assignment of closure value is not possible");
			}
			break;
			case EXPR_METAFIELD:
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

static int k_do_call(Parser *parser, TreeId tr, int dst, int nrets)
{
	ASSERT(tr->x);
	ASSERT(tr->n >= 1);

	TreeId x = tr->x;
	TreeId z = tr->x->next;
	int nargs = 1;

	int mem = get_mem_state(parser);
	SCOPE_MEM_STATE(parser)
	{
		i32 rx, ry;
		if (x->kind == TREE_TABLE_FIELD || x->kind == EXPR_METAFIELD)
		{
			ry = to_mem(parser, x->y, -1, 1);
			rx = to_mem(parser, x->x, -1, 1);
			emit_bytexyz(parser, tr->line, instr_from_tree_kind(x->kind), ry, rx, ry);
		}
		else
		{
			ry = to_mem(parser, x, -1, 1);
			// implicit 'this'
			rx = req_mem(parser);
			emit_bytexy(parser, tr->line, BC_RELOAD, rx, 0);
		}

		// ensure memory is contiguous
		ASSERT(ry == mem + 0);
		ASSERT(rx == mem + 1);

		u32 i = 0;
		for (TreeId zi = z; zi; zi = zi->next, ++ i)
		{
			u32 rz = to_mem(parser, zi, -1, 1);
			ASSERT(rz == mem + 2 + i);
			++ nargs;
		}
	}

	emit_bytexyz(parser, tr->line, BC_CALL, mem, nargs, nrets);

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
			parser_dialog(parser, tr->line, "multi-returns are not fully supported yet!");
			reporterror(parser->R, -1, "multi-returns are not fully supported");
		}

		emit_bytexy(parser, tr->line, BC_RELOAD, dst, mem);
	}

	esc:
	return dst;
}

static int make_binary_expr(Parser *par, TreeId expr, int dst, int ndst)
{
	int rx, ry;
	// inverse
	if ((expr->kind==EXPR_GT)||(expr->kind==EXPR_GTEQ))
	{
		SCOPE_MEM_STATE(par)
		{
			rx=to_any_mem(par,expr->y);
			ry=to_any_mem(par,expr->x);
		}
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(par);
		emit_bytexyz(par,expr->line,instr_from_tree_kind(expr->kind^1),dst,rx,ry);
	}
	else
	{
		SCOPE_MEM_STATE(par)
		{
			rx=to_any_mem(par,expr->x);
			ry=to_any_mem(par,expr->y);
		}
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(par);
		emit_bytexyz(par,expr->line,instr_from_tree_kind(expr->kind),dst,rx,ry);
	}
	esc:
	return dst;
}

// make a top-level target
static void k_do_tree(Parser *parser, TreeId id)
{
	Tree tree = get_tree(parser,id);

	switch (tree.kind)
	{
		case TREE_MEMORY:
		{
			// target has already been assigned memory, no good
			ASSERT(get_mem(parser, id) == -1);

			int mem = to_mem(parser, tree.x, -1, tree.tree_memory.rem);
			ASSERT(mem != -1 && mem < parser->memory_state);

			id->tree_memory.mem = mem;

			// @debugonly
			parser->memory_slots[mem] = id;
		}
		break;

		// Todo, replace with begin / end memory region instead?
		case TREE_BLOCK:
		{
			SCOPE_MEM_STATE(parser)
			{
				for (TreeId i = tree.x; i; i = i->next)
				{
					k_do_tree(parser, i);
				}
			}
		}
		break;

		case TREE_STORE:
		{
			k_do_store(parser, tree.line, tree.x, tree.y);
		}
		break;

		case TREE_RET:
		{
			u32 nrets = tree.n;
			u32 mem = get_mem_state(parser);
			u32 i = mem;
			TREE_FOR(c, tree.x)
			{
				i32 r = to_mem(parser, c, NO_SLOT, 1);
				ASSERT(r == i ++);
			}

			emit_bytexy(parser, tree.line, BC_RET, mem, nrets);
		}
		break;

		case TREE_GOTO:
		{
			int jmp = emit_jump(parser, tree.line, NO_JUMP);
			id->jump = jmp;
		}
		break;

		case TREE_WHILE_LOOP: {
			// todo: user should do this?
			SCOPE_MEM_STATE(parser)
			{
				TreeId prepred=tree.loop.prepred;
				TreeId pred=tree.loop.pred;
				TreeId prebody=tree.loop.prebody;
				TreeId body=tree.loop.body;
				TreeId probody=tree.loop.probody;

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

				emit_jump(parser, body->line, entry);

				TreeId *c=tree.loop.c;
				FOR_ARRAY(i, c)
				{
					ASSERT(c[i]->kind == TREE_GOTO);
					patch_jump2(parser, c[i]->jump, continue_target);
				}

				TreeId *b=tree.loop.b;
				FOR_ARRAY(i,b)
				{
					// ASSERT(b[i]->kind == TREE_EXIT_LOOP);
					patch_jump(parser, b[i]->jump);
				}
				patch_jumps(parser,js.f);
				free_heap_array(js.f);
				js.f = 0;
			}
		} break;
		// todo: attempt to enforce statically!
		case TREE_ENFORCE:
		{
			int rx = get_mem(parser, tree.x);
			ASSERT(rx != -1);

			emit_bytexy(parser, tree.line, BC_ENFORCE, rx, tree.rule);
		}
		break;
		// todo: allow true clause to be nil, then just
		// invert the condition...
		case TREE_IF: {
			TreeId pred,true_clause,else_clause,then_clause;
			pred=tree.tree_ifstat.pred;
			true_clause=tree.tree_ifstat.true_clause;
			else_clause=tree.tree_ifstat.else_clause;
			// then_clause=tree.tree_ifstat.then_clause;

			ASSERT(pred);
			ASSERT(true_clause);
			// todo: support when no there's no true clause,
			// if there's just else clause, we can just flip
			// the condition...
			JBuf s={};
			begin_if(parser,tree.line,&s,pred,0);

			SCOPE_MEM_STATE(parser) {
				k_do_tree(parser,true_clause);
			}
			if(else_clause){
				SCOPE_MEM_STATE(parser) {
					add_else_clause(parser,tree.line,&s);
				}
				k_do_tree(parser,else_clause);
			}
			close_if(parser,tree.line,&s);
		} break;
		default: {
			to_mem(parser,id,-1,0);
		} break;
	}
}

static int k_do_new_table(Parser *par, TreeId v, int dst, int ndst)
{
	if (ndst<1) goto esc;
	if (dst<0) dst=req_mem(par);

	emit_bytexy(par, v->cursor, BC_TABLE, dst, 0);

	int rx, ry;
	TREE_FOR(kv, v->x)
	{
		SCOPE_MEM_STATE(par)
		{
			ASSERT(kv->x);
			ASSERT(kv->y);
			// Todo, have a dedicated tree kind for emptyness, or missing ? ...
			if (kv->x->kind != EXPR_NIL)
			{
				rx = to_any_mem(par, kv->x);
				ry = to_any_mem(par, kv->y);
				emit_bytexyz(par, v->cursor, BC_SETFIELD, dst, rx, ry);
			}
			else
			{
				rx = to_any_mem(par, kv->y);
				emit_bytexy(par, v->cursor, BC_ARRAYADD, dst, rx);
			}
		}
	}

	esc:
	return dst;
}

static int to_mem(Parser *parser, TreeId id, int dst, int ndst) {
	ASSERT(id != 0);
	ASSERT(id != Y_NULL);
	ASSERT(id->line);

	elf_State *S = parser->R;
	Tree tree = * id;
	Source line = tree.line;

	int rx, ry, rz;

	switch (tree.kind)
	{
		case TREE_NOP:
		{
			if (ndst < 1) goto esc;
			if (dst < 0) dst = req_mem(parser);
		}
		break;
		case TREE_PROXY:
		{
			dst = to_mem(parser, tree.x, dst, ndst);
		}
		break;
		case TREE_MEMORY:
		{
			if (ndst < 1) goto esc;
			if (dst < 0) dst = req_mem(parser);

			int mem = get_mem(parser, id);

			if (mem == -1)
			{
				parser_dialog(parser, id->line, "invalid memory state");
			}

			ASSERT(mem != -1);

			if (dst != mem)
			{
				emit_bytexy(parser, line, BC_RELOAD, dst, mem);
			}
		}
		break;

		// Todo, might as well just have one that returns the entire meta-data in the form of a table ...
		case TREE_DEBUG_GET_MEMORY:
		{
			int mem = get_mem(parser,tree.x);
			if (mem == NO_SLOT)
			{
				parser_dialog(parser, tree_get_line(parser,tree.x), "no memory assigned to this thing");
			}
			dst = to_mem(parser,tree_int(parser,tree.line,mem),dst,ndst);
		}
		break;

		case TREE_DEBUG_GET_EXPRESSION_NAME:
		{
			char *expr = tree2s[get_tree_kind(parser, tree.x)];
			dst = to_mem(parser, tree_str(parser, tree.line, expr), dst, ndst);
		}
		break;

		case TREE_INDEX:
		case TREE_TABLE_FIELD:
		case EXPR_METAFIELD:
		{
			if (ndst < 1)
			{
				goto esc;
			}
			SCOPE_MEM_STATE(parser)
			{
				rx = to_any_mem(parser,tree.x);
				ry = to_any_mem(parser,tree.y);
			}
			if (dst < 0) dst = req_mem(parser);
			emit_bytexyz(parser, line, instr_from_tree_kind(tree.kind), dst, rx, ry);
		}
		break;

		case TREE_UPVALUE: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BC_LOADCVAL,dst,tree.expr_cvalue);
		} break;


		case TREE_GLOBAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BC_LOADGLOBAL,dst,tree.expr_global);
		} break;


		case EXPR_NIL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			emit_bytexy(parser,line,BC_LOADNIL,dst,0);
		} break;


		case EXPR_INT: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			int yy=add_const_int(S,tree.expr_int);
			emit_bytexy(parser,line,BC_LOADKINT,dst,yy);
		} break;


		case EXPR_NUM: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			int yy=add_const_num(S,tree.expr_num);
			emit_bytexy(parser,line,BC_LOADKNUM,dst,yy);
		} break;


		case EXPR_STR: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			// todo: add to constant pool and create new GETKSTR instruction
			// todo: also add dedicated get str field instruction, which is
			// the common case for field accesses!
			Str str = _string_new(S, tree.expr_str);

			int yy = heap_array_grow(S->globals->array, 1);
			to_str(&S->globals->array[yy], str);

			emit_bytexy(parser,line,BC_LOADGLOBAL,dst,yy);
		} break;

		case TREE_NEW_TABLE:{
			dst = k_do_new_table(parser, id, dst, ndst);
		} break;

		case EXPR_AND: case EXPR_OR: {
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
		case EXPR_NIL_AND: {

			dst=to_mem(parser,tree.x,dst,1);

			jumpS e = {0};
			int *j=emit_jump_if_nil(parser,tree.line,&e,tree.x);

			dst=to_mem(parser,tree.y,dst,1);

			patch_jumps(parser,j);
			free_heap_array(j);
		} break;

		/* (a ?? b) = (a != nil ? a : b) */
		case EXPR_NIL_OR: {

			dst=to_mem(parser,tree.x,dst,1);

			jumpS e = {0};
			int *j = emit_jump_if_not_nil(parser,tree.line,&e,tree.x);

			dst=to_mem(parser,tree.y,dst,1);

			patch_jumps(parser,j);
			free_heap_array(j);
		} break;

		case TREE_FUNCTION: {

			int     proto = tree.tree_funexpr.proto;
			TreeId *capts = tree.tree_funexpr.capts;

			ASSERT(proto != -1);

			if (ndst < 1) goto esc;

			int mem = get_mem_state(parser);

			SCOPE_MEM_STATE(parser) {
				FOR_ARRAY(i, capts) {
					//
					// we can only capture things with memory,
					// this should evaluate to a reload...
					//

					// todo: we can't print where the reference actually happened because
					// we don't have a tree for it!
					int reg = get_mem(parser, capts[i]);
					if (reg == NO_SLOT) {
						push_error(parser, EC_INTERNAL_CANNOT_CAPTURE_NO_MEMORY, capts[i]->line, "can only capture things with memory");
					}

					int x = to_mem(parser, capts[i], -1, 1);
					ASSERT(x == mem + i);
				}
			}

			if (dst < 0) dst = req_mem(parser);
			emit_bytexy(parser, line, BC_CLOSURE, mem, proto);

			if (mem != dst) {
				emit_bytexy(parser, line, BC_RELOAD, dst, mem);
			}
		} break;

		case TREE_CALL:
		{
			dst = k_do_call(parser, id, dst, ndst);
		}
		break;

		// unary trees
		case TREE_LENGTH:
		case EXPR_BIT_NOT: {

			if (ndst<1) goto esc;

			SCOPE_MEM_STATE(parser) {
				rx = to_any_mem(parser, tree.x);
			}

			if (dst<0) dst=req_mem(parser);

			emit_bytexy(parser,tree.line,instr_from_tree_kind(tree.kind),dst,rx);
		} break;


		case EXPR_EQ: case EXPR_NEQ:
		case EXPR_GT: case EXPR_GTEQ:
		case EXPR_LT: case EXPR_LTEQ:
		case EXPR_DIV: case EXPR_MUL: case EXPR_MOD:
		case EXPR_SUB: case EXPR_ADD: case EXPR_POW:
		case EXPR_BIT_SHL: case EXPR_BIT_SHR:
		case EXPR_BIT_XOR:
		case EXPR_BIT_AND: case EXPR_BIT_OR: {
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

static int instr_from_tree_kind(TreeKind kind)
{
	switch (kind)
	{
		case TREE_TABLE_FIELD: 	return BC_GETFIELD;
		case TREE_INDEX:        return BC_GETINDEX;
		case EXPR_METAFIELD:    return BC_GETMETAFIELD;
		case TREE_LENGTH:       return BC_GETLENGTH;
		case TREE_CALL:         return BC_CALL;
		case EXPR_ADD:          return BC_ADD;
		case EXPR_SUB:          return BC_SUB;
		case EXPR_DIV:          return BC_DIV;
		case EXPR_MUL:          return BC_MUL;
		case EXPR_POW:          return BC_POW;
		case EXPR_MOD:          return BC_MOD;
		case EXPR_NEQ:          return BC_NEQ;
		case EXPR_EQ:           return BC_EQ;
		case EXPR_LT:           return BC_LT;
		case EXPR_LTEQ:         return BC_LTEQ;
		case EXPR_BIT_NOT:      return BC_BIT_NOT;
		case EXPR_BIT_OR:       return BC_BIT_OR;
		case EXPR_BIT_AND:      return BC_BIT_AND;
		case EXPR_BIT_SHL:      return BC_BIT_SHL;
		case EXPR_BIT_SHR:      return BC_BIT_SHR;
		case EXPR_BIT_XOR:      return BC_BIT_XOR;
		default: NO_CODE;
	}
	return BC_HALT;
}

static BCPos emit_jump(Parser *parser, Source line, BCPos dst)
{
	BCPos rel = dst - parser->R->bytecur;
	return emit_bytex(parser, line, BC_J, rel);
}


#if 0
static BCPos emit_loop_jump(Parser *parser, Source line, BCPos dst) {
	BCPos rel = dst - parser->R->bytecur;
	ASSERT(rel < 0);
	return emit_bytex(parser, line, BC_LOOP_JUMP, rel);
}

static BCPos emit_exit_loop_jump(Parser *parser, Source line) {
	return emit_bytex(parser, line, BC_EXIT_LOOP_JUMP, 0);
}
#endif



static int emit_branch_if(Parser *parser, jumpS *js, bool if_true, TreeId expr) {
	int jmp;

	switch (expr->kind) {
		case EXPR_AND: {
			emit_jump_if_false(parser,js,expr->x);
			jmp=emit_branch_if(parser,js,if_true,expr->y);
		} break;
		case EXPR_OR: {
			emit_jump_if_true(parser,js,expr->x);
			jmp=emit_branch_if(parser,js,if_true,expr->y);
		} break;
		default: {
			int mem;
			SCOPE_MEM_STATE(parser) {
				mem=to_any_mem(parser,expr);
			}
			if (if_true) {
				jmp=emit_bytexy(parser,expr->line,BC_JNZ,NO_JUMP,mem);
				heap_array_add(js->t,jmp);
			} else {
				jmp=emit_bytexy(parser,expr->line,BC_JZ,NO_JUMP,mem);
				heap_array_add(js->f,jmp);
			}
		} break;
	}
	return jmp;
}



static inline int emit_branch_if_false(Parser *parser, jumpS *js, TreeId id) {
	return emit_branch_if(parser,js,0,id);
}



static inline int emit_branch_if_true(Parser *parser, jumpS *js, TreeId id) {
	return emit_branch_if(parser,js,1,id);
}



/* similar to branch if true, but additionally all
false jumps converge here */
static inline BCPos *emit_jump_if_true(Parser *fs, jumpS *js, TreeId id) {
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	free_heap_array(js->f);
	js->f = 0;
	return js->t;
}


static inline BCPos *emit_jump_if_false(Parser *fs, jumpS *js, TreeId id) {
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	free_heap_array(js->t);
	js->t = 0;
	return js->f;
}


// todo: dedicated instructions?
static inline int *emit_jump_if_not_nil(Parser *parser, Source line, jumpS *js, TreeId id) {
	return emit_jump_if_false(parser,js,tree_binary(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}



// todo: dedicated instructions?
static inline int *emit_jump_if_nil(Parser *parser, Source line, jumpS *js, TreeId id) {
	return emit_jump_if_true(parser,js,tree_binary(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}



static void begin_if(Parser *parser, Source line, JBuf *jb, TreeId x, int if_true) {
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
void add_else_clause(Parser *fs, Source line, JBuf *s) {
	if (s->jz == 0) {
		parser_dialog(fs,line,"invalid else clause");
	}
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,line,-1);
	heap_array_add(s->j,j);

	patch_jumps(fs,s->jz);
	free_heap_array(s->jz);
	s->jz = 0;
}



static void add_elif_clause(Parser *parser, Source line, JBuf *s, TreeId x) {
	add_else_clause(parser,line,s);
	begin_if(parser,line,s,x,0);
}



static void add_then_clause(Parser *parser, Source line, JBuf *s) {
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

void close_if(Parser *fs, Source line, JBuf *s)
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

static int emit_byte(Parser *parser, Source line, Bytec byte)
{
	assert(line);
	elf_State *S = parser->R;
	heap_array_add(S->lines, line);
	heap_array_add(S->bytebuf, byte);
	return S->bytecur ++;
}

static int emit_bytex(Parser *C, Source line, int k, int x)
{
	Bytec byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(C,line,byte);
}

static int emit_bytexy(Parser *C, Source line, int k, int x, int y)
{
	Bytec byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(C,line,byte);
}

static int emit_bytexyz(Parser *C, Source line, int k, int x, int y, int z)
{
	Bytec byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(C,line,byte);
}

static void patch_jump2(Parser *parser, BCPos src, BCPos dst)
{
	Bytec byte, *bytebuf;

	bytebuf = parser->R->bytebuf;
	byte = bytebuf[src];

	int j = dst - src;
	switch (BC_OP(byte)) {
#if 0
		case BC_EXIT_LOOP_JUMP:
#endif
		case BC_J: {
			bytebuf[src].b_x = j;
			bytebuf[src]=BC_XXX(BC_OP(byte),j);
		} break;

		case BC_JZ: case BC_JNZ: {
			// bytebuf[src].x = j;
			bytebuf[src]=BC_XYZ(BC_OP(byte),j,BC_ARGY(byte),BC_ARGZ(byte));
		} break;

		default: NO_CODE;
	}
}

static void patch_jumps2(Parser *par, BCPos *js, BCPos dst)
{
	FOR_ARRAY(i, js) {
		patch_jump2(par, js[i], dst);
	}
}

static void patch_jump(Parser *par, BCPos j)
{
	patch_jump2(par, j, par->R->bytecur);
}

static void patch_jumps(Parser *par, BCPos *js) {
	FOR_ARRAY(i, js) {
		patch_jump(par, js[i]);
	}
}


#if 0
void desugar_range_expr_epilogue(Parser *fs, TreeId x) {
	Tree node = get_tree(fs,x);
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
TreeId desugar_range_expr(Parser *fs, TreeId x, int flags) {
	Tree node;
	Source line;
	TreeId xx;
	TreeId array,index,value;

	node=get_target_node(fs,TREEID(x));
	switch (node.kind) {
		case IR_INDEX: case IR_FIELD: {
			xx=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);
			return tree_binary(fs,node.line,node.kind,NT_ANY,xx,node.y);
		}
		case IR_RANGE_INDEX: {
			ASSERT(get_tree_kind(fs,node.y) == IR_RANGE);

			line=node.line;
			array=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);

			int array_reg,index_reg,value_reg;
			int block;

			block = parser_begin_block(fs,BLOCK_LOOP);

			array_reg=any_reg_deprecated(fs,array);
			index_reg=reg_alloc_deprecated(fs);

			array = tree_local(fs,line,array_reg);
			index = tree_local(fs,line,index_reg);
			value = tree_index(fs,line,array,index);

			TreeId lo,hi;
			lo=get_tree(fs,node.y).x;
			hi=get_tree(fs,node.y).y;
			if (lo==Y_NULL) lo=tree_int(fs,line,0);
			if (hi==Y_NULL) hi=tree_meta_call(fs,line,array,0,"length");

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