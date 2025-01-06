/*
** See Copyright Notice In elf.h
** compile.c
*/

#include "emitter.c"




static int memory_usage;
static int memory_state;
static int memory_state_stack[16];
static int memory_state_index;
static treeID memory_slots[128];

static int emit_jump(Parser *parser, Source line, int dst);

int to_mem(Parser *parser, treeID id, int dst, int ndst);
int to_any_mem(Parser *parser, treeID id);
int tree2o(int kind);

/* this is just a simple linear 'memory' allocator */
static int get_mem_state(Parser *parser) { return memory_state; }
static void set_mem_state(Parser *parser, int state) { memory_state = state; }

static void push_mem_state(Parser *parser) {
	memory_state_stack[memory_state_index ++] = get_mem_state(parser);
}
static void pop_mem_state(Parser *parser) {
	set_mem_state(parser,memory_state_stack[-- memory_state_index]);
}

/* get the memory associated with the tree */
static int get_mem(Parser *parser, treeID id) {
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
static int set_mem(Parser *parser, treeID id) {
	ASSERT(memory_state < _countof(memory_slots));

	int reg = memory_state ++;
	memory_slots[reg] = id;
	if (memory_usage < memory_state) {
		memory_usage = memory_state;
	}

	return reg;
}

int to_any_mem(Parser *parser, treeID id) {
	int reg;
	reg=get_mem(parser,id);
	if (reg==NO_SLOT) {
		reg=to_mem(parser,id,-1,1);
	}
	ASSERT(reg != NO_SLOT);
	return reg;
}

static void gen_tree(Parser *parser, treeID id);

static elf_Proto gen_proto(Parser *parser, treeID tree){
	ASSERT(memory_state_index==0);
	ASSERT(memory_state==0);
	ASSERT(memory_usage==0);
	elf_Proto proto = {};
	proto.arity=1;
	proto.bytes=parser->R->M->nbytes;
	push_mem_state(parser);
	set_mem(parser,(treeID)1);
	gen_tree(parser,tree);
	pop_mem_state(parser);
	proto.nlocals=memory_usage;
	proto.nbytes=parser->R->M->nbytes-proto.bytes;
	ASSERT(memory_state==0);
	ASSERT(memory_state_index==0);
	memory_usage=0;
	return proto;
}
static elf_Proto gen_subproto(Parser *parser, treeID tree){
	ASSERT(tree->kind==TREE_FUNCTION);
	return gen_proto(parser,tree->expr_fun.body);
}

static elf_Proto gen_file(Parser *parser, treeID tree){
	// ASSERT(tree->kind==TREE_FUNCTION);
	elf_Proto proto,subproto;
	treeID it;

	int proto_index;
	FOR_ARRAY(i,parser->functions){
		it=parser->functions[i];
		ASSERT(it->kind==TREE_FUNCTION);
		proto_index=elf_add_proto(parser->R);
		it->expr_fun.proto=proto_index;
	}
	//todo: proto 0 should be entry...
	// proto_index=elf_add_proto(parser->R);
	proto=gen_proto(parser,tree);

	FOR_ARRAY(i,parser->functions){
		it=parser->functions[i];
		ASSERT(it->expr_fun.proto!=-1);
		subproto=gen_subproto(parser,it);
		parser->R->M->protos[it->expr_fun.proto]=subproto;
	}
	return proto;
}

static void gen_tree(Parser *parser, treeID id) {
	treeT tree;

	tree=get_tree(parser,id);
	switch(tree.kind) {
		case TREE_ASSIGN_MEM:{
			int mem;
			mem=to_mem(parser,tree.x,-1,1);
			ASSERT(mem!=-1);
		} break;
		case TREE_RET: {
			emit_bytex(parser,tree.line,BC_LEAVE,0);
		} break;
		case TREE_WHILE_LOOP: {
			treeID pred,body;
			int entry;
			BooleanJumps js = {0};

			pred=tree.stat_while.pred;
			body=tree.stat_while.body;
			push_mem_state(parser);

			entry=parser->R->M->nbytes;
			emit_jump_if_false(parser,&js,pred);
			gen_tree(parser,body);
			emit_jump(parser,NO_LINE,entry);

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

			dst=get_mem(parser,tree.x);
			if(dst!=NO_SLOT){
				mem=to_mem(parser,tree.y,dst,1);
				ASSERT(mem==dst);
				ASSERT(xx.kind!=EXPR_FIELD);
				ASSERT(xx.kind!=EXPR_INDEX);
				ASSERT(xx.kind!=EXPR_GLOBAL_REF);
			}else if(xx.kind==EXPR_FIELD||xx.kind==EXPR_INDEX){
				rz=to_any_mem(parser,tree.y);
				rx=to_any_mem(parser,xx.x);
				ry=to_any_mem(parser,xx.y);
				op=xx.kind==EXPR_INDEX?BC_SETINDEX:BC_SETFIELD;
				emit_bytexyz(parser,tree.line,op,rx,ry,rz);
			}else if(xx.kind==EXPR_GLOBAL_REF){
				rx=xx.expr_global;
				rz=to_any_mem(parser,tree.y);
				emit_bytexy(parser,tree.line,BC_SETGLOBAL,rx,rz);
			}else{
				parser_dialog(parser,tree.line,"invalid store, operand must be a global or field or index or memory");
			}
		} break;
		case STAT_BLOCK: {
			push_mem_state(parser);
			FOR_ARRAY(i,tree.z) {
				gen_tree(parser,tree.z[i]);
			}
			pop_mem_state(parser);
		} break;
		case STAT_IF: {
			ASSERT(tree.stat_if.pred);
			ASSERT(tree.stat_if.true_clause);
			BranchJumps s={};
			begin_if(parser,tree.line,&s,tree.stat_if.pred,0);
			gen_tree(parser,tree.stat_if.true_clause);
			if(tree.stat_if.else_clause){
				add_else_clause(parser,tree.line,&s);
				gen_tree(parser,tree.stat_if.else_clause);
			}
			close_if(parser,tree.line,&s);
		} break;
		default: {
			to_mem(parser,id,-1,0);
		} break;
	}
}
static int to_mem(Parser *parser, treeID id, int dst, int ndst) {
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
		ASSERT(dst!=mem);
		emit_bytexy(parser,line,BC_RELOAD,dst,mem);
		goto esc;
	}
	switch (tree.kind) {
		case EXPR_THIS_REF: {
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_RELOAD,dst,0);
		} break;
		case EXPR_GLOBAL_REF: {
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
		case EXPR_TAB:{
			if (ndst<1) goto esc;
			if (dst<0) dst=set_mem(parser,id);
			emit_bytexy(parser,line,BC_TABLE,dst,0);
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
			ASSERT(id->expr_fun.proto!=-1);

			if (ndst<1) goto esc;
			mem=get_mem_state(parser);
			// mem=get_mem_state(fs); {
			// 	FOR_ARRAY(i,node.z) {
			// 		emit_eval_deprecated(C,0,dst_alloc(fs),1,node.z[i]);
			// 	}
			// }
			set_mem_state(parser,mem);
			if (dst<0) dst=set_mem(parser,id);
			int proto;
			proto=tree.expr_fun.proto;
			emit_bytexy(parser,line,BC_CLOSURE,mem,proto);
			if (mem!=dst) {
				emit_bytexy(parser,line,BC_RELOAD,dst,mem);
			}
		} break;
		case EXPR_CALL: {
			treeT xx;
			int nargs;

			mem=get_mem_state(parser);
			ASSERT(dst<mem);

			xx=*tree.x;
			if ((xx.kind==EXPR_FIELD)||(xx.kind==EXPR_METAFIELD)) {
				ry=to_mem(parser,xx.y,-1,1);
				rx=to_mem(parser,xx.x,-1,1);
				emit_bytexyz(parser,line,tree2o(xx.kind),ry,rx,ry);
			} else {
				ry=to_mem(parser,                    tree.x,-1,1);
				rx=to_mem(parser,tree_this_ref(parser,line),-1,1);
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
		default: {
			parser_dialog(parser,line,"invalid tree (%s)",tree2s[tree.kind]);
			NO_CODE;
		}
	}
	ASSERT(dst!=NO_SLOT);
	treeID prox;
	for(prox=tree.prox;prox;prox=prox->prox){
		gen_tree(parser,prox);
	}
	esc:
	return dst;
}

int tree2o(int kind) {
	switch (kind) {
		case EXPR_FIELD: 	   return BC_GETFIELD;     // *
		case EXPR_INDEX: 	   return BC_GETINDEX;     // *
		case EXPR_METAFIELD: return BC_GETMETAFIELD; // *
		case EXPR_CALL:      return BC_CALL;         // *
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

static int emit_jump(Parser *parser, Source line, int dst) {
	return emit_bytex(parser,line,BC_J,dst-parser->R->M->nbytes);
}

static int emit_branch_if(Parser *parser, BooleanJumps *js, bool if_true, treeID id) {
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


int emit_branch_if_false(Parser *fs, BooleanJumps *js, treeID id) {
	return emit_branch_if(fs,js,0,id);
}


int emit_branch_if_true(Parser *fs, BooleanJumps *js, treeID id) {
	return emit_branch_if(fs,js,1,id);
}


/* similar to branch if true, but additionally all
false jumps converge here */
int *emit_jump_if_true(Parser *fs, BooleanJumps *js, treeID id) {
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


Instr *emit_jump_if_false(Parser *fs, BooleanJumps *js, treeID id) {
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}


int *emit_jump_if_not_nil(Parser *parser, Source line, BooleanJumps *js, treeID id) {
	return emit_jump_if_false(parser,js,tree_xy(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}


int *emit_jump_if_nil(Parser *parser, Source line, BooleanJumps *js, treeID id) {
	return emit_jump_if_true(parser,js,tree_xy(parser,line,EXPR_EQ,NT_BOL,id,tree_nil(parser,line)));
}

static void begin_if(Parser *parser, Source line, BranchJumps *s, treeID x, int if_true) {
	BooleanJumps js = {0};
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
void add_else_clause(Parser *fs, Source line, BranchJumps *s) {
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


void add_elif_clause(Parser *parser, Source line, BranchJumps *s, treeID x) {
	add_else_clause(parser,line,s);
	begin_if(parser,line,s,x,0);
}


void add_then_clause(Parser *parser, Source line, BranchJumps *s) {
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


void close_if(Parser *fs, Source line, BranchJumps *s) {
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







#if 0
int to_mem(Compiler *C, int flags, int reg, int nreg, treeID id) {
	ASSERT(reg == NO_SLOT);
	elf_State *S;
	treeT node;
	Source line;

	S=C->R;
	node=get_tree(C->P,id);
	line=node.line;

	ASSERT(node.type != NT_NON);

	int mem,rx,ry,rz;

	mem=get_mem(C,id);
	if (mem != NO_SLOT) {
		if (nreg<1) goto esc;
		ASSERT(reg != mem);
		if(reg<0)reg=set_mem(C,id);
		// ASSERT(reg != NO_SLOT);
		emit_bytexy(C,line,BC_RELOAD,reg,mem);
	}

	switch (node.kind) {
		case IR_LOCAL: {
			ASSERT(mem!=NO_SLOT);
		} break;
		case IR_LOAD: {
			if (nreg<1) goto esc;
			ASSERT(reg == NO_SLOT);
			reg = to_mem(C,0,-1,nreg,node.x);
			ASSERT(reg != NO_SLOT);
		} break;
		//todo: hack!
		case IR_LOAD_DIRECT: {
			if (nreg<1) goto esc;
			ASSERT(reg == NO_SLOT);
			ASSERT(node.x < C->memory_state);

			reg = set_mem(C,id);
			emit_bytexy(C,line,BC_RELOAD,reg,node.x);

			ASSERT(reg != NO_SLOT);
		} break;
		case IR_CLSVAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			emit_bytexy(C,line,BC_GETCLOSED,reg,node.x);
		} break;
		case IR_GLOBAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);
			emit_bytexy(C,line,BC_GETGLOBAL,reg,node.x);
		} break;
		case IR_NIL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			emit_bytexy(C,line,BC_LOADNIL,reg,0);
		} break;
		case IR_INTEGER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			int yy = elf_add_const_int(S,node.i);
			emit_bytexy(C,line,BC_GETKINT,reg,yy);
		} break;
		case IR_NUMBER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			/* todo: interning */
			int yy = elf_add_const_num(S,node.n);
			emit_bytexy(C,line,BC_GETKNUM,reg,yy);
		} break;
		case IR_STRING: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			/* todo: interning */
			int xx = elf_set_global(S->M,0,VSTR(elf_alloc_string(C->R,node.s)));
			emit_bytexy(C,line,BC_GETGLOBAL,reg,xx);
		} break;
		case IR_TABLE: {
			if (reg<0) reg=set_mem(C,id);
			/* we can't skip here because of possible side effects
			in the table's initializer, instead let the parser turn
			this into sugar */
			emit_bytexy(C,line,BC_TABLE,reg,0);

			/* todo: turn this into sugar? we can't yet... */
			//  xxx FOR_ARRAY(i,node.z) {
			//  xxx 		emit_field_initer(C,line,reg,node.z[i]);
			//  xxx }
		} break;
		case IR_METAFIELD:
		case IR_FIELD: case IR_INDEX: {
			if (nreg<1) goto esc;
			mem=get_mem_state(C);{
				rx=to_any_mem(C,node.x);
				ry=to_any_mem(C,node.y);
			} set_mem_state(C,mem);
			if (reg<0) reg=set_mem(C,id);
			emit_bytexyz(C,line,tree2o(node.kind),reg,rx,ry);
		} break;
#if 0
		case IR_CLOSURE: {
			if (nreg<1) goto esc;
			/* todo: why is it that we can't just start at reg */
			mem=get_mem_state(fs); {
				FOR_ARRAY(i,node.z) {
					emit_eval_deprecated(C,0,reg_alloc(fs),1,node.z[i]);
				}
			} set_mem_state(C,mem);
			if (reg<0) reg=reg_alloc(fs);
			emit_bytexy(C,line,BC_CLOSURE,mem,node.x);
			if (mem!=reg) {
				emit_bytexy(C,line,BC_RELOAD,reg,mem);
			}
		} break;
		case IR_TYPEGUARD: {
			if (nreg<1) goto esc;
			reg=emit_eval_deprecated(C,flags,reg,nreg,node.x);
			emit_bytexy(C,node.line,BC_TYPEGUARD,reg,node2tag(node.y));
		} break;
		/* why is this even a thing dude */
		case IR_GROUP: {
			reg=emit_eval_deprecated(C,flags,reg,nreg,node.x);
		} break;

		/* (a !! b) = (a == nil ? a : b) */
		case IR_NIL_AND: {
			BooleanJumps e = {0};
			Instr *js;

			if (reg<0)reg=reg_alloc(fs);
			emit_eval_deprecated(C,0,reg,1,node.x);
			js=emit_jump_if_nil(C,NO_LINE,&e,tree_local(C,NO_LINE,reg));
			emit_eval_deprecated(C,0,reg,1,node.y);
			patch_jumps(C,js);
			ARRAY_DELETE(js);
		} break;
		case IR_NIL_OR: {
			BooleanJumps e={0};
			Instr *js;

			if (reg<0)reg=reg_alloc(fs);
			emit_eval_deprecated(C,0,reg,1,node.x);
			js=emit_jump_if_not_nil(C,NO_LINE,&e,tree_local(C,get_tree_line(C,node.x),reg));
			emit_eval_deprecated(C,0,reg,1,node.y);
			patch_jumps(C,js);
			ARRAY_DELETE(js);
		} break;
		case IR_AND: case IR_OR: {
			// here's the bit that does short circuiting,
			// I believe I found a bug in this implementation,
			// I was working on some other compiler, and I
			// noticed that here I was doing things in a slightly
			// different way, which would result in a bug if
			// certain conditions were met, so I have to get to
			// that, has to do with the jump patching stuff.
			BooleanJumps e = {0};
			// now I'm wondering, why are we allocating a result
			// register here, as suppossed to letting eval allocate
			// one? Todo: check this...
			if (reg<0)reg = reg_alloc(fs);
			emit_eval_deprecated(C,0,reg,1,tree_int(C,line,0));
			Instr *js = emit_jump_if_false(C,&e,id);
			emit_eval_deprecated(C,0,reg,1,tree_int(C,line,1));
			patch_jumps(C,js);
			ARRAY_DELETE(js);
		} break;
#endif
		case IR_EQ: case IR_NEQ:
		case IR_GT: case IR_GTEQ: case IR_LT: case IR_LTEQ:
		case IR_DIV: case IR_MUL: case IR_MOD:
		case IR_SUB: case IR_ADD: case IR_POW:
		case IR_BIT_SHL: case IR_BIT_SHR:
		case IR_BIT_XOR:
		case IR_BIT_AND: case IR_BIT_OR: {
			/* note: can't before operands are evaluated because of possible side effects */
			if ((node.kind==IR_GT)||(node.kind==IR_GTEQ)) {
				/* flip the operator and flip the operands */
				mem=get_mem_state(C); {
					rx=to_any_mem(C,node.y);
					ry=to_any_mem(C,node.x);
				} set_mem_state(C,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=set_mem(C,id);
				emit_bytexyz(C,node.line,tree2o(node.k^1),reg,rx,ry);
			} else {
				mem=get_mem_state(C); {
					rx=to_any_mem(C,node.x);
					ry=to_any_mem(C,node.y);
				} set_mem_state(C,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=set_mem(C,id);
				elf_debug_log("%s %i, %i, %i",node2s[node.kind], reg,rx,ry);
				emit_bytexyz(C,node.line,tree2o(node.kind),reg,rx,ry);
			}
		} break;
		case IR_CALL: {
			treeT xx;
			int nargs;


			mem=get_mem_state(C); {
				ASSERT(reg<mem);
				/* meta-call */
				xx=get_tree(C->P,node.x);
				if ((xx.kind==IR_FIELD)||(xx.kind==IR_METAFIELD)) {
					ry=to_mem(C,0,-1,1,xx.y);
					rx=to_mem(C,0,-1,1,xx.x);
					emit_bytexyz(C,line,tree2o(xx.kind),ry,rx,ry);
				} else {
					/* regular call with context 'this' */
					ry=to_mem(C,0,-1,1,node.x);
					rx=to_mem(C,0,-1,1,ir_this(C->P,line));
				}
				ASSERT(ry==mem+0);
				ASSERT(rx==mem+1);
				FOR_ARRAY(i,node.z) {
					rz=to_mem(C,0,-1,1,node.z[i]);
					ASSERT(rz==mem+2+i);
				}
			} set_mem_state(C,mem);

			nargs=ARRAY_LENGTH(node.z)+1;
			emit_bytexyz(C,line,BC_CALL,mem,nargs,nreg);

			if (nreg<1) goto esc;
			if (nreg>1) parser_dialog(C->P,line,"multi-returns are not supported yet!");

			if (reg<0) reg=set_mem(C,id);
			//todo: call instruction that puts
			//the result in a specific registers
			if (reg!=mem) {
				emit_bytexy(C,line,BC_RELOAD,reg,mem);
			}
		} break;
		default: {
			parser_dialog(C->P,line,"invalid node (%s)",node2s[node.kind]);
			NO_CODE;
		}
	}
	esc:
	return reg;
}


static FileBlock *get_block(Parser *fs, BlockId id) {
	__debugbreak();
	return 0;
}


#if 0

static void emit_store_deprecated(Parser *fs, Source line, treeID x, treeID y);
static int emit_eval_deprecated(Parser *fs, int flags, int reg, int nreg, treeID id);
static int any_reg_deprecated(Parser *fs, treeID id);
static int emit_preload_deprecated(Parser *fs, treeID id);



// to avoid so many errors spewing out we'll do this
static int get_mem_state_deprecated(Parser *fs) {
	__debugbreak();
	// return fs->fn->xmemory;
	return -1;
}
static void set_mem_state_deprecated(Parser *fs, int memory) {
	__debugbreak();
	// fs->fn->xmemory=memory;
}




static Bytecode get_byte(Parser *fs, int instr) {
	return fs->M->bytes[instr];
}




static int emit_byte_deprecated(Parser *fs, Source line, Bytecode byte) {
	BC_Module *M=fs->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	ARRAY_ADD(M->track,0);
	// fpf_byte(stdout,M,-1,M->nbytes-fs->fn->bytes,byte);
	return M->nbytes ++;
}


static int emit_bytex_deprecated(Parser *fs, Source line, int k, int x) {
	__debugbreak();
	Bytecode byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte_deprecated(fs,line,byte);
}


static int emit_bytexy_deprecated(Parser *fs, Source line, int k, int x, int y) {
	Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte_deprecated(fs,line,byte);
}


static int emit_bytexyz_deprecated(Parser *fs, Source line, int k, int x, int y, int z) {
	Bytecode byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte_deprecated(fs,line,byte);
}


static FileBlock *get_block(Parser *fs, BlockId id) {
	return 0; // &fs->blocks[id > -1 ? id : fs->nblocks + id];
}


/* todo: the block ended flag could be added automatically when
we add a terminating byte to the current block */
static void add_block_flags(Parser *fs, int flags) {
	get_block(fs,-1)->flags |= flags;
}

// ok, so this sort of seems to be called
// right when we need a register for some
// instruction
static int reg_alloc_deprecated(Parser *fs) {
	__debugbreak();
	int reg = 0; // fs->fn->xmemory ++;
	ASSERT(reg<=0xff);
	// fs->fn->nlocals = MAX(fs->fn->nlocals,fs->fn->xmemory);

	FileBlock *bl = get_block(fs,-1);
	// bl->nlocals = fs->fn->nlocals;
	return reg;
}


// todo: re-add support for specifying which
// loop you're referring to
static int get_loop_reg(Parser *F, int type) {
	FileBlock *bl = get_loop_block(F,-1);
	ASSERT(bl != 0);
	ASSERT(bl->flags & BLOCK_LOOP);
	int reg = NO_SLOT;
	switch (type) {
		case SPECIAL_REGISTER_ARRAY: {
			reg = bl->loop.array_register;
		} break;
		case SPECIAL_REGISTER_VALUE: {
			reg = bl->loop.value_register;
		} break;
		case SPECIAL_REGISTER_INDEX: {
			reg = bl->loop.index_register;
		} break;
		default: NO_CODE;
	}
	return reg;
}



FileBlock *get_loop_block(Parser *fs, elf_StackId reg) {
	__debugbreak();
	#if 0
	int level;
	for (level = fs->nblocks-1; level > -1; -- level) {
		FileBlock *bl = get_block(fs,level);
		if (bl->flags & BLOCK_LOOP) {
			if (reg < 0) return bl; else
			if (bl->loop.value_register == reg) return bl;
		}
	}
	#endif
	return 0;
}

// NOTE: This is no longer necessary!
#if 0
void begin_delay_block(Parser *fs, Source line) {
	Instr jo = emit_bytex_deprecated(fs,line,BC_DELAY,NO_JUMP);
	BlockId id = parser_begin_block(fs,BLOCK_DELAYED);
	fs->blocks[id].jumpover = jo;
}
void close_delay_block(Parser *fs, Source line) {
	FileBlock *bl = get_block(fs,-1);
	emit_bytex_deprecated(fs,line,BC_LEAVE,0);
	add_block_flags(fs,BLOCK_ENDED);
	parser_close_block(fs);
	ASSERT(bl->entry != bl->jumpover);
	patch_jump(fs,bl->jumpover);
}
#endif



void desugar_range_expr_epilogue(Parser *fs, treeID x) {
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


treeID desugar_range_expr(Parser *fs, treeID x, int flags) {
#if 0
	treeT node;
	Source line;
	treeID xx;
	treeID array,index,value;

	node=get_target_node(fs,TREEID(x));
	switch (node.kind) {
		case IR_INDEX: case IR_FIELD: {
			xx=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);
			return tree_xy(fs,node.line,node.kind,NT_ANY,xx,node.y);
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
			if (hi==NO_TREE) hi=tree_call_metafield(fs,line,array,0,"length");

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
#endif
	// Todo: come back to this later
	__debugbreak();
	return NO_TREE;
}


void emit_field_initer(Parser *fs, Source line, int reg, treeID id) {
	treeT node;
	int mem,xx,xy,yy;

	node=get_tree(fs,id);
	mem=get_mem_state_deprecated(fs);
	switch (node.kind) {
		case IR_STORE: {
			treeT x = get_tree(fs,node.x);
			if ((x.kind == IR_LOCAL)) {
				NO_CODE;
			} else if ((x.kind == IR_FIELD) || (x.k == IR_INDEX)) {
				xx=reg;
				xy=any_reg_deprecated(fs,x.y);
				yy=any_reg_deprecated(fs,node.y);
				if (x.kind==IR_FIELD) {
					emit_bytexyz_deprecated(fs,line,BC_SETFIELD,xx,xy,yy);
				} else {
					emit_bytexyz_deprecated(fs,line,BC_SETINDEX,xx,xy,yy);
				}
			} else NO_CODE;
		} break;
		default: NO_CODE;
	}
	set_mem_state_deprecated(fs,mem);
}


#if 0
static int get_treereg_deprecated(Parser *fs, treeID2 id) {
	if (id.id==NO_TREE) return NO_SLOT;
	treeT node;
	int reg=-1;
	node=get_target_node(fs,id);
	if (node.kind==IR_LOCAL) {
		reg=node.x;
		if (reg>0xff) {
			reg=get_loop_reg(fs,reg);
			if (reg==NO_SLOT) {
				parser_dialog(fs,node.line,"invalid register");
			}
		}
	}
	return reg;
}



/* emits code when the node is not already a register to
load that node into any register */
// ok, seems  there's a function specifically for
// this, allocating a register for an instruction when
// it doesn't already have one.
int any_reg_deprecated(Parser *fs, treeID id) {
	int reg;
	reg=get_treereg_deprecated(fs,TREEID(id));
	if (reg==NO_SLOT) {
		reg=emit_eval_deprecated(fs,0,-1,1,id);
		ASSERT(reg!=-1);
	}
	return reg;
}


static int find_local_const_store(Parser *fs, int reg) {
	if (fs->nloops > 0) {
		return -1;
	}
	Bytecode byte, *bytes;
	int nbytes;

	bytes=fs->M->bytes;
	nbytes=fs->M->nbytes;

	int i;
	for (i=nbytes-1; i>=fs->fn->bytes; --i) {
		byte=bytes[i];
		if (
		(BC_OP(byte)==BC_GETKNUM)||
		(BC_OP(byte)==BC_GETKINT)||
		(BC_OP(byte)==BC_LOADNIL)){
			if (BC_ARGX(byte)==reg) {
				return i;
			}
		}
	}
	return -1;
}


void opt_const_fold(Parser *fs, treeT node) {
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
#endif





/* the instruction is always loaded in the target register,
even if it is already loaded, if no target register is given,
a new one is allocated */
int emit_eval_deprecated(Parser *fs, int flags, int reg, int nreg, treeID id) {
	BC_Module *M;
	treeT node;
	Source line;
	int mem, rx,ry;

	M=fs->M;
	node=get_tree(fs,id);
	line=node.line;

	switch (node.kind) {

		// We won't have "LOCALS" anymore,
		// because before, a local used to
		// represent an actual memory location,
		// now the parser has no memory state
		// knowledge, so it can't produce memory
		// locations, instead we assign registers
		// to expressions as we need here in the
		// compilation stage
#if 0
		case IR_LOCAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);
			int src;
			src=get_treereg_deprecated(fs,TREEID(id));
			if (src<0||src>=get_mem_state_deprecated(fs)) {
				parser_dialog(fs,line,"invalid memory state!");
			}
			ASSERT(src>=0&&src<get_mem_state_deprecated(fs));
			emit_bytexy_deprecated(fs,line,BC_RELOAD,reg,src);
		} break;
#endif

		case IR_CLSVAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			emit_bytexy_deprecated(fs,line,BC_GETCLOSED,reg,node.x);
		} break;
		case IR_GLOBAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			emit_bytexy_deprecated(fs,line,BC_GETGLOBAL,reg,node.x);
		} break;
		case IR_NIL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			emit_bytexy_deprecated(fs,line,BC_LOADNIL,reg,0);
		} break;
		case IR_INTEGER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->integers,1);
			M->integers[yy] = node.i;
			emit_bytexy_deprecated(fs,line,BC_GETKINT,reg,yy);
		} break;
		case IR_NUMBER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->numbers,1);
			M->numbers[yy] = node.n;
			emit_bytexy_deprecated(fs,line,BC_GETKNUM,reg,yy);
		} break;
		case IR_STRING: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc_deprecated(fs);

			/* todo: interning */
			int xx = elf_set_global(M,0,VSTR(elf_alloc_string(fs->R,node.s)));
			emit_bytexy_deprecated(fs,line,BC_GETGLOBAL,reg,xx);
		} break;
		case IR_TABLE: {
			if (reg<0) reg=reg_alloc_deprecated(fs);

			emit_bytexy_deprecated(fs,line,BC_TABLE,reg,0);
			/* todo: turn this into sugar? we can't yet... */
			FOR_ARRAY(i,node.z) {
				emit_field_initer(fs,line,reg,node.z[i]);
			}
		} break;
		case IR_METAFIELD:
		case IR_FIELD: case IR_INDEX: {
			if (nreg<1) goto esc;
			mem=get_mem_state_deprecated(fs); {
				rx=any_reg_deprecated(fs,node.x);
				ry=any_reg_deprecated(fs,node.y);
			} set_mem_state_deprecated(fs,mem);
			if (reg<0) reg=reg_alloc_deprecated(fs);
			emit_bytexyz_deprecated(fs,line,tree2o(node.kind),reg,rx,ry);
		} break;
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
		/* why is this even a thing dude */

		// xx case IR_GROUP: {
		// xx 	reg=emit_eval_deprecated(fs,flags,reg,nreg,node.x);
		// xx } break;

		/* ({x}).{y}(...) will not be recognized as a meta-call
		because it's wrapped in parenthesis, could this be
		a feature? (I don't think so silly) */
		case IR_CALL: {
			treeT xx;
			int nargs;

			xx=get_tree(fs,node.x);

			mem=get_mem_state_deprecated(fs); {
				ASSERT(reg<mem);
				if ((xx.k==IR_FIELD)||(xx.k==IR_METAFIELD)) {
					ry=emit_eval_deprecated(fs,0,-1,1,xx.y);
					rx=emit_eval_deprecated(fs,0,-1,1,xx.x);
					emit_bytexyz_deprecated(fs,line,tree2o(xx.kind),ry,rx,ry);
					ASSERT(ry==mem);
				} else {
					ry=emit_eval_deprecated(fs,0,-1,1,node.x);
					rx=emit_eval_deprecated(fs,0,-1,1,tree_local(fs,line,0));
					ASSERT(ry==mem);
				}
				FOR_ARRAY(i,node.z) {
					emit_eval_deprecated(fs,0,-1,1,node.z[i]);
				}
			} set_mem_state_deprecated(fs,mem);

			nargs=ARRAY_LENGTH(node.z)+1;
			emit_bytexyz_deprecated(fs,line,BC_CALL,mem,nargs,nreg);
			if (nreg<1) goto esc;
			if (nreg>1) parser_dialog(fs,line,"unsupported");
			if (reg<0) reg=reg_alloc_deprecated(fs);
			if (reg!=mem) {
				emit_bytexy_deprecated(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		/* (a !! b) = (a == nil ? a : b) */
		case IR_NIL_AND: {
			BooleanJumps e = {0};
			Instr *js;

			if (reg<0)reg=reg_alloc_deprecated(fs);
			emit_eval_deprecated(fs,0,reg,1,node.x);
			js=emit_jump_if_nil(fs,NO_LINE,&e,tree_local(fs,NO_LINE,reg));
			emit_eval_deprecated(fs,0,reg,1,node.y);
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case IR_NIL_OR: {
			BooleanJumps e={0};
			Instr *js;

			if (reg<0)reg=reg_alloc_deprecated(fs);
			emit_eval_deprecated(fs,0,reg,1,node.x);
			js=emit_jump_if_not_nil(fs,NO_LINE,&e,tree_local(fs,get_tree_line(fs,node.x),reg));
			emit_eval_deprecated(fs,0,reg,1,node.y);
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case IR_AND: case IR_OR: {
			// here's the bit that does short circuiting,
			// I believe I found a bug in this implementation,
			// I was working on some other compiler, and I
			// noticed that here I was doing things in a slightly
			// different way, which would result in a bug if
			// certain conditions were met, so I have to get to
			// that, has to do with the jump patching stuff.
			BooleanJumps e = {0};
			// now I'm wondering, why are we allocating a result
			// register here, as suppossed to letting eval allocate
			// one? Todo: check this...
			if (reg<0)reg = reg_alloc_deprecated(fs);
			emit_eval_deprecated(fs,0,reg,1,tree_int(fs,line,0));
			Instr *js = emit_jump_if_false(fs,&e,id);
			emit_eval_deprecated(fs,0,reg,1,tree_int(fs,line,1));
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case IR_EQ: case IR_NEQ:
		case IR_GT: case IR_GTEQ: case IR_LT: case IR_LTEQ:
		case IR_DIV: case IR_MUL: case IR_MOD:
		case IR_SUB: case IR_ADD: case IR_POW:
		case IR_BIT_SHL: case IR_BIT_SHR:
		case IR_BIT_XOR:
		case IR_BIT_AND: case IR_BIT_OR: {
			if ((node.kind==IR_GT)||(node.kind==IR_GTEQ)) {
				mem = get_mem_state_deprecated(fs); {
					rx = any_reg_deprecated(fs,node.y);
					ry = any_reg_deprecated(fs,node.x);
				} set_mem_state_deprecated(fs,mem);
				if (nreg < 1) goto esc;
				if (reg < 0) reg = reg_alloc_deprecated(fs);
				emit_bytexyz_deprecated(fs,node.line,tree2o(node.k^1),reg,rx,ry);
			} else {
				mem=get_mem_state_deprecated(fs); {
					rx=any_reg_deprecated(fs,node.x);
					ry=any_reg_deprecated(fs,node.y);
				} set_mem_state_deprecated(fs,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=reg_alloc_deprecated(fs);
				emit_bytexyz_deprecated(fs,node.line,tree2o(node.kind),reg,rx,ry);
			}
		} break;
		default: {
			parser_dialog(fs,line,"invalid node (%s)",node2s[node.kind]);
			NO_CODE;
		}
	}
	esc:
	return reg;
}


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
treeID emit_preload_deprecated(Parser *fs, treeID x) {
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

#if 0
void emit_store_deprecated(Parser *fs, Source line, treeID x, treeID y) {
	__debugbreak();

	treeT node;
	node=get_target_node(fs,TREEID(x));
	if (y<0) {
		parser_dialog(fs,line,"invalid statement, expected a value for assignment");
	}
	if (!tree_is_lvalue(node.kind)) {
		parser_dialog(fs,line,"invalid assignment to (%s)",node2s[node.kind]);
		elf_fail(fs->R,0,"syntax error: invalid assignment");
	}

	if (node.kind==IR_LOCAL) {
		entityID id;
		id=find_local_entity(fs,node.x);
		/* todo: */
		if(id!=-1){
			if (fs->entities[id].flags & ENTITY_CONSTANT) {
				parser_dialog(fs,line,"invalid assignment to constant entity");
				elf_fail(fs->R,0,"syntax error: invalid assignment to constant entity");
			}
			fs->entities[id].flags|=ENTITY_ASSIGNED;
		}
	}

	ASSERT(node.level<fs->nblocks);
	ASSERT(x>=0);
	ASSERT(y>=0);
	if (node.line!=0) line=node.line;

	int rx,ry,rz;

	rx=node.x;
	switch (node.kind) {
		case IR_GLOBAL: {
			ry=any_reg_deprecated(fs,y);
			emit_bytexy_deprecated(fs,line,BC_SETGLOBAL,rx,ry);
		} break;
		case IR_LOCAL: {
			emit_eval_deprecated(fs,0,rx,1,y);
		} break;
		ByteOP op;
		case IR_INDEX: case IR_FIELD: {
			rz=any_reg_deprecated(fs,y);
			rx=any_reg_deprecated(fs,node.x);
			ry=any_reg_deprecated(fs,node.y);
			op=node.kind==IR_INDEX?BC_SETINDEX:BC_SETFIELD;
			emit_bytexyz_deprecated(fs,line,op,rx,ry,rz);
		} break;
		case IR_CLSVAL: {
			parser_dialog(fs,line,"assignment of closure value is not possible");
		} break;
		case IR_METAFIELD: {
			parser_dialog(fs,line,"assignment of metafields is not possible");
		} break;
		default: {
			NO_CODE;
		} break;
	}
}
#endif

#endif

// Note the mapping (*)
ByteOP tree2o(treeKi kind) {
	switch (kind) {
		case IR_FIELD: 	 return BC_GETFIELD;     // *
		case IR_INDEX: 	 return BC_GETINDEX;     // *
		case IR_METAFIELD: return BC_GETMETAFIELD; // *
		case IR_CALL:      return BC_CALL;         // *
		// all the arithmetic stuff is the same
		case IR_ADD:       return BC_ADD;
		case IR_SUB:       return BC_SUB;
		case IR_DIV:       return BC_DIV;
		case IR_MUL:       return BC_MUL;
		case IR_POW:       return BC_POW;
		case IR_MOD:       return BC_MOD;
		case IR_NEQ:       return BC_NEQ;
		case IR_EQ:        return BC_EQ;
		case IR_LT:        return BC_LT;
		case IR_LTEQ:      return BC_LTEQ;
		case IR_BIT_OR:    return BC_BIT_OR;
		case IR_BIT_AND:   return BC_BIT_AND;
		case IR_BIT_SHL:   return BC_SHL;
		case IR_BIT_SHR:   return BC_SHR;
		case IR_BIT_XOR:   return BC_BIT_XOR;
		default: NO_CODE;
	}
	return BC_HALT;
}



#endif