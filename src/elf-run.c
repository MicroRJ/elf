/*
** See Copyright Notice In elf.h
** elf-run.c
** Runtime
*/


void elf_runini(elf_State *R, elf_Module *M) {
	R->M = M;
	R->bytelogging = lfalse;
	R->stklen = 4096;
	R->stk = R->top = elf_clearalloc(lHEAP,sizeof(elf_Value)*R->stklen);
	R->metatab_str = elf_newstrmetatab(R);
	R->metatab_tab = elf_newtabmetatab(R);
	elf_CallFrame Y = {0};
	Y.base = R->top;
	R->frame = &Y;
	M->globals = elf_pushnewtab(R);
	R->cache.x = elf_pushnewstr(R,"x");
	R->cache.y = elf_pushnewstr(R,"y");
	R->cache.z = elf_pushnewstr(R,"z");
	R->cache.w = elf_pushnewstr(R,"w");
	R->cache.width = elf_pushnewstr(R,"width");
	R->cache.height = elf_pushnewstr(R,"height");
	R->cache.__getfield = elf_pushnewstr(R,"__getfield");
	R->cache.__setfield = elf_pushnewstr(R,"__setfield");
	R->cache.__add = elf_pushnewstr(R,"__add");
	R->cache.__sub = elf_pushnewstr(R,"__sub");
	R->cache.__mul = elf_pushnewstr(R,"__mul");
	R->cache.__div = elf_pushnewstr(R,"__div");
	R->cache.__add1 = elf_pushnewstr(R,"__add1");
	R->cache.__sub1 = elf_pushnewstr(R,"__sub1");
	R->cache.__mul1 = elf_pushnewstr(R,"__mul1");
	R->cache.__div1 = elf_pushnewstr(R,"__div1");

	#if !defined(ELF_NOLIBS)
	/* todo: eventually we'll load these from the
	source code, each lib will be built independently,
	or maybe we can add a flag so that we don't load
	these libraries. */
	netlib_load(R);
	tstlib_load(R);
	crtlib_load(R);
	elflib_load(R);
	#endif
}



int elf_callfn(elf_State *R, elf_localid rxy, int nx, int ny) {
	return elf_callex(R,lnil,rxy,rxy,nx,ny);
}


/* todo: this should be different, rx should be the destination
registers, and ry the input registers */
int elf_callexx(elf_State *R, elf_Object *obj, elf_Value fn, elf_localid rx, elf_localid ry, int nx, int ny) {
	elf_CallFrame *caller = R->call;
	// elf_ensure(R->top - caller->locals+caller->cl->fn.nlocals > -1);
	elf_Value *locals = caller->locals + rx;
	/* top always points to one past locals,
	so far we only have nx argument locals,
	top is later incremented to match nlocals */
	elf_Value *top  = locals + nx;
	elf_CallFrame call = {0};
	call.caller = caller;
	call.top = R->top;
	call.obj = obj;
	call.locals = locals;
	call.rx = rx; call.ry = ry;
	call.nx = nx; call.ny = ny;
	if (fn.tag == TAG_CLS) {
		call.cl = fn.f;
		/* increment top to fill the function's locals */
		/* todo: replace with faster memset? */
		for (; nx < call.cl->fn.nlocals; ++ nx) {
			top->tag = TAG_NIL;
			top->i   = 0;
			top ++;
		}
	}
	R->top = top;
	R->call = &call;
	elf_localid nyield = 0;
	if (fn.tag == TAG_CLS) {
		nyield = elf_run(R);
	} else
	if (fn.tag == TAG_BID) {
		if (fn.c != lnil) {
			nyield = fn.c(R);
			/* ensure that the results were pushed to the stack */
			elf_ensure(nyield <= (R->top - call.locals));
			/* todo: we only do this to not have to make
			the user write to rx directly? */
			/* hoist results */
			for (int p = 0; p < MIN(nyield,ny); ++ p) {
				caller->locals[ry] = R->top[p-nyield];
			}
		}
	} else {
		nyield = -1;
		elf_throw(R,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[fn.tag]));
	}
	/* finally restore stack */
	R->call = caller;
	R->top = call.top;
	return nyield;
}


int elf_callex(elf_State *R, elf_Object *obj, elf_localid rx, elf_localid ry, int nx, int ny) {
	elf_CallFrame *caller = R->call;
	return elf_callexx(R,obj,caller->locals[rx],rx+1,ry,nx,ny);
}


