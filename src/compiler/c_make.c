//
// See Copyright Notice In elf.h
//

static int emit_branch_if_false(elf_Parser *fs, jumpS *js, treeID id);
static int emit_branch_if_true(elf_Parser *fs, jumpS *js, treeID id);
static int *emit_jump_if_true(elf_Parser *fs, jumpS *js, treeID id);
static int *emit_jump_if_false(elf_Parser *fs, jumpS *js, treeID id);

static void begin_if(elf_Parser *fs, Source line, BranchJumps *s, treeID x, int z);
static void add_elif_clause(elf_Parser *fs, Source line, BranchJumps *s, treeID x);
static void add_else_clause(elf_Parser *fs, Source line, BranchJumps *s);
static void add_then_clause(elf_Parser *fs, Source line, BranchJumps *s);
static void close_if(elf_Parser *fs, Source line, BranchJumps *s);

// static void begin_range_loop(elf_Parser *fs, Source line, treeID x, treeID lo, treeID hi);
// static void close_range_loop(elf_Parser *fs, Source line);
// static void begin_do_while_loop(elf_Parser *fs, Source line);
// static void close_do_while_loop(elf_Parser *fs, Source line, treeID x);
// static void begin_while_loop(elf_Parser *fs, treeID x);
// static void close_while_loop(elf_Parser *fs);

static int emit_jump(elf_Parser *parser, Source line, int dst);
static int emit_byte(elf_Parser *parser, Source line, Bytec byte);
static int emit_bytex(elf_Parser *parser, Source line, int k, int x);
static int emit_bytexy(elf_Parser *parser, Source line, int k, int x, int y);
static int emit_bytexyz(elf_Parser *parser, Source line, int k, int x, int y, int z);
static void patch_jump2(elf_Parser *parser, int src, int dst);
static void patch_jumps2(elf_Parser *parser, BCPos *js, BCPos j);
static void patch_jump(elf_Parser *parser, BCPos i);
static void patch_jumps(elf_Parser *parser, BCPos *js);

static int to_mem(elf_Parser *parser, treeID id, int dst, int ndst);
static int to_any_mem(elf_Parser *parser, treeID id);
static int tree_to_bytec(int kind);
static int *emit_jump_if_not_nil(elf_Parser *parser, Source line, jumpS *js, treeID id);
static int *emit_jump_if_nil(elf_Parser *parser, Source line, jumpS *js, treeID id);

static int get_mem_state(elf_Parser *parser) { return parser->memory_state; }
static void set_mem_state(elf_Parser *parser, int state) { parser->memory_state = state; }


// todo: interning
static int add_const_int(elf_State *S, elf_Integer i) {

	index_t index = darr_grow(S->integers, 1);
	S->integers[index] = i;

	return index;
}

// todo: interning
static int add_const_num(elf_State *S, elf_Number i) {

	index_t index = darr_grow(S->numbers, 1);
	S->numbers[index] = i;

	return index;
}


static void push_mem_state(elf_Parser *parser) {
	ASSERT(parser->memory_state_index < _countof(parser->memory_state_stack));
	parser->memory_state_stack[parser->memory_state_index ++] = get_mem_state(parser);
}
static void pop_mem_state(elf_Parser *parser) {
	ASSERT(parser->memory_state_index > 0);
	set_mem_state(parser,parser->memory_state_stack[-- parser->memory_state_index]);
}


static int get_mem(elf_Parser *parser, treeID id) {
	int mem = -1;
	if (id->kind == TREE_MEMORY) {
#ifdef _DEBUG
		if (id->tree_memory.mem != -1) {
			for (int i = 0; i < parser->memory_state; i ++) {
				if (parser->memory_slots[i] == id) {
					ASSERT(id->kind == TREE_MEMORY);
					ASSERT(id->tree_memory.mem == i);
					return i;
				}
			}
			ASSERT(!"internal error");
		}
#else
		return id->tree_memory.mem;
#endif
	}
	return NO_SLOT;
}



