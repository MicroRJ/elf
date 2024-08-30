// /*
// ** See Copyright Notice In elf.h
// ** ltest.c
// ** Test Tools
// */



// void sets(elf_Shell *c, elf_Table *table, char *k) {
// 	elf_tset(table,elSTR(elf_alloc_string(c,k)),* -- c->v);
// }


// int testlib_logging(elf_Shell *R) {
// 	elf_Int logging = elf_get_integer(R,0);
// 	R->call->caller->logging = logging;
// 	return 0;
// }


// int testlib_globallogging(elf_Shell *R) {
// 	elf_Int logging = elf_get_integer(R,0);
// 	R->bytelogging = logging;
// 	return 0;
// }


// int testlib_debugbreak(elf_Shell *R) {
// 	elf_debugger("this function is removed");
// 	return 0;
// }





// int testlib_disasm(elf_Shell *c) {
// 	elf_Module *md = c->md;
// 	elf_Closure *cl = elf_get_closure(c,0);
// 	elf_Function p = cl->fn;
// 	char file[BUFFER];
// 	clear_memory(file,sizeof(file));
// 	int j;
// 	for (j = 0; j < p.nbytes; ++j) {
// 		if (j != 0) strcatf(file,"\n");
// 		elf_Bytecode b = md->bytes[p.bytes+j];
// 		switch (b.k) {
// 			case BC_METACALL:
// 			case BC_CALL: {
// 				strcatf(file,"%s(%i,%i)", get_byte_label(b.k), b.x,b.y);
// 			} break;
// 			default: {
// 				strcatf(file,"%s(%lli)", get_byte_label(b.k), b.i);
// 			} break;
// 		}
// 	}
// 	elf_new_string(c,file);
// 	return 1;
// }


// int testlib_absslot(elf_Shell *c) {
// 	elf_StackId slot = elf_get_integer(c,0);
// 	PUSHV(c,c->s[slot]);
// 	return 1;
// }


// int testlib_absslotid(elf_Shell *c) {
// 	elf_put_integer(c,c->v-c->s);
// 	return 1;
// }


// int testlib_gcpause(elf_Shell *c) {
// 	elf_gcpause(c);
// 	return 0;
// }


// int testlib_gcunpause(elf_Shell *c) {
// 	elf_gcresume(c);
// 	return 0;
// }


// int testlib_gc(elf_Shell *c) {
// 	elf_trigger_collection_cycle(c);
// 	return 0;
// }


// int _gidof(elf_Module *fs, elf_Object *j) {
// 	FOR_ARRAY(fs->g->v) {
// 		if (fs->g->v[i].j == j) {
// 			return i;
// 		}
// 	}
// 	return -1;
// }


// int _gtable(elf_Shell *c) {
// 	elf_new_table(c,c->md->g);
// 	return 1;
// }


// char *gccolor2s(elf_GCColor c) {
// 	return
// 	c == elf_GC_BLACK ? "black" :
// 	c == elf_GC_WHITE ? "white" :
// 	c == elf_GC_PINK  ? "pink"  :
// 	c == elf_GC_RED   ? "red"   : "unknown";
// }


// void tstlib_load(elf_Shell *rt) {
// 	elf_Module *md = rt->md;
// 	elf_gset(md,elf_new_string(rt,"__gc"),elCFN(testlib_gc));
// 	elf_gset(md,elf_new_string(rt,"__gcpause"),elCFN(testlib_gcpause));
// 	elf_gset(md,elf_new_string(rt,"__gcunpause"),elCFN(testlib_gcunpause));
// 	elf_gset(md,elf_new_string(rt,"__disasm"),elCFN(testlib_disasm));
// 	elf_gset(md,elf_new_string(rt,"__logging"),elCFN(testlib_logging));
// 	elf_gset(md,elf_new_string(rt,"__globallogging"),elCFN(testlib_globallogging));

// 	elf_gset(md,elf_new_string(rt,"__debugbreak"),elCFN(testlib_debugbreak));
// 	elf_gset(md,elf_new_string(rt,"absslotid"),elCFN(testlib_absslotid));
// 	elf_gset(md,elf_new_string(rt,"absslot"),elCFN(testlib_absslot));
// 	elf_gset(md,elf_new_string(rt,"_gtable"),elCFN(_gtable));
// }