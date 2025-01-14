/*
** See Copyright Notice In elf.h
** gen.c
*/

// functions used during code generation, these seem sort of
// generic...
static int elf_add_const_int(elf_State *S, elf_Int i) {
	int index = ARRAY_GROW(S->M->integers,1);
	S->M->integers[index] = i;
	return index;
}
static int elf_add_const_num(elf_State *S, elf_Num i) {
	int index = ARRAY_GROW(S->M->numbers,1);
	S->M->numbers[index] = i;
	return index;
}


static int emit_jump(elf_Parser *parser, Source line, int dst);
static int emit_byte(elf_Parser *parser, Source line, Bytecode byte);
static int emit_bytex(elf_Parser *parser, Source line, int k, int x);
static int emit_bytexy(elf_Parser *parser, Source line, int k, int x, int y);
static int emit_bytexyz(elf_Parser *parser, Source line, int k, int x, int y, int z);
static void patch_jump2(elf_Parser *parser, int src, int dst);
static void patch_jumps2(elf_Parser *parser, Instr *js, Instr j);
static void patch_jump(elf_Parser *parser, Instr i);
static void patch_jumps(elf_Parser *parser, Instr *js);


int to_mem(elf_Parser *parser, treeID id, int dst, int ndst);
int to_any_mem(elf_Parser *parser, treeID id);
int tree2o(int kind);
int *emit_jump_if_not_nil(elf_Parser *parser, Source line, jumpS *js, treeID id);
int *emit_jump_if_nil(elf_Parser *parser, Source line, jumpS *js, treeID id);

/* simple memory allocator */
static int memory_usage;
static int memory_state;
static int memory_state_stack[128];
static int memory_state_index;
static treeID memory_slots[128];

static int get_mem_state(elf_Parser *parser) { return memory_state; }
static void set_mem_state(elf_Parser *parser, int state) { memory_state = state; }

static void push_mem_state(elf_Parser *parser) {
	ASSERT(memory_state_index < _countof(memory_state_stack));
	memory_state_stack[memory_state_index ++] = get_mem_state(parser);
}
static void pop_mem_state(elf_Parser *parser) {
	ASSERT(memory_state_index > 0);
	set_mem_state(parser,memory_state_stack[-- memory_state_index]);
}


static int get_mem(elf_Parser *parser, treeID id) {
	int reg = -1;
	if (id != NO_TREE) {
		for (int i = 0; i < memory_state; i ++) {
			if (memory_slots[i] == id) {
				return i;
			}
		}
	}
	return NO_SLOT;
}

/* assign a memory location to the given tree */
static int set_mem(elf_Parser *parser, treeID id) {
	ASSERT(memory_state < _countof(memory_slots));

	int reg = memory_state ++;
	memory_slots[reg] = id;
	if (memory_usage < memory_state) {
		memory_usage = memory_state;
	}

	return reg;
}

int to_any_mem(elf_Parser *parser, treeID id) {
	int reg;
	reg=get_mem(parser,id);
	if (reg==NO_SLOT) {
		reg=to_mem(parser,id,-1,1);
	}
	ASSERT(reg != NO_SLOT);
	return reg;
}

static void gen_tree(elf_Parser *parser, treeID id);

static elf_Proto gen_proto(elf_Parser *parser, treeID tree){
	elf_State *R = parser->R;
	elf_Module *M = parser->R->M;
	ASSERT(get_tree_kind(parser,tree)==TREE_FUNCTION);
	ASSERT(memory_state_index==0);
	ASSERT(memory_state==0);
	ASSERT(memory_usage==0);
	int start=M->nbytes;
	gen_tree(parser,tree->expr_fun.body);
	elf_Proto proto = {};
	proto.arity=1;
	proto.bytes=start;
	proto.nvalues=ARRAY_LENGTH(tree->expr_fun.capts);
	proto.nlocals=memory_usage;
	proto.nbytes=M->nbytes-start;
	// todo: come back to this
	// ASSERT(BC_OP(M->bytes[M->nbytes-1]) == BC_RET);

	ASSERT(memory_state==0);
	ASSERT(memory_state_index==0);
	memory_usage=0;
	return proto;
}

