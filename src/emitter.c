static int emit_byte(Parser *C, Source line, Bytecode byte) {
	BC_Module *M = C->R->M;
	ARRAY_ADD(M->lines,line);
	ARRAY_ADD(M->bytes,byte);
	ARRAY_ADD(M->track,0);
	// fpf_byte(stdout,M,-1,M->nbytes-C->fn->bytes,byte);
	return M->nbytes ++;
}


static int emit_bytex(Parser *C, Source line, int k, int x) {
	Bytecode byte=BC_XXX(k,x);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	return emit_byte(C,line,byte);
}


static int emit_bytexy(Parser *C, Source line, int k, int x, int y) {
	Bytecode byte=BC_XYY(k,x,y);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	return emit_byte(C,line,byte);
}


static int emit_bytexyz(Parser *C, Source line, int k, int x, int y, int z) {
	Bytecode byte=BC_XYZ(k,x,y,z);
	ASSERT(BC_OP(byte)==k);
	ASSERT(BC_ARGX(byte)==x);
	ASSERT(BC_ARGY(byte)==y);
	ASSERT(BC_ARGZ(byte)==z);
	return emit_byte(C,line,byte);
}

static void patch_jump2(Parser *fs, int src, int dst) {
	Bytecode byte,*bytes;
	bytes=fs->R->M->bytes;
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
	patch_jump2(fs,i,fs->R->M->nbytes);
}


static void patch_jumps(Parser *fs, Instr *js) {
	FOR_ARRAY(i,js) {
		patch_jump(fs,js[i]);
	}
}