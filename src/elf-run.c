/*
** See Copyright Notice In elf.h
** elf-run.c
** Runtime
*/


void elf_runini(elState *R, elModule *M) {
	#if 0
	_logging_io = fopen("elf.logs.txt","wb");
	#endif

	R->M = M;
	R->bytelogging = false;
	R->stklen = 4096;
	R->stk = R->top = elf_clear_alloc(lHEAP,sizeof(elValue)*R->stklen);
	R->metatables.string = elf_new_string_metatable(R);
	R->metatables.table = elf_new_table_metatable(R);
	R->call_level = 0;

	//TODO: So when you call a function,
	//using 0 for rx, 0 is relative to locals,
	//and it will override values that have
	//been pushed... I added a temporary fix...
	R->root_call = (elCallState){0};
	R->root_call.locals = R->top;
	R->call = &R->root_call;

	M->globals = elf_add_new_table(R);
	R->cache.x = elf_add_new_string(R,"x");
	R->cache.y = elf_add_new_string(R,"y");
	R->cache.z = elf_add_new_string(R,"z");
	R->cache.w = elf_add_new_string(R,"w");
	R->cache.width = elf_add_new_string(R,"width");
	R->cache.height = elf_add_new_string(R,"height");
	R->cache.__getfield = elf_add_new_string(R,"__getfield");
	R->cache.__setfield = elf_add_new_string(R,"__setfield");
	R->cache.__add = elf_add_new_string(R,"__add");
	R->cache.__sub = elf_add_new_string(R,"__sub");
	R->cache.__mul = elf_add_new_string(R,"__mul");
	R->cache.__div = elf_add_new_string(R,"__div");
	R->cache.__add1 = elf_add_new_string(R,"__add1");
	R->cache.__sub1 = elf_add_new_string(R,"__sub1");
	R->cache.__mul1 = elf_add_new_string(R,"__mul1");
	R->cache.__div1 = elf_add_new_string(R,"__div1");
	R->cache.__hash = elf_add_new_string(R,"__hash");

	//TODO: temporary fix
	R->call->locals = R->top;
	#if !defined(ELF_NOLIBS)
	crtlib_load(R);
	netlib_load(R);
	elflib_loadall(R);
	#endif
	//TODO: temporary fix
	R->call->locals = R->top;
}


int elf_call_function(elState *R, elRegId rxy, int nx, int ny) {
	return elf_callex(R,elNil,rxy,rxy,nx,ny);
}


int elf_callexx(elState *R, elObject *obj, elValue fn, elRegId rx, elRegId ry, int nx, int ny) {
	elCallState *caller = R->call;
	// elf_ensure((R->top-caller->locals)+caller->cl->fn.zstack-1 > rx);
	elValue *locals = caller->locals + rx;
	/* top always points to one past locals,
	so far we only have nx argument locals,
	top is later incremented to match nlocals */
	elValue *top  = locals + nx;
	elCallState call = {0};
	call.caller = caller;

	call.head = R->byte;
	call.tail = 0;
	call.top = R->top;
	call.obj = obj;
	call.locals = locals;
	call.rx = rx; call.ry = ry;
	call.nx = nx; call.ny = ny;
	if (fn.tag == TAG_CLS) {
		call.cl = fn.f;
		/* increment top to fill the function's locals */
		/* todo: replace with faster memset? */
		for (; nx < call.cl->fn.zstack; ++ nx) {
			top->tag = TAG_NIL;
			top->i   = 0;
			top ++;
		}
	}
	R->top = top;
	R->call = &call;
	R->call_level ++;

	if (R->oncalldebuggerflag) {
		elf_debugger("on-call-debugger");
	}
	elRegId nyield = 0;
	if (fn.tag == TAG_CLS) {
		nyield = elf_run(R);
	} else
	if (fn.tag == TAG_BID) {
		if (fn.c != elNil) {
			nyield = fn.c(R);
			/* ensure that the results were pushed to the stack */
			elRegId nstack = R->top - call.locals;
			if (nstack < nyield) {
				elf_throw(R,NO_BYTE,elf_tpf("number of values on stack '%i', is incoherent with specified number of yielded values '%i'",nstack,nyield));
			}
			/* todo: we only do this to not have to make
			the user write to rx directly? */
			/* hoist results */
			for (int p = 0; p < MIN(nyield,ny); ++ p) {
				caller->locals[ry] = R->top[p-nyield];
			}
		} else elf_throw(R,NO_BYTE,"binding function is nil!");
	} else {
		nyield = -1;
		elf_throw(R,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[fn.tag]));
	}
	/* finally restore stack */
	R->call = caller;
	R->top = call.top;
	R->call_level --;
	elf_ensure(R->call_level > -1);
	return nyield;
}