static int req_mem2(elf_Parser *parser, int z) {
	ASSERT(parser->memory_state + z < _countof(parser->memory_slots));

	int mem = parser->memory_state;
	parser->memory_state += z;
	if (parser->memory_usage < parser->memory_state) {
		parser->memory_usage = parser->memory_state;
	}

	return mem;
}


static int req_mem(elf_Parser *parser) {
	return req_mem2(parser, 1);
}



static int to_any_mem(elf_Parser *parser, treeID id) {
	int mem = get_mem(parser, id);
	if (mem == NO_SLOT) {
		mem = to_mem(parser,id,-1,1);
	}
	ASSERT(mem != NO_SLOT);
	return mem;
}



static void make_tree(elf_Parser *parser, treeID id);


static Proto make_proto(elf_Parser *parser, treeID tree) {
	elf_State *R = parser->R;

	ASSERT(tree->kind == TREE_FUNCTION);
	ASSERT(parser->memory_state_index == 0);
	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_usage == 0);

	int bytepos = R->bytecur;
	make_tree(parser, tree->tree_funexpr.body);

	ASSERT(tree->tree_funexpr.arity >= 1);
	Proto proto = {};
	proto.bytes = bytepos;
	proto.arity = tree->tree_funexpr.arity;
	proto.variadic = tree->tree_funexpr.variadic;
	proto.numbytes = R->bytecur - bytepos;
	proto.ncaptures = darr_l(tree->tree_funexpr.capts);
	proto.stacksize = parser->memory_usage;

	// todo: come back to this
	// ASSERT(BC_OP(R->bytes[R->bytecur-1]) == BC_RET);

	ASSERT(parser->memory_state == 0);
	ASSERT(parser->memory_state_index == 0);
	parser->memory_usage = 0;
	return proto;
}



static int make_call(elf_Parser *parser, treeID id, int dst, int nrets) {

	treeID x, *z;
	int mem;
	int rx, ry, rz;

	x=id->x;
	z=id->z;
	mem=get_mem_state(parser);

	ASSERT(dst < mem);

	treeT xx = get_tree(parser, x);

	// todo: parser should generate meta-call tree instead
	// we essentially have to check whether the tree has memory to make
	// sure is a direct meta call
	if (get_mem(parser, x) == NO_SLOT && (xx.kind==EXPR_FIELD)||(xx.kind==EXPR_METAFIELD)) {

		ry = to_mem(parser, xx.y, -1, 1);
		rx = to_mem(parser, xx.x, -1, 1);
		emit_bytexyz(parser, id->line, tree_to_bytec(xx.kind), ry, rx, ry);
	}
	else {
		ry = to_mem(parser, x, -1, 1);
		// implicit 'this'
		rx = req_mem(parser);
		emit_bytexy(parser, id->line, BC_RELOAD, rx, 0);
	}

	// ensure memory is contiguous
	ASSERT(ry == mem + 0);
	ASSERT(rx == mem + 1);

	FOR_ARRAY(i, z) {
		rz = to_mem(parser, z[i], -1, 1);
		// ensure memory is contiguous
		ASSERT(rz == mem + 2 + i);
	}

	// free up memory
	set_mem_state(parser, mem);

	int nargs = darr_l(z) + 1;
	emit_bytexyz(parser, id->line, BC_CALL, mem, nargs, nrets);

	if (nrets < 1) goto esc;
	if (nrets > 1) parser_dialog(parser, id->line, "multi-returns are not supported yet!");

	// todo: allocate memory for the number of expected returns
	if (dst < 0) dst = req_mem(parser);

	if (dst != mem) {
		emit_bytexy(parser, id->line, BC_RELOAD, dst, mem);
	}

	esc:
	return dst;
}



