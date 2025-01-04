

static int emit_byte(Compiler *C, Source line, Bytecode byte) {
	BC_Module *M = C->R->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	ARRAY_ADD(M->track,0);
	// fpf_byte(stdout,M,-1,M->nbytes-C->fn->bytes,byte);
	return M->nbytes ++;
}


static int emit_bytex(Compiler *C, Source line, int k, int x) {
	Bytecode byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(C,line,byte);
}


static int emit_bytexy(Compiler *C, Source line, int k, int x, int y) {
	Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(C,line,byte);
}


static int emit_bytexyz(Compiler *C, Source line, int k, int x, int y, int z) {
	Bytecode byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(C,line,byte);
}

static void patch_jump2(Parser *fs, int src, int dst) {
	Bytecode byte,*bytes;
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


static void patch_jumps2(Parser *fs, Instr *js, Instr j) {
	FOR_ARRAY(i,js) {
		patch_jump2(fs,js[i],j);
	}
}


static void patch_jump(Parser *fs, Instr i) {
	patch_jump2(fs,i,fs->M->nbytes);
}


static void patch_jumps(Parser *fs, Instr *js) {
	FOR_ARRAY(i,js) {
		patch_jump(fs,js[i]);
	}
}


static Instr emit_jump(Parser *fs, Source line, Instr j) {
	__debugbreak();
	// xx return emit_bytex_deprecated(fs,line,BC_J,j-fs->M->nbytes);
}



int emit_branch_if(Parser *fs, BooleanJumps *js, elf_Bool if_true, IR_Id id) {
	IR_Node node = get_ir(fs,id);

	int mem,reg,jmp;
	switch (node.kind) {
		case IR_AND: {
			emit_jump_if_false(fs,js,node.x);
			jmp=emit_branch_if(fs,js,if_true,node.y);
		} break;
		case IR_OR: {
			emit_jump_if_true(fs,js,node.x);
			jmp=emit_branch_if(fs,js,if_true,node.y);
		} break;
		default: {
			ASSERT(!"FIXME");
			// xxx mem=get_mem_state_deprecated(fs);
			// xxx reg=any_reg_deprecated(fs,id);
			// xxx set_mem_state_deprecated(fs,mem);

			if (if_true) {
				// xxx jmp=emit_bytexy_deprecated(fs,node.line,BC_JNZ,NO_JUMP,reg);
				ARRAY_ADD(js->t,jmp);
			} else {
				// xxx jmp=emit_bytexy_deprecated(fs,node.line,BC_JZ,NO_JUMP,reg);
				ARRAY_ADD(js->f,jmp);
			}
		} break;
	}

	return jmp;
}


int emit_branch_if_false(Parser *fs, BooleanJumps *js, IR_Id id) {
	return emit_branch_if(fs,js,0,id);
}


int emit_branch_if_true(Parser *fs, BooleanJumps *js, IR_Id id) {
	return emit_branch_if(fs,js,1,id);
}


/* similar to branch if true, but additionally all
false jumps converge here */
int *emit_jump_if_true(Parser *fs, BooleanJumps *js, IR_Id id) {
	emit_branch_if_true(fs,js,id);
	patch_jumps(fs,js->f);
	ARRAY_DELETE(js->f);
	js->f = 0;
	return js->t;
}


Instr *emit_jump_if_false(Parser *fs, BooleanJumps *js, IR_Id id) {
	emit_branch_if_false(fs,js,id);
	patch_jumps(fs,js->t);
	ARRAY_DELETE(js->t);
	js->t = 0;
	return js->f;
}


int *emit_jump_if_not_nil(Parser *fs, Source line, BooleanJumps *js, IR_Id id) {
	return emit_jump_if_false(fs,js,node_xy(fs,line,IR_EQ,NT_BOL,id,node_nil(fs,line)));
}


int *emit_jump_if_nil(Parser *fs, Source line, BooleanJumps *js, IR_Id id) {
	return emit_jump_if_true(fs,js,node_xy(fs,line,IR_EQ,NT_BOL,id,node_nil(fs,line)));
}