int elf_callex(elState *R, elObject *obj, elRegId rx, elRegId ry, int nx, int ny) {
	elCallState *caller = R->call;
	return elf_callexx(R,obj,caller->locals[rx],rx+1,ry,nx,ny);
}


int elf_loadexprfs(elState *R, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents) {
	elModule *M = R->M;
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

	elFileFnState fn = {0};
	elf_emitter_begin_function(fs,&fn,fs->tk.line);
	elNodeId id = elf_load_file_expr(fs);
	elf_emit_yield(fs,fs->tk.line,id);
	elf_emitter_close_function(fs);

	elFileInfo file = {0};
	file.bytes = fn.bytes;
	file.nbytes = M->nbytes - fn.bytes;
	file.contents = contents;
	file.length = strlen(contents);
	elf_xarray_add(M->files,file);

	elProto p = {0};
	p.zstack = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes = M->nbytes - fn.bytes;

	return elf_callexx(R,elNil,elf_closure_value(elf_new_closure(R,p)),rxy,rxy,0,ny);
}


int elf_loadcodefs(elState *R, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents) {
	if (filename == elNil || contents == elNil) {
		return -1;
	}

	elModule *M = R->M;
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
	elFileFnState fn = {0};
	elf_emitter_begin_function(fs,&fn,fs->tk.line);
	while (!elf_test_token(fs,0)) elf_load_file_stat(fs);
	elf_emitter_close_function(fs);
	/* todo: this is temporary, please remove this or make
	some sort of object out of it... */
	elFileInfo fl = {0};
	fl.bytes = fn.bytes;
	fl.nbytes = M->nbytes - fn.bytes;
	fl.name = filename->c;
	fl.contents = contents;
	fl.length = strlen(contents);
	fl.pathondisk = filename->c;
	elf_xarray_add(M->files,fl);

	elProto p = {0};
	p.zstack = fn.nlocals;
	p.bytes = fn.bytes;
	p.nbytes = M->nbytes - fn.bytes;

	elValue cls = elf_closure_value(elf_new_closure(R,p));
	return elf_callexx(R,elNil,cls,rxy,rxy,0,ny);
}


int elf_loadfilefs(elState *R, elFileState *fs, elString *name, elRegId x, int y) {
	char *contents;
	elError error = sys_load_file_contents(lHEAP,(void**)&contents,name->c);
	if (elFAILED(error)) {
		elf_logerror("'%s': could not load file",name->c);
		return -1;
	}
	return elf_loadcodefs(R,fs,name,x,y,contents);
}


int elf_loadcode(elState *R, elString *filename, elRegId rxy, int ny, char *contents) {
	elFileState fs = {0};
	return elf_loadcodefs(R,&fs,filename,rxy,ny,contents);
}


int elf_loadexpr(elState *R, elString *filename, elRegId rxy, int ny, char *contents) {
	elFileState fs = {0};
	return elf_loadexprfs(R,&fs,filename,rxy,ny,contents);
}