static void make_store(elf_Parser *parser, Source line, treeID x, treeID y, int nrets) {
	int rx,ry,rz,op;

	treeT xx = get_tree(parser, x);
	ASSERT(xx.type != NT_NON);
	ASSERT(xx.kind == TREE_MEMORY || get_mem(parser, x) == -1);

	push_mem_state(parser);

	if (xx.kind == TREE_MEMORY) {
		int dst = get_mem(parser, x);
		int tomem = to_mem(parser, y, dst, 1);
		ASSERT(tomem == dst);
	}
	else if (xx.kind == EXPR_FIELD || xx.kind == EXPR_INDEX) {
		rz = to_any_mem(parser, y);
		rx = to_any_mem(parser, xx.x);
		ry = to_any_mem(parser, xx.y);
		op = xx.kind == EXPR_INDEX ? BC_SETINDEX : BC_SETFIELD;
		emit_bytexyz(parser, line, op, rx, ry, rz);
	}
	else if(xx.kind == TREE_GLOBAL) {
		rx = xx.expr_global;
		rz = to_any_mem(parser, y);
		emit_bytexy(parser, line, BC_SETGLOBAL, rx, rz);
	}
	else if(xx.kind == TREE_UPVALUE) {
		parser_dialog(parser, line, "assignment of closure value is not possible");
	}
	else if(xx.kind == EXPR_METAFIELD) {
		parser_dialog(parser, line, "assignment of metafields is not possible");
	}
	else {

		parser_dialog(parser, line
		, "internal error: invalid l-value, got: %s", tree2s[xx.kind]);

		ASSERT(!"error");
	}

	pop_mem_state(parser);
}



static int make_binary_expr(elf_Parser *parser, treeID id, int dst, int ndst) {
	int rx, ry;
	if ((id->kind==EXPR_GT)||(id->kind==EXPR_GTEQ)) {
		push_mem_state(parser);
		rx=to_any_mem(parser,id->y);
		ry=to_any_mem(parser,id->x);
		pop_mem_state(parser);
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(parser);
		emit_bytexyz(parser,id->line,tree_to_bytec(id->kind^1),dst,rx,ry);
	} else {
		push_mem_state(parser);
		rx=to_any_mem(parser,id->x);
		ry=to_any_mem(parser,id->y);
		pop_mem_state(parser);
		if (ndst<1) goto esc;
		if (dst<0) dst=req_mem(parser);
		emit_bytexyz(parser,id->line,tree_to_bytec(id->kind),dst,rx,ry);
	}
	esc:
	return dst;
}



