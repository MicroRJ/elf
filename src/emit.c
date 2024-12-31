/*
** See Copyright Notice In elf.h
** emit.c
** elf_Bytecode Generation
*/



static elf_Bytecode get_byte(FileState *fs, int instr) {
	return fs->M->bytes[instr];
}


static int get_instr_cursor(FileState *fs) {
	return fs->M->nbytes;
}


static int get_mem_state(FileState *fs) {
	return fs->fn->xmemory;
}


static void set_mem_state(FileState *fs, int memory) {
	fs->fn->xmemory=memory;
}


static int emit_byte(FileState *fs, Source line, elf_Bytecode byte) {
	elf_Module *M=fs->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	ARRAY_ADD(M->track,0);
	// fpf_byte(stdout,M,-1,M->nbytes-fs->fn->bytes,byte);
	return M->nbytes ++;
}


static int emit_bytex(FileState *fs, Source line, int k, int x) {
	elf_Bytecode byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(fs,line,byte);
}


static int emit_bytexy(FileState *fs, Source line, int k, int x, int y) {
	elf_Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(fs,line,byte);
}


static int emit_bytexyz(FileState *fs, Source line, int k, int x, int y, int z) {
	elf_Bytecode byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(fs,line,byte);
}


static FileBlock *get_block(FileState *fs, BlockId id) {
	return &fs->blocks[id > -1 ? id : fs->nblocks + id];
}


/* todo: the block ended flag could be added automatically when
we add a terminating byte to the current block */
static void add_block_flags(FileState *fs, int flags) {
	get_block(fs,-1)->flags |= flags;
}


static int reg_alloc(FileState *fs) {
	int reg = fs->fn->xmemory ++;
	ASSERT(reg<=0xff);
	fs->fn->nlocals = MAX(fs->fn->nlocals,fs->fn->xmemory);

	FileBlock *bl = get_block(fs,-1);
	bl->nlocals = fs->fn->nlocals;
	return reg;
}