int elf_loadfile(elState *R, elString *filename, elRegId rxy, int ny) {
	elFileState fs = {0};
	return elf_loadfilefs(R,&fs,filename,rxy,ny);
}


void elf_check_division_by_zero(elState *S, elValue xx, elValue yy) {
	if ((yy.tag == TAG_NUM) && (yy.n == 0.)) elf_throw(S,NO_BYTE,"division by zero");
	else if ((yy.tag == TAG_INT) && (yy.i == 0)) elf_throw(S,NO_BYTE,"integer division by zero");
}


int elf_call_overload(elState *S, elObject *obj, elString *name, elRegId io, elValue in) {
	elValue field = elf_table_get_field(obj->metatable,name);
	if (field.tag != TAG_CLS && field.tag != TAG_BID) {
		elf_throw(S,NO_BYTE,elf_tpf("'%s': overload is %s, not a function",name->c,tag2s[field.tag]));
	}
	/* So we do it this way, because when the call frame
	for a function is created at rx it could override
	other values past rx, so we always call a function
	at stack top which is always free.
	todo: the question is whether the bytecode generator
	should take this into account, because so far we've
	relied on the order of execution, so technically we
	only override values that haven't been set yet, and
	also, even for calling overloads, the order of
	execution still applies, and we should only override
	values that haven't been set yet! */
	elRegId base = S->top - S->call->locals;
	S->call->locals[base] = in;
	int ny = elf_callexx(S,obj,field,base,io,1,1);
	if (ny < 1) {
		elf_throw(S,NO_BYTE,"overload function must return at least one value");
	}
	return ny;
}