static void make_tree(elf_Parser *parser, treeID id) {
	treeT tree = get_tree(parser,id);

	switch (tree.kind) {

		case TREE_MEMORY: {
			// tree has already been assigned memory, no good
			ASSERT(get_mem(parser, id) == -1);

			int mem = to_mem(parser, tree.x, -1, 1);
			ASSERT(mem != -1 && mem < parser->memory_state);

			id->tree_memory.mem = mem;
			parser->memory_slots[mem] = id;
		} break;

		case TREE_MULTISTORE: {
			treeID *xs = tree.tree_multistore.xs;
			treeID *ys = tree.tree_multistore.ys;
			int i;
			for (i = 0; i < darr_l(xs); i ++) {
				treeID x = xs[i];
				treeID y;
				if (i < darr_l(ys)) {
					y = ys[i];
				} else {
					y = tree_nil(parser, x->line);
				}
				make_store(parser, x->line, x, y, 1);
			}
		} break;

		case TREE_STORE: {

			make_store(parser, tree.line, tree.x, tree.y, 1);
		} break;

		case STAT_BLOCK: {
			push_mem_state(parser);

			FOR_ARRAY(i, tree.z) {
				make_tree(parser, tree.z[i]);
			}

			pop_mem_state(parser);
		} break;

		case TREE_RET: {
			int mem,num;

			mem=0,num=0;
			if(tree.x!=NO_TREE){
				num=1;
				mem=to_mem(parser,tree.x,NO_SLOT,num);
				ASSERT(mem!=NO_SLOT);
			}

			// if (fs->fn->nyield < nreg) fs->fn->nyield = nreg;

			emit_bytexy(parser,tree.line,BC_RET,mem,num);
		} break;

		case TREE_GOTO: {
			int jmp = emit_jump(parser, tree.line, NO_JUMP);
			id->jump = jmp;
		} break;

		case TREE_WHILE_LOOP: {
			jumpS js = {0};

			treeID pred,body,post,prev;
			pred=tree.loop.pred;
			body=tree.loop.body;
			post=tree.loop.probody;
			prev=tree.loop.prebody;


			push_mem_state(parser);

			int entry = parser->R->bytecur;
			int continue_target = entry;

			emit_jump_if_false(parser, &js, pred);

			if (prev) make_tree(parser, prev);

			make_tree(parser,body);
			if (post) {
				continue_target = parser->R->bytecur;
				make_tree(parser, post);
			}
			emit_jump(parser, NO_LINE, entry);

			treeID *c,*b;
			b=tree.loop.b;
			c=tree.loop.c;
			FOR_ARRAY(i, c) {
				patch_jump2(parser,c[i]->jump,continue_target);
			}

			FOR_ARRAY(i,b){
				patch_jump(parser,b[i]->jump);
			}
			patch_jumps(parser,js.f);
			ARRAY_DELETE(js.f);
			js.f = 0;

			pop_mem_state(parser);
		} break;


		// todo: allow true clause to be nil, then just
		// invert the condition...
		case TREE_IF: {
			treeID pred,true_clause,else_clause,then_clause;
			pred=tree.tree_ifstat.pred;
			true_clause=tree.tree_ifstat.true_clause;
			else_clause=tree.tree_ifstat.else_clause;
			then_clause=tree.tree_ifstat.then_clause;

			ASSERT(pred);
			ASSERT(true_clause);
			// todo: support when no there's no true clause,
			// if there's just else clause, we can just flip
			// the condition...
			BranchJumps s={};
			begin_if(parser,tree.line,&s,pred,0);
			make_tree(parser,true_clause);
			if(else_clause){
				add_else_clause(parser,tree.line,&s);
				make_tree(parser,else_clause);
			}
			close_if(parser,tree.line,&s);
		} break;
		default: {
			to_mem(parser,id,-1,0);
		} break;
	}
}



static int make_newtable_expr(elf_Parser *parser, treeID v, int dst, int ndst) {
	if (ndst<1) goto esc;
	if (dst<0) dst=req_mem(parser);

	emit_bytexy(parser, v->line, BC_TABLE, dst, 0);

	int rx, ry;
	FOR_ARRAY(i, v->expr_newtable.kvs) {
		push_mem_state(parser);

		rx = to_any_mem(parser, v->expr_newtable.kvs[i].x);
		ry = to_any_mem(parser, v->expr_newtable.kvs[i].y);
		emit_bytexyz(parser, v->line, BC_SETFIELD, dst, rx, ry);

		pop_mem_state(parser);
	}

	esc:
	return dst;
}