// todo: re-add support for specifying which
// loop you're referring to
static int get_loop_reg(FileState *F, int type) {
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


static int get_node_register(FileState *fs, NodeIdGuard id) {
	if (id.id==NO_NODE) return NO_SLOT;
	Node node;
	int reg=-1;
	node=get_target_node(fs,id);
	if (node.kind==NODE_LOCAL) {
		reg=node.x;
		if (reg>0xff) {
			reg=get_loop_reg(fs,reg);
			if (reg==NO_SLOT) {
				file_dialog(fs,node.line,"invalid register");
			}
		}
	}
	return reg;
}


static void patch_jump2(FileState *fs, int src, int dst) {
	elf_Bytecode byte,*bytes;
	bytes=fs->M->bytes;
	byte=bytes[src];
	int j = dst - src;
	switch (BC_OP(byte)) {
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


static void patch_jumps2(FileState *fs, Instr *js, Instr j) {
	FOR_ARRAY(i,js) {
		patch_jump2(fs,js[i],j);
	}
}


static void patch_jump(FileState *fs, Instr i) {
	patch_jump2(fs,i,get_instr_cursor(fs));
}


static void patch_jumps(FileState *fs, Instr *js) {
	FOR_ARRAY(i,js) {
		patch_jump(fs,js[i]);
	}
}


static Instr emit_jump(FileState *fs, Source line, Instr j) {
	return emit_bytex(fs,line,BC_J,j-fs->M->nbytes);
}


int begin_block(FileState *fs, elf_Bool flags) {
	int level = fs->nblocks ++;
	if (ARRAY_LENGTH(fs->blocks) < fs->nblocks) {
		ARRAY_GROW(fs->blocks,1);
	}

	FileBlock *bl = &fs->blocks[level];
	clear_memory(bl,sizeof(*bl));

	bl->loop.array_register = NO_SLOT;
	bl->loop.index_register = NO_SLOT;
	bl->loop.value_register = NO_SLOT;

	bl->level = level;
	bl->xmemory = fs->fn->xmemory;
	bl->xentity = fs->nentities;
	bl->xnode = fs->nnodes;
	bl->flags = flags;
	bl->entry = get_instr_cursor(fs);
	bl->jumpover = bl->entry;

	fs->nloops += (flags & BLOCK_LOOP) != 0;
	return level;
}


void close_block(FileState *fs) {
	ASSERT(fs->nentities >= fs->fn->entities);
	FileBlock *bl = get_block(fs,-1);
	EntityId id;
	/* xentity is the first entity within a block, if any. */
	for (id = bl->xentity; id < fs->nentities; ++ id) {
		if (~fs->entities[id].flags & ENTITY_REFERENCED) {
			file_dialog(fs,fs->entities[id].line,"'%s': unreferenced entity", fs->entities[id].name);
		}
	}
	fs->nentities = bl->xentity;
	fs->nnodes    = bl->xnode;
	fs->nblocks  -= 1;
	// ASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		patch_jumps(fs,bl->leavejumps);
		ARRAY_DELETE(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->nloops -= (bl->flags & BLOCK_LOOP) != 0;
}


FileBlock *get_loop_block(FileState *fs, elf_StackId reg) {
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


void begin_delay_block(FileState *fs, Source line) {
	Instr jo = emit_bytex(fs,line,BC_DELAY,NO_JUMP);
	BlockId id = begin_block(fs,BLOCK_DELAYED);
	fs->blocks[id].jumpover = jo;
}


void close_delay_block(FileState *fs, Source line) {
	FileBlock *bl = get_block(fs,-1);
	emit_bytex(fs,line,BC_LEAVE,0);
	add_block_flags(fs,BLOCK_ENDED);
	close_block(fs);
	ASSERT(bl->entry != bl->jumpover);
	patch_jump(fs,bl->jumpover);
}


int emit_branch_if(FileState *fs, BooleanJumps *js, elf_Bool if_true, NodeId id) {
	Node node;

	node=get_node(fs,id);
	int mem,reg,jmp;
	switch (node.kind) {
		case NODE_GROUP: {
			jmp=emit_branch_if(fs,js,if_true,node.x);
		} break;
		case NODE_AND: {
			emit_jump_if_false(fs,js,node.x);
			jmp=emit_branch_if(fs,js,if_true,node.y);
		} break;
		case NODE_OR: {
			emit_jump_if_true(fs,js,node.x);
			jmp=emit_branch_if(fs,js,if_true,node.y);
		} break;
		default: {
			mem=get_mem_state(fs);
			reg=emit_load(fs,id);
			if (if_true) {
				jmp=emit_bytexy(fs,node.line,BC_JNZ,NO_JUMP,reg);
				ARRAY_ADD(js->t,jmp);
			} else {
				jmp=emit_bytexy(fs,node.line,BC_JZ,NO_JUMP,reg);
				ARRAY_ADD(js->f,jmp);
			}
			set_mem_state(fs,mem);
		} break;
	}
	return jmp;
}


int emit_branch_if_false(FileState *fs, BooleanJumps *js, NodeId id) {
	return emit_branch_if(fs,js,0,id);
}


int emit_branch_if_true(FileState *fs, BooleanJumps *js, NodeId id) {
	return emit_branch_if(fs,js,1,id);
}


/* similar to branch if true, but additionally all
false jumps converge here */
int *emit_jump_if_true(FileState *fs, BooleanJumps *js, NodeId id) {
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


Instr *emit_jump_if_false(FileState *fs, BooleanJumps *js, NodeId id) {
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}


int *emit_jump_if_not_nil(FileState *fs, Source line, BooleanJumps *js, NodeId id) {
	return emit_jump_if_false(fs,js,node_xy(fs,line,NODE_EQ,NT_BOL,id,node_nil(fs,line)));
}


int *emit_jump_if_nil(FileState *fs, Source line, BooleanJumps *js, NodeId id) {
	return emit_jump_if_true(fs,js,node_xy(fs,line,NODE_EQ,NT_BOL,id,node_nil(fs,line)));
}


void emit_continue(FileState *fs, Source line, int reg) {
	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,reg);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->loop.true_jumps,jmp);
}


void emit_break(FileState *fs, Source line, elf_StackId with_value_register) {
	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,with_value_register);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->leavejumps,jmp);
}


void desugar_range_expr_epilogue(FileState *fs, NodeId x) {
	Node node = get_node(fs,x);
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			desugar_range_expr_epilogue(fs,node.x);
		} break;
		case NODE_RANGE_INDEX: {
			close_range_loop(fs,NO_LINE);
			close_block(fs);
			desugar_range_expr_epilogue(fs,node.x);
		} break;
		default: ;
	}
}