int elf_loadexprfs(elf_State *R, elf_FileState *fs, elf_String *filename, elf_localid rxy, int ny, char *contents) {
	elf_Module *M = R->M;
	fs->R = R;
	fs->M = M;
	fs->bytes = M->nbytes;
	fs->filename = filename->c;
	fs->contents = contents;
	fs->thischar = contents;
	fs->linechar = contents;
	fs->linenumber = 1;

	/* kick start by lexing the first two tokens */
	elf_lexone(fs);
	elf_lexone(fs);

	elf_FileFunc fn = {0};
	elf_beginfsfn(fs,&fn,fs->tk.line);
	elf_nodeid id = elf_fsloadexpr(fs);
	elf_emityield(fs,fs->tk.line,id);
	elf_closefsfn(fs);

	elf_File file = {0};
	file.bytes = fn.bytes;
	file.nbytes = M->nbytes - fn.bytes;
	file.lines = contents;
	file.nlines = strlen(contents);
	elf_varadd(M->files,file);

	elf_Proto p = {0};
	p.nlocals = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes = M->nbytes - fn.bytes;

	return elf_callexx(R,lnil,elf_valcls(elf_newcls(R,p)),rxy,rxy,0,ny);
}



int elf_loadcodefs(elf_State *R, elf_FileState *fs, elf_String *filename, elf_localid rxy, int ny, char *contents) {
	if (filename == lnil) return -1;
	if (contents == lnil) return -1;

	elf_Module *M = R->M;
	fs->R = R;
	fs->M = M;
	fs->bytes = M->nbytes;
	fs->filename = filename->c;
	fs->contents = contents;
	fs->thischar = contents;
	fs->linechar = contents;
	fs->linenumber = 1;

	/* kick start by lexing the first two tokens */
	elf_lexone(fs); elf_lexone(fs);
	elf_FileFunc fn = {0};
	elf_beginfsfn(fs,&fn,fs->tk.line);
	while (!elf_testtk(fs,0)) elf_fsloadstat(fs);
	elf_closefsfn(fs);
	/* todo: this is temporary, please remove this or make
	some sort of object out of it... */
	elf_File fl = {0};
	fl.bytes = fn.bytes;
	fl.nbytes = M->nbytes - fn.bytes;
	fl.name = filename->c;
	fl.lines = contents;
	fl.nlines = strlen(contents);
	fl.pathondisk = filename->c;
	elf_varadd(M->files,fl);

	elf_Proto p = {0};
	p.nlocals = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes = M->nbytes - fn.bytes;

	elf_Value cls = elf_valcls(elf_newcls(R,p));
	return elf_callexx(R,lnil,cls,rxy,rxy,0,ny);
}


int elf_loadfilefs(elf_State *R, elf_FileState *fs, elf_String *name, elf_localid x, int y) {
	char *contents;
	Error error = sys_loadfilebytes(lHEAP,(void**)&contents,name->c);
	if (LFAILED(error)) {
		elf_logerror("'%s': could not load file",name->c);
		return -1;
	}
	return elf_loadcodefs(R,fs,name,x,y,contents);
}


int elf_loadcode(elf_State *R, elf_String *filename, elf_localid rxy, int ny, char *contents) {
	elf_FileState fs = {0};
	return elf_loadcodefs(R,&fs,filename,rxy,ny,contents);
}


int elf_loadexpr(elf_State *R, elf_String *filename, elf_localid rxy, int ny, char *contents) {
	elf_FileState fs = {0};
	return elf_loadexprfs(R,&fs,filename,rxy,ny,contents);
}


int elf_loadfile(elf_State *R, elf_String *filename, elf_localid rxy, int ny) {
	elf_FileState fs = {0};
	return elf_loadfilefs(R,&fs,filename,rxy,ny);
}