static int to_mem(elf_Parser *parser, treeID id, int dst, int ndst) {
	int _dst = dst;

	ASSERT(id != 0);
	ASSERT(id != NO_TREE);


	elf_State *S = parser->R;
	treeT tree = get_tree(parser,id);
	Source line = tree.line;

	int rx,ry,rz;
	// int mem = get_mem(parser,id);
	// if(mem != NO_SLOT) {
	// 	if (ndst < 1) goto esc;
	// 	if (dst < 0) dst = req_mem(parser);
	// 	// let a = 0
	// 	// a = a ?? 1
	// 	if (dst != mem) {
	// 		emit_bytexy(parser,line,BC_RELOAD,dst,mem);
	// 	}
	// 	goto esc;
	// }

	if (tree.kind != TREE_MEMORY) {
		ASSERT(get_mem(parser, id) == -1);
	}

	switch (tree.kind) {

		case TREE_PROXY: {
			dst = to_mem(parser, tree.x, dst, ndst);
		} break;

		case TREE_MEMORY: {
			int mem = get_mem(parser, id);
			ASSERT(mem != -1);
			if (ndst < 1) goto esc;
			if (dst < 0) dst = req_mem(parser);

			if (dst != mem) {
				emit_bytexy(parser,line,BC_RELOAD,dst,mem);
			}
		} break;

		case TREE_NOP: {
			if (ndst < 1) goto esc;
			if (dst < 0) dst = req_mem(parser);
		} break;

		// todo: come back to this and explain this
		// case TREE_RELOAD: {
		// 	if (ndst < 1) goto esc;
		// 	int mem = get_mem(parser,tree.x);
		// 	// if already has memory create new one, unless we were
		// 	// given memory
		// 	if (mem != -1 && dst < 0) {
		// 		dst=req_mem(parser);
		// 	}
		// 	dst=to_mem(parser,tree.x,dst,ndst);
		// } break;

		case TREE_GETMEM: {
			int mem = get_mem(parser,tree.x);
			if (mem == NO_SLOT) {
				parser_dialog(parser,get_tree_line(parser,tree.x),"no memory assigned to this thing");
			}
			dst = to_mem(parser,tree_int(parser,tree.line,mem),dst,ndst);
		} break;

		case TREE_GETEXPR: {
			char *expr = tree2s[get_tree_kind(parser,tree.x)];
			dst=to_mem(parser,tree_str(parser,tree.line,expr),dst,ndst);
		} break;

		case EXPR_METAFIELD:
		case EXPR_FIELD: case EXPR_INDEX: {
			if (ndst<1) goto esc;
			push_mem_state(parser);
			rx=to_any_mem(parser,tree.x);
			ry=to_any_mem(parser,tree.y);
			pop_mem_state(parser);
			if (dst<0) dst=req_mem(parser);
			emit_bytexyz(parser, line, tree_to_bytec(tree.kind), dst, rx, ry);
		} break;

		case TREE_UPVALUE: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BC_GETUPVAL,dst,tree.expr_upvalue);
		} break;


		case TREE_GLOBAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);
			emit_bytexy(parser,line,BC_GETGLOBAL,dst,tree.expr_global);
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
			emit_bytexy(parser,line,BC_GETKINT,dst,yy);
		} break;
		case EXPR_NUM: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			int yy=add_const_num(S,tree.expr_num);
			emit_bytexy(parser,line,BC_GETKNUM,dst,yy);
		} break;
		case EXPR_STR: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			// todo: add to constant pool and create new
			// GETKSTR instruction
			elf_String *str = elf_alloc_string(S, tree.expr_str);

			int yy = elf_get_global_slot(S, 0);
			vsetstr(&S->globals->array[yy], str);

			emit_bytexy(parser,line,BC_GETGLOBAL,dst,yy);
		} break;

		case TREE_NEW_TABLE:{
			dst = make_newtable_expr(parser, id, dst, ndst);
		} break;

		/* (a !! b) = (a == nil ? a : b) */
		case EXPR_NIL_AND: {
			jumpS e = {0};
			int *j,mem;

			mem=get_mem(parser,tree.x);
			dst=to_mem(parser,tree.x,dst,1);
			j=emit_jump_if_nil(parser,NO_LINE,&e,tree.x);
			// ASSERT(mem==NO_SLOT||dst==mem);

			dst=to_mem(parser,tree.y,dst,1);
			patch_jumps(parser,j);
			ARRAY_DELETE(j);
		} break;
		case EXPR_NIL_OR: {

			dst = to_mem(parser,tree.x,dst,1);

			jumpS e = {0};
			int *j = emit_jump_if_not_nil(parser,NO_LINE,&e,tree.x);

			dst = to_mem(parser,tree.y,dst,1);
			patch_jumps(parser,j);
			ARRAY_DELETE(j);
		} break;

		case TREE_FUNCTION: {
			int     proto = tree.tree_funexpr.proto;
			treeID *capts = tree.tree_funexpr.capts;
			ASSERT(proto != -1);
			if (ndst<1) goto esc;

			int mem=get_mem_state(parser);
			FOR_ARRAY(i,capts) {
				// we can only capture things with memory,
				// this should evaluate to a reload...
				ASSERT(get_mem(parser, capts[i]) != NO_SLOT);
				int x = to_mem(parser, capts[i], -1, 1);
				ASSERT(x == mem + i);
			}
			set_mem_state(parser,mem);

			if (dst<0) dst = req_mem(parser);
			emit_bytexy(parser,line,BC_CLOSURE,mem,proto);

			if (mem!=dst) {
				emit_bytexy(parser,line,BC_RELOAD,dst,mem);
			}

		} break;

		case TREE_CALL: {
			dst = make_call(parser, id, dst, ndst);
			if (dst == -1) {
				goto esc;
			}
		} break;

		case EXPR_BIT_NOT: {
			if (ndst<1) goto esc;
			if (dst<0) dst=req_mem(parser);

			push_mem_state(parser);
			rx = to_any_mem(parser, tree.x);
			pop_mem_state(parser);

			emit_bytexy(parser,tree.line,tree_to_bytec(tree.kind),dst,rx);
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

		// todo:fix the bug!
		case EXPR_AND: case EXPR_OR: {
			jumpS e = {0};
			int *js;
			dst=to_mem(parser,tree_int(parser,line,0),dst,1);
			js=emit_jump_if_false(parser,&e,id);
			dst=to_mem(parser,tree_int(parser,line,1),dst,1);
			patch_jumps(parser,js);
			ARRAY_DELETE(js);
		} break;
		default: {
			parser_dialog(parser,line,"invalid tree (%s)",tree2s[tree.kind]);
			NO_CODE;
		}
	}
	ASSERT(dst!=NO_SLOT);
	//	treeID prox;
	//	for(prox=tree.prox;prox;prox=prox->prox){
	//		push_mem_state(parser);
	//		make_tree(parser,prox);
	//		pop_mem_state(parser);
	//	}
	esc:
	ASSERT(_dst==NO_SLOT||_dst==dst);
	return dst;
}