NodeId desugar_range_expr(FileState *fs, NodeId x, elf_Bool flags) {
	Node node;
	Source line;
	NodeId xx;
	NodeId array,index,value;

	node=get_target_node(fs,NODE(x));
	switch (node.kind) {
		case NODE_INDEX: case NODE_FIELD: {
			xx=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);
			return node_xy(fs,node.line,node.kind,NT_ANY,xx,node.y);
		}
		case NODE_RANGE_INDEX: {
			ASSERT(get_node_kind(fs,node.y) == NODE_RANGE);

			line=node.line;
			array=desugar_range_expr(fs,node.x,flags&~EXPR_LHS);

			int array_reg,index_reg,value_reg;
			int block;

			block=begin_block(fs,BLOCK_LOOP);

			array_reg=emit_load(fs,array);
			index_reg=reg_alloc(fs);

			array = node_local(fs,line,array_reg);
			index = node_local(fs,line,index_reg);
			value = node_index(fs,line,array,index);

			NodeId lo,hi;
			lo=get_node(fs,node.y).x;
			hi=get_node(fs,node.y).y;
			if (lo==NO_NODE) lo=node_integer(fs,line,0);
			if (hi==NO_NODE) hi=node_call_metafield(fs,line,array,0,"length");

			begin_range_loop(fs,line,index,lo,hi);
			value_reg=emit_load(fs,value);

			/* todo: all these should be nodes instead */
			get_block(fs,block)->loop.array_register=array_reg;
			get_block(fs,block)->loop.value_register=value_reg;
			return value;
		}

		default: ;
	}
	return x;
}


void emit_field_initer(FileState *fs, Source line, int reg, NodeId id) {
	Node node;
	int mem,xx,xy,yy;

	node=get_node(fs,id);
	mem=get_mem_state(fs);
	switch (node.k) {
		case NODE_STORE: {
			Node x = get_node(fs,node.x);
			if ((x.kind == NODE_LOCAL)) {
				NO_CODE;
			} else if ((x.kind == NODE_FIELD) || (x.k == NODE_INDEX)) {
				xx=reg;
				xy=emit_load(fs,x.y);
				yy=emit_load(fs,node.y);
				if (x.kind==NODE_FIELD) {
					emit_bytexyz(fs,line,BC_SETFIELD,xx,xy,yy);
				} else {
					emit_bytexyz(fs,line,BC_SETINDEX,xx,xy,yy);
				}
			} else NO_CODE;
		} break;
		default: NO_CODE;
	}
	set_mem_state(fs,mem);
}


/* emits code when the node is not already a register to
load that node into any register */
int emit_load(FileState *fs, NodeId id) {
	int reg;
	reg=get_node_register(fs,NODE(id));
	if (reg==NO_SLOT) {
		reg=emit_eval(fs,0,-1,1,id);
		ASSERT(reg!=-1);
	}
	return reg;
}


