// /*
// ** See Copyright Notice In elf.h
// ** ltest.c
// ** Test Tools
// */



// void sets(elState *c, elTable *table, char *k) {
// 	elf_tabset(table,elf_valstr(elf_newstr(c,k)),* -- c->v);
// }


// int testlib_logging(elState *R) {
// 	elInteger logging = elf_getint(R,0);
// 	R->call->caller->logging = logging;
// 	return 0;
// }


// int testlib_globallogging(elState *R) {
// 	elInteger logging = elf_getint(R,0);
// 	R->bytelogging = logging;
// 	return 0;
// }


// int testlib_debugbreak(elState *R) {
// 	elf_debugger("this function is removed");
// 	return 0;
// }





// int testlib_disasm(elState *c) {
// 	elModule *md = c->md;
// 	elClosure *cl = elf_getcls(c,0);
// 	elProto p = cl->fn;
// 	char file[BUFFER];
// 	elf_clear_memory(file,sizeof(file));
// 	int j;
// 	for (j = 0; j < p.nbytes; ++j) {
// 		if (j != 0) strcatf(file,"\n");
// 		elBytecode b = md->bytes[p.bytes+j];
// 		switch (b.k) {
// 			case BC_METACALL:
// 			case BC_CALL: {
// 				strcatf(file,"%s(%i,%i)", elf_get_byte_label(b.k), b.x,b.y);
// 			} break;
// 			default: {
// 				strcatf(file,"%s(%lli)", elf_get_byte_label(b.k), b.i);
// 			} break;
// 		}
// 	}
// 	elf_pushnewstr(c,file);
// 	return 1;
// }


// int testlib_absslot(elState *c) {
// 	elRegId slot = elf_getint(c,0);
// 	elf_pushany(c,c->s[slot]);
// 	return 1;
// }


// int testlib_absslotid(elState *c) {
// 	elf_pushint(c,c->v-c->s);
// 	return 1;
// }


// int testlib_gcpause(elState *c) {
// 	elf_gcpause(c);
// 	return 0;
// }


// int testlib_gcunpause(elState *c) {
// 	elf_gcresume(c);
// 	return 0;
// }


// int testlib_gc(elState *c) {
// 	elf_collect(c);
// 	return 0;
// }


// int _gidof(elModule *fs, elObject *j) {
// 	elf_xarray_foreachi(fs->g->v) {
// 		if (fs->g->v[i].j == j) {
// 			return i;
// 		}
// 	}
// 	return -1;
// }


// int _gtable(elState *c) {
// 	elf_pushtab(c,c->md->g);
// 	return 1;
// }


// char *gccolor2s(elGCColor c) {
// 	return
// 	c == GC_BLACK ? "black" :
// 	c == GC_WHITE ? "white" :
// 	c == GC_PINK  ? "pink"  :
// 	c == GC_RED   ? "red"   : "unknown";
// }


// void tstlib_load(elState *rt) {
// 	elModule *md = rt->md;
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__gc"),elf_valbid(testlib_gc));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__gcpause"),elf_valbid(testlib_gcpause));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__gcunpause"),elf_valbid(testlib_gcunpause));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__disasm"),elf_valbid(testlib_disasm));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__logging"),elf_valbid(testlib_logging));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"__globallogging"),elf_valbid(testlib_globallogging));

// 	elf_add_global_value(md,elf_pushnewstr(rt,"__debugbreak"),elf_valbid(testlib_debugbreak));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"absslotid"),elf_valbid(testlib_absslotid));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"absslot"),elf_valbid(testlib_absslot));
// 	elf_add_global_value(md,elf_pushnewstr(rt,"_gtable"),elf_valbid(_gtable));
// }