// just have one big, compile function
static elf_File gen_file(elf_Parser *parser, treeID tree){
	int index;
	elf_Proto *protos;
	index=ARRAY_GROW(parser->R->M->protos,ARRAY_LENGTH(parser->functions));
	protos=& parser->R->M->protos[index];

	int start=parser->R->M->nbytes;
	FOR_ARRAY(i,parser->functions){
		parser->functions[i]->expr_fun.proto=index++;
	}
	FOR_ARRAY(i,parser->functions){
		protos[i]=gen_proto(parser,parser->functions[i]);
		// elf_debug_log("PROTO: [%i, %i) (%i)"
		// , 	protos[i].bytes
		// , 	protos[i].bytes+protos[i].nbytes
		// ,	protos[i].nbytes);
	}
	int end=parser->R->M->nbytes;
	elf_File file = {};
	file.pos=start;
	file.end=end;
	file.proto=protos[0];
	// ASSERT(protos[0].bytes==start);
	// protos[0].nbytes=end-start;
	return file;
}

static void gen_tree(elf_Parser *parser, treeID id) {
	treeT tree;

	tree=get_tree(parser,id);
	switch(tree.kind) {
		case TREE_ASSIGN_MEM:{
			int mem;
			mem=to_mem(parser,tree.x,-1,1);
			ASSERT(mem!=-1);
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
			int jmp;
			jmp=emit_jump(parser,tree.line,NO_JUMP);
			id->jump=jmp;
		} break;
		case TREE_WHILE_LOOP: {
			jumpS js = {0};

			treeID pred,body,post,prev;
			pred=tree.loop.pred;
			body=tree.loop.body;
			post=tree.loop.post;
			prev=tree.loop.prev;


			push_mem_state(parser);

			int continue_target;
			int entry;
			entry=parser->R->M->nbytes;
			continue_target=entry;
			emit_jump_if_false(parser,&js,pred);

			if(prev)gen_tree(parser,prev);
			gen_tree(parser,body);
			if(post){
				continue_target=parser->R->M->nbytes;
				gen_tree(parser,post);
			}
			emit_jump(parser,NO_LINE,entry);

			treeID *c,*b;
			b=tree.loop.b;
			c=tree.loop.c;
			FOR_ARRAY(i,c){
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
		case TREE_STORE: {
			treeT xx;
			int dst,mem;
			int rx,ry,rz,op;

			xx=get_tree(parser,tree.x);
			if(xx.type==NT_NON){
				parser_dialog(parser,tree.line
				,	"invalid storage class");
			}
			ASSERT(xx.type!=NT_NON);

			dst=get_mem(parser,tree.x);
			if(dst!=NO_SLOT){
				mem=to_mem(parser,tree.y,dst,1);
				ASSERT(mem==dst);
			}else if(xx.kind==EXPR_FIELD||xx.kind==EXPR_INDEX){
				rz=to_any_mem(parser,tree.y);
				rx=to_any_mem(parser,xx.x);
				ry=to_any_mem(parser,xx.y);
				op=xx.kind==EXPR_INDEX?BC_SETINDEX:BC_SETFIELD;
				emit_bytexyz(parser,tree.line,op,rx,ry,rz);
			}else if(xx.kind==TREE_GLOBAL){
				rx=xx.expr_global;
				rz=to_any_mem(parser,tree.y);
				emit_bytexy(parser,tree.line,BC_SETGLOBAL,rx,rz);
			}else if(xx.kind==TREE_UPVALUE) {
				parser_dialog(parser,tree.line,"assignment of closure value is not possible");
			}else if(xx.kind==EXPR_METAFIELD) {
				parser_dialog(parser,tree.line,"assignment of metafields is not possible");
			} else {
				parser_dialog(parser,tree.line,"internal error: invalid store, operand must be a global or field or index or memory, instead got: %s", tree2s[xx.kind]);
				ASSERT(!"error");
			}
		} break;
		case STAT_BLOCK: {
			push_mem_state(parser);
			FOR_ARRAY(i,tree.z) {
				gen_tree(parser,tree.z[i]);
			}
			pop_mem_state(parser);
		} break;
		// todo: allow true clause to be nil, then just
		// invert the condition...
		case TREE_IF: {
			treeID pred,true_clause,else_clause,then_clause;
			pred=tree.stat_if.pred;
			true_clause=tree.stat_if.true_clause;
			else_clause=tree.stat_if.else_clause;
			then_clause=tree.stat_if.then_clause;

			ASSERT(pred);
			ASSERT(true_clause);
			BranchJumps s={};
			begin_if(parser,tree.line,&s,pred,0);
			gen_tree(parser,true_clause);
			if(else_clause){
				add_else_clause(parser,tree.line,&s);
				gen_tree(parser,else_clause);
			}
			close_if(parser,tree.line,&s);
		} break;
		default: {
			to_mem(parser,id,-1,0);
		} break;
	}
}
static int to_mem(elf_Parser *parser, treeID id, int dst, int ndst) {
	int _dst=dst;

	ASSERT(id!=0);
	ASSERT(id!=NO_TREE);
	elf_State *S;
	treeT tree;
	Source line;
	int mem,rx,ry,rz;

	S=parser->R;
	tree=get_tree(parser,id);
	line=tree.line;

	mem=get_mem(parser,id);
	if(mem!=NO_SLOT){
		if (ndst<1) goto esc;
		if (dst<0) dst=set_mem(parser,id);
		// let a = 0
		// a = a ?? 1
		if(dst!=mem){
			emit_bytexy(parser,line,BC_RELOAD,dst,mem);
		}
		goto esc;
	}
	switch (tree.kind) {
		case TREE_NOP: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
		} break;
		case EXPR_METAFIELD:
		case EXPR_FIELD: case EXPR_INDEX: {
			if (ndst<1) goto esc;
			push_mem_state(parser);
			rx=to_any_mem(parser,tree.x);
			ry=to_any_mem(parser,tree.y);
			pop_mem_state(parser);
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexyz(parser,line,tree2o(tree.kind),dst,rx,ry);
		} break;
		case TREE_UPVALUE: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_GETUPVAL,dst,tree.expr_upvalue);
		} break;
		case TREE_GLOBAL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_GETGLOBAL,dst,tree.expr_global);
		} break;
		case EXPR_NIL: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);

			emit_bytexy(parser,line,BC_LOADNIL,dst,0);
		} break;
		case EXPR_INT: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);

			int yy;
			yy=elf_add_const_int(S,tree.expr_int);
			emit_bytexy(parser,line,BC_GETKINT,dst,yy);
		} break;
		case EXPR_NUM: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);

			int yy;
			yy=elf_add_const_num(S,tree.expr_num);
			emit_bytexy(parser,line,BC_GETKNUM,dst,yy);
		} break;
		case EXPR_STR: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);

			/* todo: interning */
			elf_String *str;
			int yy;

			str=elf_alloc_string(S,tree.expr_str);
			yy=elf_set_global(S->M,0,VSTR(str));
			emit_bytexy(parser,line,BC_GETGLOBAL,dst,yy);
		} break;
		case TREE_NEW_TABLE:{
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_TABLE,dst,0);
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
			jumpS e={0};
			int *j,mem;

			mem=get_mem(parser,tree.x);
			dst=to_mem(parser,tree.x,dst,1);
			j=emit_jump_if_not_nil(parser,NO_LINE,&e,tree.x);
			// ASSERT(mem==NO_SLOT||dst==mem);

			dst=to_mem(parser,tree.y,dst,1);
			patch_jumps(parser,j);
			ARRAY_DELETE(j);
		} break;
		case EXPR_EQ: case EXPR_NEQ:
		case EXPR_GT: case EXPR_GTEQ: case EXPR_LT: case EXPR_LTEQ:
		case EXPR_DIV: case EXPR_MUL: case EXPR_MOD:
		case EXPR_SUB: case EXPR_ADD: case EXPR_POW:
		case EXPR_BIT_SHL: case EXPR_BIT_SHR:
		case EXPR_BIT_XOR:
		case EXPR_BIT_AND: case EXPR_BIT_OR: {
			if ((tree.kind==EXPR_GT)||(tree.kind==EXPR_GTEQ)) {
				push_mem_state(parser);
				rx=to_any_mem(parser,tree.y);
				ry=to_any_mem(parser,tree.x);
				pop_mem_state(parser);
				if (ndst<1) goto esc;
				if (dst<0) dst=set_mem(parser,id);
				emit_bytexyz(parser,tree.line,tree2o(tree.kind^1),dst,rx,ry);
			} else {
				push_mem_state(parser);
				rx=to_any_mem(parser,tree.x);
				ry=to_any_mem(parser,tree.y);
				pop_mem_state(parser);
				if (ndst<1) goto esc;
				if (dst<0) dst=set_mem(parser,id);
				emit_bytexyz(parser,tree.line,tree2o(tree.kind),dst,rx,ry);
			}
		} break;
		case TREE_FUNCTION: {
			int     proto;
			treeID *capts;
			proto=tree.expr_fun.proto;
			capts=tree.expr_fun.capts;

			ASSERT(proto!=-1);

			if (ndst<1) goto esc;
			mem=get_mem_state(parser);
			FOR_ARRAY(i,capts) {
				// we can only capture things with memory,
				// this should evaluate to a reload...
				ASSERT(get_mem(parser,capts[i])!=NO_SLOT);
				int x=to_mem(parser,capts[i],-1,1);
				ASSERT(x==mem+i);
			}
			set_mem_state(parser,mem);
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_CLOSURE,mem,proto);
			if (mem!=dst) {
				emit_bytexy(parser,line,BC_RELOAD,dst,mem);
			}
		} break;
		case TREE_CALL: {
			treeT xx;
			int nargs;

			mem=get_mem_state(parser);
			ASSERT(dst<mem);

			xx=get_tree(parser,tree.x);
			if ((xx.kind==EXPR_FIELD)||(xx.kind==EXPR_METAFIELD)) {
				ry=to_mem(parser,xx.y,-1,1);
				rx=to_mem(parser,xx.x,-1,1);
				emit_bytexyz(parser,line,tree2o(xx.kind),ry,rx,ry);
			} else {
				ry=to_mem(parser,                    tree.x,-1,1);
				// rx=to_mem(parser,tree_this_ref(parser,line),-1,1);
				rx=set_mem(parser,id);
				emit_bytexy(parser,line,BC_RELOAD,rx,0);
			}

			ASSERT(ry==mem+0);
			ASSERT(rx==mem+1);
			FOR_ARRAY(i,tree.z) {
				rz=to_mem(parser,tree.z[i],-1,1);
				ASSERT(rz==mem+2+i);
			}
			set_mem_state(parser,mem);

			nargs=ARRAY_LENGTH(tree.z)+1;
			emit_bytexyz(parser,line,BC_CALL,mem,nargs,ndst);

			if (ndst<1) goto esc;
			if (ndst>1) parser_dialog(parser,line,"multi-returns are not supported yet!");

			if (dst<0) dst=set_mem(parser,id);
			//todo: call instruction that puts
			//the result in a specific dstisters
			if (dst!=mem) {
				emit_bytexy(parser,line,BC_RELOAD,dst,mem);
			}
		} break;
		//todo:fix the bug!
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
	treeID prox;
	for(prox=tree.prox;prox;prox=prox->prox){
		push_mem_state(parser);
		gen_tree(parser,prox);
		pop_mem_state(parser);
	}
	esc:
	ASSERT(_dst==NO_SLOT||_dst==dst);
	return dst;
}