// todo: replace with table!
static int tree_to_bytec(int kind) {
	switch (kind) {
		case EXPR_FIELD: 	   return BC_GETFIELD;     // *
		case EXPR_INDEX: 	   return BC_GETINDEX;     // *
		case EXPR_METAFIELD: return BC_GETMETAFIELD; // *
		case TREE_CALL:      return BC_CALL;         // *
		case EXPR_ADD:       return BC_ADD;
		case EXPR_SUB:       return BC_SUB;
		case EXPR_DIV:       return BC_DIV;
		case EXPR_MUL:       return BC_MUL;
		case EXPR_POW:       return BC_POW;
		case EXPR_MOD:       return BC_MOD;
		case EXPR_NEQ:       return BC_NEQ;
		case EXPR_EQ:        return BC_EQ;
		case EXPR_LT:        return BC_LT;
		case EXPR_LTEQ:      return BC_LTEQ;
		case EXPR_BIT_NOT:   return BC_BIT_NOT;
		case EXPR_BIT_OR:    return BC_BIT_OR;
		case EXPR_BIT_AND:   return BC_BIT_AND;
		case EXPR_BIT_SHL:   return BC_BIT_SHL;
		case EXPR_BIT_SHR:   return BC_BIT_SHR;
		case EXPR_BIT_XOR:   return BC_BIT_XOR;
		default: NO_CODE;
	}
	return BC_HALT;
}

static int emit_jump(elf_Parser *parser, Source line, int dst) {
	return emit_bytex(parser,line,BC_J,dst-parser->R->bytecur);
}


static int emit_branch_if(elf_Parser *parser, jumpS *js, bool if_true, treeID id) {

	// int mem=get_mem(parser,id);
	// if(mem!=NO_SLOT) goto _got_mem;

	int jmp;

	treeT tree = get_tree(parser,id);

	switch (tree.kind) {
		case EXPR_AND: {
			ASSERT(get_mem(parser, id) == -1);
			emit_jump_if_false(parser,js,tree.x);
			jmp=emit_branch_if(parser,js,if_true,tree.y);
		} break;
		case EXPR_OR: {
			ASSERT(get_mem(parser, id) == -1);
			emit_jump_if_true(parser,js,tree.x);
			jmp=emit_branch_if(parser,js,if_true,tree.y);
		} break;
		default: {
			push_mem_state(parser);
			int mem=to_any_mem(parser,id);
			pop_mem_state(parser);
			if (if_true) {
				jmp=emit_bytexy(parser,tree.line,BC_JNZ,NO_JUMP,mem);
				darr_add(js->t,jmp);
			} else {
				jmp=emit_bytexy(parser,tree.line,BC_JZ,NO_JUMP,mem);
				darr_add(js->f,jmp);
			}
		} break;
	}

	return jmp;
}