int elf_run(elState *R) {
	elModule *md = R->md;
	//
	elModule *M = R->M;
	elCallState *call = R->call;
	elClosure *cl = call->cl;
	elProto fn = cl->fn;
	elCallState *caller = call->caller;
	elValue *locals = call->locals;
	elf_ensure((elInteger)(R->top - locals) >= fn.zstack);

	while (call->tail < fn.nbytes) {
		/* todo: call->tail is redundant ... */
		elInteger jp = call->tail ++;
		elInteger bc = fn.bytes + jp;
		R->byte = bc;

		elBytecode b = md->bytes[bc];
		elBytecode byte = b;

#if defined(_DEBUG)
		if (R->bytelogging || call->logging) elf_bytefpf(stdout,md,-1,jp,b);
		if (R->debuggerflag) elf_debugger("elf-run: debugger break");
#endif

#if defined(ELF_EXPERIMENTAL_FEATURES)
		if (R->bytetracking) {
			elInteger track = ++ M->track[bc];
			if (track == 64) {
				elFileInfo file = M->files[elf_find_file_info_by_byte(M,bc)];
				elf_lineid line = M->lines[bc];
				int linenum;
				elf_get_line_location_info(file.lines,line,&linenum,0);
				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,bc,track);
			}
		}
#endif
		switch (b.k) {
	case BC_LEAVE: {
		if (call->delay_list != elNil) {
			call->tail = call->delay_list->j;
			call->delay_list = call->delay_list->n;
		} else goto leave;
	} break;
	case BC_DELAY: {
		/* todo: can we make this better */
		elf_delaylist *delay = elf_alloc(lHEAP,sizeof(elf_delaylist));
		delay->n = call->delay_list;
		delay->j = call->tail;
		call->delay_list = delay;

		elf_ensure(b.i >= 0);
		call->tail = jp + b.i;
	} break;
	case BC_YIELD: {
		elf_ensure(b.x >= 0);
		/* check that we don't exceed number of
		expected outputs */
		int ny = MIN(b.z,call->ny);
		for (elRegId y = 0; y < ny; ++y) {
			caller->locals[call->ry+y] = locals[b.y+y];
		}
		call->ny = ny;
		call->tail = jp + b.x;
	} break;
	// case BC_LOADFILE: {
	// 	elString *fname = elf_get_string(R,b.x);
	// 	if (fname == elNil) elf_throw(R,bc,"'load': attempted to call load with nil");
	// 	int result = elf_loadfile(R,fname,b.x,b.y);
	// 	if (result == -1) {
	// 		locals[b.x].tag = TAG_NIL;
	// 		locals[b.x].i   = 0;
	// 	}
	// } break;
	case BC_J: {
		call->tail = jp + b.i;
	} break;
	case BC_JZ: {
		if (locals[b.y].x_int == 0) call->tail = jp + b.x;
	} break;
	case BC_JNZ: {
		if (locals[b.y].x_int != 0) call->tail = jp + b.x;
	} break;
	case BC_LOADTHIS: {
		if (call->obj == elNil) {
			/* todo: we gotta rework this logic, first of all,
			not passing the object in the argument list makes
			it impossible to cache the meta-function because
			you have to follow the x:x() pattern to let the
			loader know you're calling a meta-function that takes
			the additional 'this' argument.
			The simplest way I can think of to allow for caching
			of meta-function and invoking them properly is to
			add a separate way to call a function which makes it
			so that it is possible to cache the meta-function and
			then call it.
			This involves creating new language semantics which
			would allow for further optimizations.

			let x = SOME_OBJECT
			let _fn = x:getmetatable():idxof("fn")
			for i in 0..10000 ? {
				x:(fn,1,2,3)
			// ^ this new syntax essentially allows you
			// to pass in the function that you'd like to
			// invoke on x

			literal VEC_X = 0
			literal VEC_Y = 0
			literal VEC_Z = 0
			x[VEC_X]
			*/
			elf_throw(R,NO_BYTE,"'this' is invalid for this function, not a meta-call");
		}
		locals[b.x].tag = elf_object_type_to_value_tag(call->obj->type);
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
	case BC_LOAD_CLOSURE_VALUE: {
		elf_ensure(b.y >= 0 && b.y < fn.zcache);
		locals[b.x] = cl->enclosure[b.y];
	} break;
	case BC_CLOSURE: {
		elf_ensure(b.y >= 0 && b.y < elf_xarray_length(md->prototypes));
		elProto proto = md->prototypes[b.y];
		elClosure *new_closure = elf_new_closure(R,proto);
		for (int i = 0; i < proto.zcache; ++i) {
			new_closure->enclosure[i] = locals[b.x+i];
		}
		locals[b.x].tag = TAG_CLS;
		locals[b.x].x_cls = new_closure;
		// ncl->obj.gccolor = GC_WHITE;
	} break;
	case BC_TABLE: {
		/* ensure objects are created, then the
		local is renamed atomically, otherwise
		gc could trigger in between think the
		local is some other type */
		elTable *tab = elf_new_table(R);
		locals[b.x].tag = TAG_TAB;
		locals[b.x].x_tab = tab;
	} break;
	case BC_TYPEGUARD: {
		elf_type_check(R,bc,b.x,b.y,locals[b.x].tag);
	} break;
	case BC_METAFIELD: {
		elValue yy = locals[b.y];
		elTable *metatable = {0};
		switch (yy.tag) {
			case TAG_STR: case TAG_TAB:
			case TAG_OBJ: case TAG_CLS: {
				metatable = yy.x_obj->metatable;
			} goto _lookup;
			case TAG_NUM: {
				metatable = R->metatables.number;
			} goto _lookup;
			case TAG_INT: {
				metatable = R->metatables.integer;
			} goto _lookup;
			default: {
				elf_throw(R,bc,elf_tpf("'%s': not an object", tag2s[yy.tag]));
			} break;
		}
		if (metatable != 0) {

			_lookup:
			locals[b.x] = elf_table_lookup(metatable,locals[b.z]);

		} else elf_throw(R,bc,"Invalid object, no metatable.");
	} break;
	/* todo: why are these two so similar ... */
	case BC_INDEX: case BC_FIELD: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (yy.tag == TAG_NIL) {
			/* todo: this isn't a big deal... */
			elf_throw(R,bc,"attempted to get nil field");
		} else if (xx.tag == TAG_TAB) {
			if (xx.x_obj->color == GC_RED) {
				elf_throw(R,bc,"Invalid object, GC'd.");
			}
			locals[b.x] = elf_table_lookup(xx.x_tab,yy);
		} else if (xx.tag == TAG_OBJ) {
			if (xx.x_obj->color == GC_RED) {
				elf_throw(R,bc,"Invalid object, GC'd.");
			}
			// elf_call_overload(R,xx.x_obj->metatable,R->cache.__getfield,b.x,yy);
			elValue overload = elf_table_get_field(xx.x_obj->metatable,R->cache.__getfield);
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
			// todo: allow for strings to find substrings,
			// and return the index of the substring!
			// for instance, "my name is"["name"].
			elf_type_check(R,bc,0,TAG_INT,yy.tag);
			locals[b.x].tag = TAG_INT;
			locals[b.x].i   = locals[b.y].x_str->c[locals[b.z].i];
		} else if (xx.tag == TAG_NIL) {
			elf_throw(R,bc,"attempted to get field of nil value");
		} else elf_throw(R,bc,"invalid object to perform this operator on");
	} break;
	case BC_SETINDEX: case BC_SETFIELD: {
		elValue xx,yy,zz;
		xx = locals[b.x];
		yy = locals[b.y];
		zz = locals[b.z];
		if (xx.tag == TAG_TAB) {
#if defined(ELF_EXPERIMENTAL_FEATURES)
			if (elf_is_object_tag(yy.tag)) {
				if (yy.x_obj->metatable == elNil) {
					goto else_;
				}
				elValue overload = elf_table_get_field(yy.x_obj->metatable,R->cache.__hash);
				if (overload.tag == TAG_NIL) {
					goto else_;
				}
				if ((overload.tag == TAG_CLS) || (overload.tag == TAG_BID)) {
					elRegId base = R->top - R->call->locals;
					int ny = elf_callexx(R,yy.x_obj,overload,base,b.x,0,1);
					if (ny < 1) {
						elf_throw(R,NO_BYTE,"overload function must return at least one value");
					}
				} else elf_throw(R,NO_BYTE,"'__hash': overload must be a function");
			} else else_:
#endif
			{
				elf_table_insert(xx.x_tab,yy,zz);
			}
		} else if (xx.tag == TAG_OBJ) {

	elValue overload = elf_table_get_field(xx.x_obj->metatable,R->cache.__setfield);
	if (overload.tag != TAG_NIL) {
		if (overload.tag == TAG_CLS || overload.tag == TAG_BID) {
			/* Here we use temporary stack space to put
			the arguments, this could be skipped if the
			arguments are already next to each other in
			order. */
			elValue *home = R->top;
			elRegId base = home-locals;

			R->top += 2;
			home[0] = yy; home[1] = zz;

			elf_callexx(R,xx.x_obj,overload,base,b.x,2,0);

			R->top = home;

		} else elf_throw(R,NO_BYTE,"'__setfield': overload must be a function");

	} else elf_throw(R,NO_BYTE,"'__setfield': overload is not implemented for this object");

		} else elf_throw(R,NO_BYTE,elf_tpf("attempted to set field of '%s' value", tag2s[xx.tag]));

	} break;
	#if 0
	case BC_SETMETATABLE: {
		elValue xx = locals[b.x];
		elValue yy = locals[b.y];
		if (elf_is_object_tag(xx.tag) && yy.tag == TAG_TAB) {
			xx.x_obj->metatable = yy.x_tab;
		} else elf_unreachable;
	} break;
	case BC_SETMETAFIELD: {
		elValue xx = locals[b.x];
		if (elf_is_object_tag(xx.tag)) {
			elValue yy = locals[b.y];
			elValue zz = locals[b.z];
			elf_table_insert(xx.x_obj->metatable,yy,zz);
		} else elf_throw(R,bc,elf_tpf("'%s': not an object", tag2s[xx.tag]));
	} break;
	#endif

	case BC_METACALL: {
		elf_callex(R,locals[b.x].x_obj,b.x+1,b.x,b.y,b.z);
		elf_ensure((elInteger)(R->top - locals) >= fn.zstack);
	} break;
	case BC_CALL: {
		elf_call_function(R,b.x,b.y,b.z);
		elf_ensure((elInteger)(R->top - locals) >= fn.zstack);
	} break;
	case BC_ISNIL: {
		elValue x = locals[b.y];
		elBool nan = x.tag != TAG_INT && x.tag != TAG_NUM;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = x.tag == TAG_NIL || (nan && x.i == 0);
	} break;
	case BC_EQ: case BC_NEQ: {
		elValue x = locals[b.y];
		elValue y = locals[b.z];
		elBool eq = false;
		if ((x.tag == TAG_NIL) || (y.tag == TAG_NIL)) {
			eq = elf_is_value_nil(x) == elf_is_value_nil(y);
		} else if ((x.tag == TAG_STR) && (y.tag == TAG_STR)) {
			eq = elf_streq(x.x_str,y.x_str);
		} else if (elf_value_tag_is_numeric(x.tag) && elf_value_tag_is_numeric(y.tag)) {
			eq = x.x_int == y.x_int;
		} else eq = (x.tag == y.tag) && (x.x_int == y.x_int);

		if (b.k == BC_NEQ) eq = !eq;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = eq;
	} break;
	/* todo: make this better */
	#define CASE_IBOP(OPNAME,OP) \
	case OPNAME : {\
		elValue xx = locals[b.y];\
		elValue yy = locals[b.z];\
		if (((b.k == BC_DIV) || (b.k == BC_MOD))) {\
			elf_check_division_by_zero(R,xx,yy);\
		}\
		locals[b.x].tag = TAG_INT;\
		locals[b.x].x_int = elf_toint(xx) OP elf_toint(yy);\
	} break
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
	case OPCODE : {\
		elValue xx = locals[b.y];\
		elValue yy = locals[b.z];\
		if (elf_is_object_tag(xx.tag) || elf_is_object_tag(yy.tag)) {\
			if (!elf_is_object_tag(xx.tag)) elf_throw(R,NO_BYTE,"invalid ordering, object type must come first");\
			if (((b.k == BC_DIV) || (b.k == BC_MOD))) {\
				elf_check_division_by_zero(R,xx,yy);\
			}\
			elf_call_overload(R,xx.x_obj,elf_is_object_tag(yy.tag)?FN:FN1,b.x,yy);\
		} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {\
			if (((b.k == BC_DIV) || (b.k == BC_MOD))) {\
				elf_check_division_by_zero(R,xx,yy);\
			}\
			if (!elf_value_tag_is_numeric(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			locals[b.x].tag = TAG_NUM;\
			locals[b.x].x_num = elf_tonum(xx) OP elf_tonum(yy);\
		} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {\
			if (!elf_value_tag_is_numeric(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			if (((b.k == BC_DIV) || (b.k == BC_MOD))) {\
				elf_check_division_by_zero(R,xx,yy);\
			}\
			locals[b.x].tag = TAG_INT;\
			locals[b.x].x_int = elf_toint(xx) OP elf_toint(yy);\
		} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XSTRINGIFY(OP)));\
	} break
	case BC_LTEQ: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elf_tonum(xx) <= elf_tonum(yy);
		} else {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elf_toint(xx) <= elf_toint(yy);
		}
	} break;
	case BC_LT: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (xx.tag == TAG_NUM || yy.tag == TAG_NUM) {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elf_tonum(xx) < elf_tonum(yy);
		} else {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elf_toint(xx) < elf_toint(yy);
		}
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
	}

	leave:
	return call->ny;
}