int tree2o(int kind) {
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
		case EXPR_BIT_OR:    return BC_BIT_OR;
		case EXPR_BIT_AND:   return BC_BIT_AND;
		case EXPR_BIT_SHL:   return BC_SHL;
		case EXPR_BIT_SHR:   return BC_SHR;
		case EXPR_BIT_XOR:   return BC_BIT_XOR;
		default: NO_CODE;
	}
	return BC_HALT;
}

static int emit_jump(elf_Parser *parser, Source line, int dst) {
	return emit_bytex(parser,line,BC_J,dst-parser->R->M->nbytes);
}

static int emit_branch_if(elf_Parser *parser, jumpS *js, bool if_true, treeID id) {
	treeT node = get_tree(parser,id);

	int reg,jmp;
	switch (node.kind) {
		case EXPR_AND: {
			emit_jump_if_false(parser,js,node.x);
			jmp=emit_branch_if(parser,js,if_true,node.y);
		} break;
		case EXPR_OR: {
			emit_jump_if_true(parser,js,node.x);
			jmp=emit_branch_if(parser,js,if_true,node.y);
		} break;
		default: {
			push_mem_state(parser);
			reg=to_any_mem(parser,id);
			pop_mem_state(parser);

			if (if_true) {
				jmp=emit_bytexy(parser,node.line,BC_JNZ,NO_JUMP,reg);
				ARRAY_ADD(js->t,jmp);
			} else {
				jmp=emit_bytexy(parser,node.line,BC_JZ,NO_JUMP,reg);
				ARRAY_ADD(js->f,jmp);
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


Instr *emit_jump_if_false(elf_Parser *fs, jumpS *js, treeID id) {
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
	ARRAY_ADD(s->j,j);

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


static int emit_byte(elf_Parser *C, Source line, Bytecode byte) {
	elf_Module *M = C->R->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	// fpf_byte(stdout,M,-1,M->nbytes-C->fn->bytes,byte);
	return M->nbytes ++;
}


static int emit_bytex(elf_Parser *C, Source line, int k, int x) {
	Bytecode byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(C,line,byte);
}


static int emit_bytexy(elf_Parser *C, Source line, int k, int x, int y) {
	Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(C,line,byte);
}


static int emit_bytexyz(elf_Parser *C, Source line, int k, int x, int y, int z) {
	Bytecode byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(C,line,byte);
}

static void patch_jump2(elf_Parser *fs, int src, int dst) {
	Bytecode byte,*bytes;
	bytes=fs->R->M->bytes;
	byte=bytes[src];
	int j = dst - src;
	switch (BC_OP(byte)) {
		// TODO: remove BC_DELAY and BC_YIELD!
		case BC_J: case BC_DELAY: {
			// bytes[src].x = j;
			bytes[src]=BC_XXX(BC_OP(byte),j);
		} break;
		case BC_JZ: case BC_JNZ: case BC_YIELD: {
			// bytes[src].x = j;
			bytes[src]=BC_XYZ(BC_OP(byte),j,BC_ARGY(byte),BC_ARGZ(byte));
		} break;
		default: NO_CODE;
	}
}


static void patch_jumps2(elf_Parser *fs, Instr *js, Instr j) {
	FOR_ARRAY(i,js) {
		patch_jump2(fs,js[i],j);
	}
}


static void patch_jump(elf_Parser *fs, Instr i) {
	patch_jump2(fs,i,fs->R->M->nbytes);
}


static void patch_jumps(elf_Parser *fs, Instr *js) {
	FOR_ARRAY(i,js) {
		patch_jump(fs,js[i]);
	}
}


#if 0
int to_mem(Compiler *C, int flags, int reg, int nreg, treeID id) {
	switch (node.kind) {


		case IR_TYPEGUARD: {
			if (nreg<1) goto esc;
			reg=emit_eval_deprecated(C,flags,reg,nreg,node.x);
			emit_bytexy(C,node.line,BC_TYPEGUARD,reg,node2tag(node.y));
		} break;


FileBlock *get_loop_block(elf_Parser *fs, elf_StackId reg) {
	__debugbreak();
	int level;
	for (level = fs->nblocks-1; level > -1; -- level) {
		FileBlock *bl = get_block(fs,level);
		if (bl->flags & BLOCK_LOOP) {
			if (reg < 0) return bl; else
			if (bl->loop.value_register == reg) return bl;
		}
	}
	return 0;
}

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