/* todo: Can we add a failsafe system that attempts
to recover from failed instructions?
So any instructions that depend on a previous
one are skipped?
For instance, table:add(table:length()), here if
table is nil or not even a table, you have to skip
the call instruction and its arguments. */
int elf_run(elf_State *R) {
	/* todo: these names are deprecated */
	elf_CallFrame *c = R->f;
	elf_Module *md = R->md;
	//
	elf_Module *M = R->M;
	elf_CallFrame *call = R->call;
	elf_Closure *cl = call->cl;
	elf_Proto fn = cl->fn;
	elf_CallFrame *caller = call->caller;
	elf_Value *locals = call->locals;
	elf_ensure((elf_int)(R->top - locals) >= fn.nlocals);

	while (call->j < fn.nbytes) {
		elf_int jp = call->j ++;
		elf_int bc = fn.bytes + jp;
		R->byte = bc;
		elf_Bytecode b = md->bytes[bc];
		elf_Bytecode byte = b;
#if defined(_DEBUG)
		if (R->bytelogging || call->logging) elf_bytefpf(stdout,md,-1,jp,b);
		if (R->debuggerflag) elf_debugger("elf-run: debugger break");

		if (R->bytetracking) {
			elf_int track = ++ M->track[bc];
			if (track == 64) {
				elf_File file = M->files[elf_fndfilebybyte(M,bc)];
				elf_lineid line = M->lines[bc];
				int linenum;
				elf_getlinelocinfo(file.lines,line,&linenum,0);
				elf_logdebug("%s %i: %lli: %lli detected hot path",file.name,linenum,bc,track);
			}
		}
#endif


		switch (b.k) {
	case BC_LEAVE: {
		if (call->dl != lnil) {
			call-> j = call->dl->j;
			call->dl = call->dl->n;
		} else goto leave;
	} break;
	case BC_DELAY: {
		/* todo: can we make this better */
		elf_delaylist *dl = elf_alloc(lHEAP,sizeof(elf_delaylist));
		dl->n = c->dl;
		dl->j = c->j;
		c->dl = dl;

		elf_ensure(b.i >= 0);
		c->j = jp + b.i;
	} break;
	case BC_YIELD: {
		elf_ensure(b.x >= 0);
		/* check that we don't exceed number of
		expected outputs */
		int ny = MIN(b.z,call->ny);
		for (elf_localid y = 0; y < ny; ++y) {
			caller->locals[call->ry+y] = locals[b.y+y];
		}
		call->ny = ny;
		call->j = jp + b.x;
	} break;
	case BC_STKGET: {
		elf_tycheck(R,bc,0,TAG_INT,locals[b.y].tag);
		locals[b.x] = locals[locals[b.y].i];
	} break;
	case BC_STKLEN: {
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = R->top - locals;
	} break;
	case BC_LOADFILE: {
		elf_String *fname = elf_getstr(R,b.x);
		if (fname == lnil) elf_throw(R,bc,"'load': attempted to call load with nil");
		elf_loadfile(R,fname,b.x,b.y);
	} break;
	case BC_J: {
		c->j = jp + b.i;
	} break;
	case BC_JZ: {
		if (locals[b.y].i == 0) call->j = jp + b.x;
	} break;
	case BC_JNZ: {
		if (locals[b.y].i != 0) call->j = jp + b.x;
	} break;
	case BC_LOADTHIS: {
		locals[b.x].tag = elf_objtotag(call->obj->type);
		locals[b.x].x_obj = call->obj;
	} break;
	case BC_RELOAD: {
		locals[b.x] = locals[b.y];
	} break;
	case BC_LOADGLOBAL: {
		locals[b.x] = md->g->v[b.y];
	} break;
	case BC_SETGLOBAL: {
		md->g->v[b.x] = call->base[b.y];
	} break;
	case BC_LOADNIL: {
		locals[b.x].tag = TAG_NIL;
		locals[b.x].i   = 0;
	} break;
	case BC_LOADINT: {
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = md->ki[b.y];
	} break;
	case BC_LOADNUM: {
		locals[b.x].tag = TAG_NUM;
		locals[b.x].n   = md->kn[b.y];
	} break;
	case BC_LOADCACHED: {
		elf_ensure(b.y >= 0 && b.y < fn.ncaches);
		locals[b.x] = cl->caches[b.y];
	} break;
	case BC_CLOSURE: {
		elf_ensure(b.y >= 0 && b.y < elf_varlen(md->p));
		elf_Proto p = md->p[b.y];
		elf_Closure *ncl = elf_newcls(R,p);
		for (int i = 0; i < p.ncaches; ++i) {
			ncl->caches[i] = locals[b.x+i];
		}
		locals[b.x].tag = TAG_CLS;
		locals[b.x].f   = ncl;
		// ncl->obj.gccolor = GC_WHITE;
	} break;
	case BC_TABLE: {
		/* ensure objects are created, then the
		local is renamed atomically, otherwise
		gc could trigger in between think the
		local is some other type */
		elf_Table *tab = elf_newtab(R);
		locals[b.x].tag = TAG_TAB;
		locals[b.x].x_tab = tab;
	} break;
	case BC_TYPEGUARD: {
		elf_tycheck(R,bc,b.x,b.y,locals[b.x].tag);
	} break;
	case BC_INDEX: case BC_FIELD: {
		if (locals[b.y].tag == TAG_NIL) {
			elf_throw(R,bc,"attempted to get field of nil value");
		}
		elf_Value xx = locals[b.y];
		elf_Value yy = locals[b.z];
		if (xx.tag == TAG_TAB) {
			locals[b.x] = elf_tablookup(xx.x_tab,yy);
		} else if (xx.tag == TAG_OBJ) {
	elf_Value overload = elf_tabgetfld(xx.x_obj->metatable,R->cache.__getfield);
	if (overload.tag != TAG_NIL) {
		if (overload.tag == TAG_CLS || overload.tag == TAG_BID) {
			locals[b.x] = yy;
			int ny = elf_callexx(R,xx.x_obj,overload,b.x,b.x,1,1);
			if (ny < 1) {
				elf_throw(R,NO_BYTE,"__getfield operator must return atleast one value");
			}
		} else elf_throw(R,NO_BYTE,"__getfield operator must be a function");
	} else elf_throw(R,NO_BYTE,"__getfield operator is not implemented for this object");
		} else if (xx.tag == TAG_STR) {
			elf_tycheck(R,bc,0,TAG_INT,yy.tag);
			locals[b.x].tag = TAG_INT;
			locals[b.x].i   = locals[b.y].s->c[locals[b.z].i];
		} else locals[b.x] = (elf_Value){TAG_NIL};
	} break;
	case BC_SETINDEX: case BC_SETFIELD: {
		elf_Value xx,yy,zz;
		xx = locals[b.x];
		yy = locals[b.y];
		zz = locals[b.z];
		if (xx.tag == TAG_TAB) {
			elf_tabset(xx.x_tab,yy,zz);

		} else if (xx.tag == TAG_OBJ) {

	elf_Value overload = elf_tabgetfld(xx.x_obj->metatable,R->cache.__setfield);
	if (overload.tag != TAG_NIL) {
		if (overload.tag == TAG_CLS || overload.tag == TAG_BID) {
			/* Here we use temporary stack space to put
			the arguments, this could be skipped if the
			arguments are already next to each other in
			order. */
			elf_Value *home = R->top;
			elf_localid base = home-locals;

			R->top += 2;
			home[0] = yy; home[1] = zz;

			elf_callexx(R,xx.x_obj,overload,base,b.x,2,0);

			R->top = home;

		} else elf_throw(R,NO_BYTE,"'__setfield': operator must be a function");

	} else elf_throw(R,NO_BYTE,"'__setfield': operator is not implemented for this object");

		} else elf_throw(R,NO_BYTE,elf_tpf("attempted to set field of '%s' value", tag2s[xx.tag]));

	} break;
	case BC_SETMETATABLE: {
		elf_Value xx = locals[b.x];
		elf_Value yy = locals[b.y];
		if (elf_tagisobj(xx.tag) && yy.tag == TAG_TAB) {
			xx.x_obj->metatable = yy.x_tab;
		} else elf_unreachable;
	} break;
	case BC_SETMETAFIELD: {
		elf_Value xx = locals[b.x];
		if (elf_tagisobj(xx.tag)) {
			elf_Value yy = locals[b.y];
			elf_Value zz = locals[b.z];
			elf_tabset(xx.x_obj->metatable,yy,zz);
		} else elf_throw(R,bc,elf_tpf("'%s': not an object", tag2s[xx.tag]));
	} break;
	case BC_METAFIELD: {
		elf_Value *yy = &locals[b.y];
		if (elf_tagisobj(yy->tag)) {
			locals[b.x] = elf_tablookup(yy->j->metatable,locals[b.z]);
		} else {
			locals[b.x] = (elf_Value){TAG_NIL};
			elf_throw(R,bc,elf_tpf("'%s': not an object", tag2s[yy->tag]));
		}
	} break;
	case BC_METACALL: {
		R->byte = bc;
		elf_callex(R,locals[b.x].x_obj,b.x+1,b.x,b.y,b.z);
		elf_ensure((elf_int)(R->top - locals) >= fn.nlocals);
	} break;
	case BC_CALL: {
		R->byte = bc;
		elf_callfn(R,b.x,b.y,b.z);
		elf_ensure((elf_int)(R->top - locals) >= fn.nlocals);
	} break;
	case BC_ISNIL: {
		elf_Value x = locals[b.y];
		elf_bool nan = x.tag != TAG_INT && x.tag != TAG_NUM;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = x.tag == TAG_NIL || (nan && x.i == 0);
	} break;
	case BC_EQ: case BC_NEQ: {
		elf_Value x = locals[b.y];
		elf_Value y = locals[b.z];
		elf_bool eq = lfalse;
		if ((x.tag == TAG_NIL) || (y.tag == TAG_NIL)) {
			eq = elf_valisnil(x) == elf_valisnil(y);
		} else if ((x.tag == TAG_STR) && (y.tag == TAG_STR)) {
			eq = elf_streq(x.x_str,y.x_str);
		} else if (elf_tagisnumeric(x.tag) && elf_tagisnumeric(y.tag)) {
			eq = x.x_int == y.x_int;
		} else eq = (x.tag == y.tag) && (x.x_int == y.x_int);

		if (b.k == BC_NEQ) eq = !eq;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = eq;
	} break;
	/* todo: make this better */
	#define CASE_IBOP(OPNAME,OP) \
	case OPNAME : {\
		locals[b.x].tag = TAG_INT;\
		locals[b.x].x_int = elf_toint(c->l[b.y]) OP elf_toint(c->l[b.z]);\
	} break
	/* FN1 is actually so silly */
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
	case OPCODE : {\
		elf_Value xx = locals[b.y];\
		elf_Value yy = locals[b.z];\
		if (elf_tagisobj(xx.tag) || elf_tagisobj(yy.tag)) {\
			if (!elf_tagisobj(xx.tag)) elf_throw(R,NO_BYTE,"invalid ordering, object type must come first");\
			elf_String *mfname = FN;\
			if (!elf_tagisobj(yy.tag)) mfname = FN1;\
			elf_Value mfield = elf_tabgetfld(xx.x_obj->metatable,mfname);\
			if (mfield.tag == TAG_CLS) {\
				locals[b.x] = yy;\
				int ny = elf_callexx(R,xx.x_obj,mfield,b.x,b.x,1,1);\
				if (ny < 1) elf_throw(R,bc,"function must return atleast one value");\
			} else elf_throw(R,bc,elf_tpf("'%s': overload is %s, not a function",mfname->c,tag2s[mfield.tag]));\
		} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {\
			if (!elf_tagisnumeric(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			locals[b.x].tag = TAG_NUM;\
			locals[b.x].x_num = elf_tonum(xx) OP elf_tonum(yy);\
		} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {\
			if (!elf_tagisnumeric(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			locals[b.x].tag = TAG_INT;\
			locals[b.x].x_int = elf_toint(xx) OP elf_toint(yy);\
		} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XSTRINGIFY(OP)));\
	} break
	case BC_LTEQ: {
		elf_Value xx = locals[b.y];
		elf_Value yy = locals[b.z];
		if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
			locals[b.x].tag = TAG_INT;
			locals[b.x].i   = elf_tonum(xx) <= elf_tonum(yy);
		} else {
			locals[b.x].tag = TAG_INT;
			locals[b.x].i   = elf_toint(xx) <= elf_toint(yy);
		}
	} break;
	case BC_LT: {
		elf_Value xx = locals[b.y];
		elf_Value yy = locals[b.z];
		if (xx.tag == TAG_NUM || yy.tag == TAG_NUM) {
			locals[b.x].i = elf_tonum(xx) < elf_tonum(yy);
		} else {
			locals[b.x].i = elf_toint(xx) < elf_toint(yy);
		}
		locals[b.x].tag = TAG_NUM;
	} break;
	CASE_IBOP(BC_SHL,  <<);
	CASE_IBOP(BC_SHR,  >>);
	CASE_IBOP(BC_XOR,   ^);
	CASE_IBOP(BC_MOD,   %);
	CASE_IBOP(BC_BITOR, |);
	/* todo: could we cache these strings */
	CASE_BOP(BC_ADD, +, R->cache.__add, R->cache.__add1);
	CASE_BOP(BC_SUB, -, R->cache.__sub, R->cache.__sub1);
	CASE_BOP(BC_MUL, *, R->cache.__mul, R->cache.__mul1);
	CASE_BOP(BC_DIV, /, R->cache.__div, R->cache.__div1);
	#undef CASE_BOP
	default: {
		elf_unreachable;
	} break;
		}
		elf_ensure((elf_int)(R->top - locals) >= fn.nlocals);
		int BREAKPOINT;
		BREAKPOINT = 0; (void) BREAKPOINT;
	}

	leave:
	return c->y;
}