static int find_local_const_store(FileState *fs, int reg) {
	if (fs->nloops > 0) {
		return -1;
	}
	elf_Bytecode byte, *bytes;
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


void opt_const_fold(FileState *fs, Node node) {
	#define isconst(k) (k==NODE_STRING||k==NODE_INTEGER||k==NODE_NUMBER)
	Node xx,yy;
	int xconst;
	int yconst;
	xx=get_node(fs,node.x);
	yy=get_node(fs,node.y);
	xconst = isconst(xx.k) || ((xx.k==NODE_LOCAL) && (find_local_const_store(fs,xx.x) != -1));
	yconst = isconst(yy.k) || ((yy.k==NODE_LOCAL) && (find_local_const_store(fs,yy.x) != -1));

	if (xconst && yconst) {
		file_dialog(fs,node.line,"possible constant fold");
	}
}


/* if no register is given, a new one is allocated,
registers are allocated in depth first order, which
results in minimal register usage.
This function will always allocate a new register,
even if the node is a local, in which case it will
emit a reload instruction. */
int emit_eval(FileState *fs, int flags, int reg, int nreg, NodeId id) {
	elf_Module *M;
	Node node;
	Source line;
	int mem, rx,ry;

	M=fs->M;
	node=get_node(fs,id);
	line=node.line;

	switch (node.kind) {
		case NODE_LOCAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);
			int src;
			src=get_node_register(fs,NODE(id));
			if (src<0||src>=get_mem_state(fs)) {
				file_dialog(fs,line,"invalid memory state!");
			}
			ASSERT(src>=0&&src<get_mem_state(fs));
			emit_bytexy(fs,line,BC_RELOAD,reg,src);
		} break;
		case NODE_CLSVAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			emit_bytexy(fs,line,BC_GETCLOSED,reg,node.x);
		} break;
		case NODE_GLOBAL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			emit_bytexy(fs,line,BC_GETGLOBAL,reg,node.x);
		} break;
		case NODE_NIL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			emit_bytexy(fs,line,BC_LOADNIL,reg,0);
		} break;
		case NODE_INTEGER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->integers,1);
			M->integers[yy] = node.lit.i;
			emit_bytexy(fs,line,BC_GETKINT,reg,yy);
		} break;
		case NODE_NUMBER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			/* todo: interning */
			int yy = ARRAY_GROW(M->numbers,1);
			M->numbers[yy] = node.lit.n;
			emit_bytexy(fs,line,BC_GETKNUM,reg,yy);
		} break;
		case NODE_STRING: {
			if (nreg<1) goto esc;
			if (reg<0) reg=reg_alloc(fs);

			/* todo: interning */
			int xx = elf_gsets(M,0,VSTR(elf_alloc_string(fs->R,node.lit.s)));
			emit_bytexy(fs,line,BC_GETGLOBAL,reg,xx);
		} break;
		case NODE_TABLE: {
			if (reg<0) reg=reg_alloc(fs);

			emit_bytexy(fs,line,BC_TABLE,reg,0);
			/* todo: turn this into sugar? we can't yet... */
			FOR_ARRAY(i,node.z) {
				emit_field_initer(fs,line,reg,node.z[i]);
			}
		} break;
		case NODE_METAFIELD:
		case NODE_FIELD: case NODE_INDEX: {
			if (nreg<1) goto esc;
			mem=get_mem_state(fs); {
				rx=emit_load(fs,node.x);
				ry=emit_load(fs,node.y);
			} set_mem_state(fs,mem);
			if (reg<0) reg=reg_alloc(fs);
			emit_bytexyz(fs,line,node2byte(node.kind),reg,rx,ry);
		} break;
		case NODE_CLOSURE: {
			if (nreg<1) goto esc;
			/* todo: why is it that we can't just start at reg */
			mem=get_mem_state(fs); {
				FOR_ARRAY(i,node.z) {
					emit_eval(fs,0,reg_alloc(fs),1,node.z[i]);
				}
			} set_mem_state(fs,mem);
			if (reg<0) reg=reg_alloc(fs);
			emit_bytexy(fs,line,BC_CLOSURE,mem,node.x);
			if (mem!=reg) {
				emit_bytexy(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		case NODE_TYPEGUARD: {
			if (nreg<1) goto esc;
			reg=emit_eval(fs,flags,reg,nreg,node.x);
			emit_bytexy(fs,node.line,BC_TYPEGUARD,reg,node2tag(node.y));
		} break;
		/* why is this even a thing dude */
		case NODE_GROUP: {
			reg=emit_eval(fs,flags,reg,nreg,node.x);
		} break;
		/* ({x}).{y}(...) will not be recognized as a meta-call
		because it's wrapped in parenthesis, could this be
		a feature */
		case NODE_CALL: {
			Node xx;
			int nargs;

			xx=get_node(fs,node.x);

			mem=get_mem_state(fs); {
				ASSERT(reg<mem);
				if ((xx.k==NODE_FIELD)||(xx.k==NODE_METAFIELD)) {
					ry=emit_eval(fs,0,-1,1,xx.y);
					rx=emit_eval(fs,0,-1,1,xx.x);
					emit_bytexyz(fs,line,node2byte(xx.k),ry,rx,ry);
					ASSERT(ry==mem);
				} else {
					ry=emit_eval(fs,0,-1,1,node.x);
					rx=emit_eval(fs,0,-1,1,node_local(fs,line,0));
					ASSERT(ry==mem);
				}
				FOR_ARRAY(i,node.z) {
					emit_eval(fs,0,-1,1,node.z[i]);
				}
			} set_mem_state(fs,mem);

			nargs=ARRAY_LENGTH(node.z)+1;
			emit_bytexyz(fs,line,BC_CALL,mem,nargs,nreg);
			if (nreg<1) goto esc;
			if (nreg>1) file_dialog(fs,line,"unsupported");
			if (reg<0) reg=reg_alloc(fs);
			if (reg!=mem) {
				emit_bytexy(fs,line,BC_RELOAD,reg,mem);
			}
		} break;
		/* (a !! b) = (a == nil ? a : b) */
		case NODE_NIL_AND: {
			BooleanJumps e = {0};
			Instr *js;

			if (reg<0)reg=reg_alloc(fs);
			emit_eval(fs,0,reg,1,node.x);
			js=emit_jump_if_nil(fs,NO_LINE,&e,node_local(fs,NO_LINE,reg));
			emit_eval(fs,0,reg,1,node.y);
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_NIL_OR: {
			BooleanJumps e={0};
			Instr *js;

			if (reg<0)reg=reg_alloc(fs);
			emit_eval(fs,0,reg,1,node.x);
			js=emit_jump_if_not_nil(fs,NO_LINE,&e,node_local(fs,get_node_line(fs,node.x),reg));
			emit_eval(fs,0,reg,1,node.y);
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_AND: case NODE_OR: {
			BooleanJumps e={0};
			Instr *js;

			if (reg<0)reg=reg_alloc(fs);
			emit_eval(fs,0,reg,1,node_integer(fs,line,0));
			js=emit_jump_if_false(fs,&e,id);
			emit_eval(fs,0,reg,1,node_integer(fs,line,1));
			patch_jumps(fs,js);
			ARRAY_DELETE(js);
		} break;
		case NODE_EQ: case NODE_NEQ:
		case NODE_GT: case NODE_GTEQ: case NODE_LT: case NODE_LTEQ:
		case NODE_DIV: case NODE_MUL: case NODE_MOD:
		case NODE_SUB: case NODE_ADD: case NODE_POW:
		case NODE_BIT_SHL: case NODE_BIT_SHR:
		case NODE_BIT_XOR:
		case NODE_BIT_AND: case NODE_BIT_OR: {
			if ((node.kind==NODE_GT)||(node.kind==NODE_GTEQ)) {
				mem=get_mem_state(fs); {
					rx=emit_load(fs,node.y);
					ry=emit_load(fs,node.x);
				} set_mem_state(fs,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=reg_alloc(fs);
				emit_bytexyz(fs,node.line,node2byte(node.k^1),reg,rx,ry);
			} else {
				mem=get_mem_state(fs); {
					rx=emit_load(fs,node.x);
					ry=emit_load(fs,node.y);
				} set_mem_state(fs,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=reg_alloc(fs);
				emit_bytexyz(fs,node.line,node2byte(node.k),reg,rx,ry);
			}
		} break;
		default: {
			file_dialog(fs,line,"invalid node (%s)",node2s[node.kind]);
			NO_CODE;
		}
	}
	esc:
	return reg;
}


/*
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
NodeId emit_preload(FileState *fs, NodeId x) {
	ASSERT(x>=0);

	Node   node;
	int     reg;

	node=get_target_node(fs,NODE(x));

	switch (node.kind) {
		case NODE_FIELD: {
			reg=emit_load(fs,node.x);
			x=node_field(fs,node.line,node_local(fs,node.line,reg),node.y);
		} break;
		case NODE_INDEX: {
			reg=emit_load(fs,node.x);
			x=node_index(fs,node.line,node_local(fs,node.line,reg),node.y);
		} break;
		default:;
	}

	return x;
}


void emit_store(FileState *fs, Source line, NodeId x, NodeId y) {
	Node node;
	node=get_target_node(fs,NODE(x));
	if (y<0) {
		file_dialog(fs,line,"invalid statement, expected a value for assignment");
	}
	if (!node_is_lvalue(node.kind)) {
		file_dialog(fs,line,"invalid assignment to (%s)",node2s[node.kind]);
		elf_fail(fs->R,0,"syntax error: invalid assignment");
	}
	if (node.kind==NODE_LOCAL) {
		EntityId id;
		id=find_local_entity(fs,node.x);
		/* todo: */
		if(id!=-1){
			if (fs->entities[id].flags & ENTITY_CONSTANT) {
				file_dialog(fs,line,"invalid assignment to constant entity");
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
		case NODE_GLOBAL: {
			ry=emit_load(fs,y);
			emit_bytexy(fs,line,BC_SETGLOBAL,rx,ry);
		} break;
		case NODE_LOCAL: {
			emit_eval(fs,0,rx,1,y);
		} break;
		ByteOP op;
		case NODE_INDEX: case NODE_FIELD: {
			rz=emit_load(fs,y);
			rx=emit_load(fs,node.x);
			ry=emit_load(fs,node.y);
			op=node.kind==NODE_INDEX?BC_SETINDEX:BC_SETFIELD;
			emit_bytexyz(fs,line,op,rx,ry,rz);
		} break;
		case NODE_CLSVAL: {
			file_dialog(fs,line,"assignment of closure value is not possible");
		} break;
		case NODE_METAFIELD: {
			file_dialog(fs,line,"assignment of metafields is not possible");
		} break;
		default: {
			NO_CODE;
		} break;
	}
}


void begin_if(FileState *fs, Source line, BranchJumps *s, NodeId x, int z) {
	BooleanJumps js = {0};
	emit_branch_if(fs,&js,z,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		ASSERT(js.f != 0);
		patch_jumps(fs,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		ASSERT(js.t != 0);
		patch_jumps(fs,js.f);
		ARRAY_DELETE(js.f);
		js.f = 0;
		s->jz = js.t;
	}
}


/*
** Closes previous conditional block by emitting
** escape jump, patches previous jz (jump if false)
** list to enter this block.
*/
void add_else_clause(FileState *fs, Source line, BranchJumps *s) {
	if (s->jz == 0) {
		file_dialog(fs,line,"invalid else clause");
	}
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,line,-1);
	ARRAY_ADD(s->j,j);

	patch_jumps(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void add_elif_clause(FileState *fs, Source line, BranchJumps *s, int x) {
	add_else_clause(fs,line,s);
	begin_if(fs,line,s,x,L_IF);
}


void add_then_clause(FileState *fs, Source line, BranchJumps *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	patch_jumps(fs,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void close_if(FileState *fs, Source line, BranchJumps *s) {
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


/* todo: add support for multiple results */
void emit_yield(FileState *fs, Source line, NodeId id) {
	/* todo: if we only return one value we don't have to reload */
	int mem,reg,nreg,j;
	mem=get_mem_state(fs);
	if (id!=NO_NODE) {
		/* todo: multi-returns */
		emit_eval(fs,0,reg=reg_alloc(fs),nreg=1,id);
		if (fs->fn->nyield < nreg) fs->fn->nyield = nreg;
		j=emit_bytexyz(fs,line,BC_YIELD,NO_JUMP,reg,nreg);
		ARRAY_ADD(fs->fn->yield_jumps,j);
	} else emit_bytex(fs,line,BC_LEAVE,0);
	set_mem_state(fs,mem);
	add_block_flags(fs,BLOCK_ENDED);
}


void begin_do_while_loop(FileState *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block
	ASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = get_instr_cursor(fs);
	bl->loop.false_jumps = 0;
	bl->loop.x = NO_NODE;
	bl->loop.index_register = NO_SLOT;
}


void close_do_while_loop(FileState *fs, Source line, NodeId x) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	BooleanJumps js = {0};
	emit_jump_if_true(fs,&js,x);

	patch_jumps2(fs,js.t,bl->loop.entry);
	ARRAY_DELETE(js.t);
	js.t = 0;

	patch_jumps2(fs,bl->loop.true_jumps,bl->loop.entry);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
}


void begin_while_loop(FileState *fs, NodeId x) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.x = x;

	bl->loop.entry = get_instr_cursor(fs);

	/* todo: this is temporary */
	emit_bytex(fs,NO_LINE,BC_LOOP,-1);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,x);
}


void close_while_loop(FileState *fs) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;

	/* todo: this is temporary */
	ASSERT(BC_OP(get_byte(fs,bl->loop.entry))==BC_LOOP);
	// fs->M->bytes[bl->loop.entry].x = get_instr_cursor(fs);

	emit_jump(fs,NO_LINE,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


void begin_range_loop(FileState *fs, Source line, NodeId index_node, NodeId lo, NodeId hi) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	ASSERT(index_node != NO_NODE);

	elf_StackId index_register = emit_load(fs,index_node);
	index_node = node_local(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	emit_eval(fs,0,index_register,1,node_type_guard(fs,get_node_line(fs,lo),lo,NT_INT));

	bl->loop.entry = get_instr_cursor(fs);

	elf_StackId hi_register = emit_load(fs,node_type_guard(fs,get_node_line(fs,hi),hi,NT_INT));
	hi = node_local(fs,line,hi_register);
	NodeId c = node_less_than(fs,line,index_node,hi);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,c);
}


void close_range_loop(FileState *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	// NodeId index_node = bl->loop.index_node;
	NodeId index_node = node_local(fs,NO_LINE,bl->loop.index_register);
	NodeId k = node_xy(fs,NO_LINE,NODE_ADD,NT_INT,index_node,node_integer(fs,NO_LINE,1));
	emit_store(fs,line,index_node,k);
	emit_jump(fs,line,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}