int emit_branch_if_false(elf_Parser *fs, jumpS *js, treeID id) {
	return emit_branch_if(fs,js,0,id);
}


int emit_branch_if_true(elf_Parser *fs, jumpS *js, treeID id) {
	return emit_branch_if(fs,js,1,id);
}


/* similar to branch if true, but additionally all
false jumps converge here */
int *emit_jump_if_true(elf_Parser *fs, jumpS *js, treeID id) {
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


BCPos *emit_jump_if_false(elf_Parser *fs, jumpS *js, treeID id) {
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}


int *emit_jump_if_not_nil(elf_Parser *parser, Source line, jumpS *js, treeID id) {
	return emit_jump_if_false(parser,js,tree_binary(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}


int *emit_jump_if_nil(elf_Parser *parser, Source line, jumpS *js, treeID id) {
	return emit_jump_if_true(parser,js,tree_binary(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}

static void begin_if(elf_Parser *parser, Source line, BranchJumps *s, treeID x, int if_true) {
	jumpS js = {0};
	emit_branch_if(parser,&js,if_true,x);
	if (if_true) {
		ASSERT(js.t != 0);
		patch_jumps(parser,js.f);
		ARRAY_DELETE(js.f);
		js.f = 0;
		s->jz = js.t;
	} else {
		ASSERT(js.f != 0);
		patch_jumps(parser,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	}
}


/* closes previous conditional block by emitting
escape jump, patches previous jz (jump if false)
list to enter this block. */
void add_else_clause(elf_Parser *fs, Source line, BranchJumps *s) {
	if (s->jz == 0) {
		parser_dialog(fs,line,"invalid else clause");
	}
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,line,-1);
	darr_add(s->j,j);

	patch_jumps(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void add_elif_clause(elf_Parser *parser, Source line, BranchJumps *s, treeID x) {
	add_else_clause(parser,line,s);
	begin_if(parser,line,s,x,0);
}


void add_then_clause(elf_Parser *parser, Source line, BranchJumps *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. (can't believe I used the word natuarally twice) */
	patch_jumps(parser,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void close_if(elf_Parser *fs, Source line, BranchJumps *s) {
	/* collect missing else branch */
	if (s->jz != 0) {
		patch_jumps(fs,s->jz);
		ARRAY_DELETE(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		patch_jumps(fs,s->j);
		ARRAY_DELETE(s->j);
		s->j = 0;
	}
}


static int emit_byte(elf_Parser *parser, Source line, Bytec byte) {
	elf_State *S = parser->R;
	// todo: please remove this :)
	line = line ? line : parser->sourceloc;
	ASSERT(line != 0);
	parser->sourceloc = line;
	darr_add(S->lines, line);
	darr_add(S->track, 0);
	darr_add(S->bytebuf, byte);

	// printf("%s(%i, %i, %i)\n", byte2s[byte.b_k], byte.b_x, byte.b_y, byte.b_z);
	return S->bytecur ++;
}


static int emit_bytex(elf_Parser *C, Source line, int k, int x) {
	Bytec byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(C,line,byte);
}


static int emit_bytexy(elf_Parser *C, Source line, int k, int x, int y) {
	Bytec byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(C,line,byte);
}


static int emit_bytexyz(elf_Parser *C, Source line, int k, int x, int y, int z) {
	Bytec byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(C,line,byte);
}


static void patch_jump2(elf_Parser *parser, BCPos src, BCPos dst) {

	Bytec byte, *bytebuf;

	bytebuf = parser->R->bytebuf;
	byte = bytebuf[src];

	int j = dst - src;
	switch (BC_OP(byte)) {

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


static void patch_jumps2(elf_Parser *fs, BCPos *js, BCPos j) {
	FOR_ARRAY(i,js) {
		patch_jump2(fs,js[i],j);
	}
}


static void patch_jump(elf_Parser *fs, BCPos i) {
	patch_jump2(fs,i,fs->R->bytecur);
}


static void patch_jumps(elf_Parser *parser, BCPos *s) {
	FOR_ARRAY(i,s) {
		patch_jump(parser,s[i]);
	}
}


#if 0
void desugar_range_expr_epilogue(elf_Parser *fs, treeID x) {
	treeT node = get_tree(fs,x);
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
treeID desugar_range_expr(elf_Parser *fs, treeID x, int flags) {
	treeT node;
	Source line;
	treeID xx;
	treeID array,index,value;

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

			treeID lo,hi;
			lo=get_tree(fs,node.y).x;
			hi=get_tree(fs,node.y).y;
			if (lo==NO_TREE) lo=tree_int(fs,line,0);
			if (hi==NO_TREE) hi=tree_meta_call(fs,line,array,0,"length");

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
	return NO_TREE;
}

void opt_const_fold(elf_Parser *fs, treeT node) {
	#define isconst(k) (k==IR_STRING||k==IR_INTEGER||k==IR_NUMBER)
	treeT xx,yy;
	int xconst;
	int yconst;
	xx=get_tree(fs,node.x);
	yy=get_tree(fs,node.y);
	xconst = isconst(xx.kind) || ((xx.k==IR_LOCAL) && (find_local_const_store(fs,xx.x) != -1));
	yconst = isconst(yy.kind) || ((yy.k==IR_LOCAL) && (find_local_const_store(fs,yy.x) != -1));

	if (xconst && yconst) {
		parser_dialog(fs,node.line,"possible constant fold");
	}
}
		case IR_CLOSURE: {
			if (nreg<1) goto esc;
			/* todo: why is it that we can't just start at reg */
			mem=get_mem_state_deprecated(fs); {
				FOR_ARRAY(i,node.z) {
					emit_eval_deprecated(fs,0,reg_alloc_deprecated(fs),1,node.z[i]);
				}
			} set_mem_state_deprecated(fs,mem);
			if (reg<0) reg=reg_alloc_deprecated(fs);
			emit_bytexy_deprecated(fs,line,BC_CLOSURE,mem,node.x);
			if (mem!=reg) {
				emit_bytexy_deprecated(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		case IR_TYPEGUARD: {
			if (nreg<1) goto esc;
			reg=emit_eval_deprecated(fs,flags,reg,nreg,node.x);
			emit_bytexy_deprecated(fs,node.line,BC_TYPEGUARD,reg,node2tag(node.y));
		} break;

/*
-- I think this hack will become obsolete


Emits auxilary code for a particular compound expression.

More specifically, it loads the dependencies of said
expression into a register, and then returns a new node
which points to the register where the dependencies
where evaluated.

For instance, the expression:
a.b.c.d ?= 1 ::= (if a.b.c.d nil ? a.b.c.d = 1)

Which uses emits a bunch of loads and stores, gets
translated to:

tmp=a.b.c
if tmp.d nil ? tmp.d=1

*/
treeID emit_preload_deprecated(elf_Parser *fs, treeID x) {
	ASSERT(x>=0);

	treeT   node;
	int     reg;

	node=get_target_node(fs,TREEID(x));

	switch (node.kind) {
		case IR_FIELD: {
			reg=any_reg_deprecated(fs,node.x);
			x=tree_field(fs,node.line,tree_local(fs,node.line,reg),node.y);
		} break;
		case IR_INDEX: {
			reg=any_reg_deprecated(fs,node.x);
			x=tree_index(fs,node.line,tree_local(fs,node.line,reg),node.y);
		} break;
		default:;
	}

	return x;
}
#endif