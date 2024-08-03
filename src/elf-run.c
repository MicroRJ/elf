/*
** See Copyright Notice In elf.h
** elf-run.c
** Runtime
*/


/* NOTE: DO NOT AUTO-FORMAT THIS FILE */


void elf_begin(elState *R, elModule *M) {
	R->M = M;
	R->bytelogging = false;
	R->stack_length = 4096;
	R->stack = elf_clear_alloc(elHEAP_ALLOCATOR,sizeof(elValue)*R->stack_length);
	R->stack_top = R->stack;
	R->metatables.string = elf_new_string_metatable(R);
	R->metatables.table = elf_new_table_metatable(R);
	R->call_level = 0;

	//TODO: So when you call a function,
	//using 0 for rx, 0 is relative to locals,
	//and it will override values that have
	//been pushed... I added a temporary fix...
	R->root_call = (elStackFrame){0};
	R->root_call.locals = R->top;
	R->call = &R->root_call;

	M->strings = elf_add_new_table(R);
	M->globals = elf_add_new_table(R);
	/* this will be replaced, todo: */
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
	return elf_call_function2(R,elNIL,rxy,rxy,nx,ny);
}


int elf_callexx(elState *R, elObject *obj, elValue fn, elRegId rx, elRegId ry, int nx, int ny) {
	elStackFrame *caller = R->call;
	// elASSERT((R->top-caller->locals)+caller->cl->fn.nlocals-1 > rx);
	elValue *locals = caller->locals + rx;
	/* top always points to one past locals,
	so far we only have nx argument locals,
	top is later incremented to match nlocals */
	elValue *top  = locals + nx;
	elStackFrame call = {0};
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
		for (; nx < call.cls->proto.nlocals; ++ nx) {
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
		if (fn.c != elNIL) {
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
	elASSERT(R->call_level > -1);
	return nyield;
}


int elf_call_function2(elState *R, elObject *obj, elRegId rx, elRegId ry, int nx, int ny) {
	elStackFrame *caller = R->call;
	return elf_callexx(R,obj,caller->locals[rx],rx+1,ry,nx,ny);
}


int elf_load_expr_fs(elState *R, elFileState *fs, elString *filename, elRegId rxy, int ny, elString *contents) {

	elModule *M = R->M;

	fs->R = R;
	fs->M = M;
	fs->filename   = filename->contents;
	fs->contents   = contents->contents;
	fs->thischar   = fs->contents;
	fs->linechar   = fs->contents;
	fs->linenumber = 1;
	/* the module keeps track of all the bytes, so
	n-bytes is naturally the next byte id */
	fs->bytes      = M->nbytes;

	/* kick start by lexing the first two tokens */
	elf_lexone(fs);
	elf_lexone(fs);

	elFileFnState fn = {0};
	elf_emitter_begin_function(fs,&fn,fs->this_token.line);
	elNodeId id = elf_load_file_expr(fs,0);
	elf_emit_yield(fs,fs->this_token.line,id);
	elf_emitter_close_function(fs);

	elFileProto fp = {0};
	fp.bytes    = fn.bytes;
	fp.nbytes   = M->nbytes - fn.bytes;
	fp.contents = contents;
	fp.nlocals  = fn.nlocals;
	ARRAY_ADD(M->files,fp);

	/* this closure will keep the file alive, since
	it references the file directly... In case I
	ever thought that the name and contents strings
	could be GC'd... However, once all closures go
	MIA, the file will be rightfully collected, and
	even though it is in the files array kept by the
	modules, we shouldn't have to reference it...
	Though we need something to get rid of dead
	files, we prob don't even need the files array
	because we can do lookups on closure objects
	themselves, though this would be slower since
	we don't have any form of hierarchical benefits... */
	elValue cls = elf_closure_value(elf_new_closure(R,fp));
	return elf_callexx(R,elNIL,cls,rxy,rxy,0,ny);
}


int elf_load_code_fs(elState *R, elFileState *fs, elString *filename, elRegId rxy, int ny, elString *contents) {
	if ((filename == elNIL) || (contents == elNIL)) {
		return -1;
	}

	elModule *M = R->M;
	fs->R = R;
	fs->M = M;
	fs->filename   = filename->contents;
	fs->contents   = contents->contents;
	fs->thischar   = fs->contents;
	fs->linechar   = fs->contents;
	fs->linenumber = 1;
	fs->bytes      = M->nbytes;

	/* kick start by lexing the first two tokens */
	elf_lexone(fs);
	elf_lexone(fs);

	elFileFnState fn = {0};
	elf_emitter_begin_function(fs,&fn,fs->tk.line);
	while (!elf_test_token(fs,0)) {
		elf_load_file_stat(fs);
	}
	elf_emitter_close_function(fs);

	elFileProto fp = {0};
	fp.name     = filename;
	fp.contents = contents;
	fp.bytes    = fn.bytes;
	fp.nbytes   = M->nbytes - fn.bytes;
	fp.nlocals  = fn.nlocals;
	ARRAY_ADD(M->files,fp);

	elValue cls = elf_closure_value(elf_new_closure(R,fp));
	return elf_callexx(R,elNIL,cls,rxy,rxy,0,ny);
}


int elf_load_file_fs(elState *R, elFileState *fs, elString *name, elRegId x, int y) {
	char *contents;
	elError error = sys_load_file_contents(elHEAP_ALLOCATOR,(void**)&contents,name->contents);
	if (elFAILED(error)) {
		elf_logerror("'%s': could not load file",name->contents);
		return -1;
	}
	/* todo: would we instead allocate the string beforehand... */
	elString *string = elf_add_new_string(R,contents);
	return elf_load_code_fs(R,fs,name,x,y,string);
}


int elf_load_code(elState *R, elString *filename, elRegId rxy, int ny, elString *contents) {
	elFileState fs = {0};
	return elf_load_code_fs(R,&fs,filename,rxy,ny,contents);
}


int elf_load_expr(elState *R, elString *filename, elRegId rxy, int ny, elString *contents) {
	elFileState fs = {0};
	return elf_load_expr_fs(R,&fs,filename,rxy,ny,contents);
}


int elf_load_file(elState *R, elString *filename, elRegId rxy, int ny) {
	elFileState fs = {0};
	return elf_load_file_fs(R,&fs,filename,rxy,ny);
}


void elf_check_division_by_zero(elState *S, elValue xx, elValue yy) {
	if ((yy.tag == TAG_NUM) && (yy.n == 0.)) elf_throw(S,NO_BYTE,"division by zero");
	else if ((yy.tag == TAG_INT) && (yy.i == 0)) elf_throw(S,NO_BYTE,"integer division by zero");
}


int elf_call_overload(elState *S, elObject *obj, elString *name, elRegId io, elValue in) {
	if (obj->metatable == elNIL) {
		elf_throw(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}
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


/*
** If an object is white it marks it black
** and marks its references.
** Returns the number of objects that have
** been uniquely marked.
** If already black, 0.
** If white, 1 + marked children.
*/
elInteger elf_mark_object(elObject *obj) {
	elASSERT(obj != 0);
	elASSERT(obj->color != GC_RED);
	/* black object simply means it was
	already marked and we found another
	path to it, since the object was
	already accounted for, return 0 */
	if (obj->color == GC_BLACK) {
		return 0;
	}
	elInteger num = 1;
	/* Here we check whether the object
	is explicitly white, because there
	are other colors that we don't want
	to get rid of, I suppose we don't
	propagate pink because if the object
	were to not become pink anymore we'd
	have to also propagate those changes
	latter... Pink objecs are rare though...
	I think we only use them for debugging... */
	if (obj->color == GC_WHITE) {
		obj->color = GC_BLACK;
	}
	if (obj->metatable) {
		num += elf_mark_object((elObject*)obj->metatable);
	}
	if (obj->type == OBJ_CLS) {
		elClosure *cls = (elClosure*) obj;
		FOR_RANGE(i, 0, cls->proto.nlocals) {
			if (elISOBJTAG(cls->values[i].tag)) {
				num += elf_mark_object(cls->values[i].x_obj);
			}
		}
	} else if (obj->type == OBJ_TAB) {
		elTable *table = (elTable*) obj;
		elValue *array = table->array;
		elEntry *slots = table->slots;
		elInteger k;
		for (k=0; k<table->ntotal; ++k) {
			if (elISOBJTAG(slots[k].key.tag)) {
				num += elf_mark_object(slots[k].key.x_obj);
			}
		}
		for (k=0; k<ARRAY_LENGTH(array); ++k) {
			if (elISOBJTAG(array[k].tag)) {
				num += elf_mark_object(array[k].x_obj);
			}
		}
	}

	return num;
}


elInteger elf_unmark_objects(elState *R) {
	elInteger result = 0;
	elInteger i;
	elObject **objects = R->memory.objects;
	for (i = 0; i < ARRAY_LENGTH(objects); i ++) {
		elObject *it = objects[i];
		switch (it->color) {
			case GC_RED:
			elf_debugger("internal error: gc failed");
			break;
			case GC_BLACK: {
				it->color = GC_WHITE;
				result += 1;
			} break;
			default: break;
		}
	}
	return result;
}


elInteger elf_hold_phase(elState *R) {
	elASSERT(R->memory.phase == elGC_PHASE_HOLD);
	R->memory.phase ^= 1;

	elInteger num = elf_mark_object((elObject*)R->M->globals);
	elValue *val;
	for (val=R->stack; val<R->stack_top; ++val) {
		if (elISOBJTAG(val->tag)) {
			num += elf_mark_object(val->x_obj);
		}
	}
	return num;
}


elInteger elf_free_phase(elState *R) {
	elASSERT(R->memory.phase == elGC_PHASE_FREE);
	R->memory.phase ^= 1;


	elObject **new_objects = R->memory.new_objects;
	elObject **objects = R->memory.objects;
	if (new_objects) {
		ARRAY(new_objects).min = 0;
	}
	elInteger n = 0;
	elInteger k;
	for (k=0; k<ARRAY_LENGTH(objects); ++ k) {
		elObject *it = objects[k];
		if (it == 0 || it->color == GC_RED) {
			elf_throw(R,NO_BYTE,"internal error, GC failed");
		}
		if (it->color == GC_BLACK) {
			it->color = GC_WHITE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (it->color == GC_WHITE) {
			n += 1;
			it->color = GC_RED;
			R->memory.memory_allocated -= it->tell;
			if (it->type == OBJ_TAB) {
				elf_dealloc_table((elTable*)it);
			}
			elf_dealloc(elHEAP_ALLOCATOR,it);
		} else n += 1;
	}
	R->memory.objects = new_objects;
	R->memory.new_objects = objects;
	return n;
}


elInteger elf_trigger_collection_cycle(elState *R) {
	elInteger time_ = elf_clocktime();
	elInteger num_marked = elf_hold_phase(R);
	elInteger num_objects = ARRAY_LENGTH(R->memory.objects);
	elInteger num_to_collect = num_objects - num_marked;
	elf_debug_log("GC: %lli - %lli -> %lli, (total - marked = expected)",num_objects,num_to_collect,num_marked);
	elInteger num_collected = elf_free_phase(R);
	num_to_collect -= num_collected;
	elf_debug_log("	(%f) => leaked: %lli"
	, elf_timediffms(time_),num_to_collect);
	return num_collected;
}


void elf_collect(elState *R) {
	if (R->memory.paused) {
		return;
	}
	if (R->memory.memory_threshold <= 0) {
		R->memory.memory_threshold = elGC_MEM_THRESHOLD_MIN;
	}
	if (R->memory.object_trigger_threshold <= 0) {
		R->memory.object_trigger_threshold = elGC_OBJ_THRESHOLD_MIN;
	}
	elInteger num_objects = ARRAY_LENGTH(R->memory.objects);
	elInteger obj_threshold = R->collector.object_trigger_threshold;
	if (num_objects > obj_threshold) {
		elInteger num_collected = elf_trigger_collection_cycle(R);
		elASSERT(num_collected <= num_objects);
		R->collector.object_trigger_threshold += elGC_OBJ_THRESHOLD_MIN - num_collected;
	} else if (R->memory.memory_allocated > R->memory.memory_threshold) {
		R->memory.memory_threshold *= 2;
		if (R->memory.memory_threshold > elGC_MEM_THRESHOLD_MAX) {
			R->memory.memory_threshold = elGC_MEM_THRESHOLD_MAX;
		}
		elf_trigger_collection_cycle(R);
		if (R->memory.memory_allocated > R->memory.memory_threshold) {
			elf_throw(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated",R->memory.memory_allocated / MEGABYTES(1)));
		}
	}
}


void *elf_new_object(elState *R, elObjType type, elInteger tell) {
	R->memory.memory_allocated += tell;
	elf_collect(R);

	elObject *obj = elf_clear_alloc(elHEAP_ALLOCATOR,tell);
	obj->color = (elGCColor) R->memory.phase;
	if (obj->color != GC_WHITE) {
		elf_throw(R,NO_BYTE,"object allocation out of phase");
	}
	obj->type  = type;
	obj->tell  = tell;

	ARRAY_ADD(R->memory.objects,obj);
	return obj;
}

int elf_run(elState *R) {
	elModule *md = R->md;
	//
	elModule *M = R->M;
	elStackFrame *call = R->call;
	elStackFrame *caller = call->caller;
	elValue *locals = call->locals;
	elValue *values = call->cls->values;
	elTable *globals = M->globals;
	elFileProto proto = call->cls->proto;
	elASSERT(elWITHIN((elInteger)(R->top - locals), proto.nlocals, proto.nlocals + 1024)); //  1024 is some arbitrary to detect extraneous errors

	while (call->tail < proto.nbytes) {
		/* todo: call->tail is redundant ... */
		elInteger jp = call->tail ++;
		elInteger bc = proto.bytes + jp;
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
				elFileProto file = M->files[elf_find_file_info_by_byte(M,bc)];
				elFileLine line = M->lines[bc];
				int linenum;
				elf_get_line_location_info(file.lines,line,&linenum,0);
				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,bc,track);
			}
		}
#endif
		switch (b.k) {
	case BC_LEAVE: {
		if (call->delay_list != elNIL) {
			call->tail = call->delay_list->j;
			call->delay_list = call->delay_list->n;
		} else goto leave;
	} break;
	case BC_DELAY: {
		/* todo: can we make this better */
		elDelaylist *delay = elf_alloc(elHEAP_ALLOCATOR,sizeof(elDelaylist));
		delay->n = call->delay_list;
		delay->j = call->tail;
		call->delay_list = delay;

		elASSERT(b.i >= 0);
		call->tail = jp + b.i;
	} break;
	case BC_YIELD: {
		elASSERT(b.x >= 0);
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
	// 	if (fname == elNIL) elf_throw(R,bc,"'load': attempted to call load with nil");
	// 	int result = elf_load_file(R,fname,b.x,b.y);
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
	/* todo: this is to be soon deprecated
	When you call a function 'this' will always be passed,
	if you call a field or metafield, this will be the object,
	if you call a function not from a field or metafield
	you have to pass in 'this' manually using special syntax,
	otherwise this defaults to the current...
	For instance,

	my_table:mymetafield(1,2,3)
	my_table.myregularfield(1,2,3)

	'this' is implicit here as 'my_table',

	let field = my_table.myregularfield

	field(1,2,3)

	'this' is implicit here as 'this' (from the current context),

	field<my_table>(1,2,3)

	'this' is explicit here as 'my_table',

	The default 'this' is the current closure object...

	To get the current closure object, always, you can
	do '#this'.

	The closure object is above 'this', so '#this'
	translates could translate to
	elf.get_local(#register this - 1)

	*/
	case BC_LOADTHIS: {
		if (call->obj == elNIL) {
			elf_throw(R,NO_BYTE,"'this' is invalid for this function, not a meta-call");
		}
		locals[b.x].tag   = elOBJTOTAG(call->obj->type);
		locals[b.x].x_obj = call->obj;
	} break;
	case BC_RELOAD: {
		locals[b.x] = locals[b.y];
	} break;
	/* todo: prob cache 'globals->values' as globals
	instead, but then we'd have to sync after every
	call point or table field set... */
	case BC_LOADGLOBAL: {
		locals[b.x] = globals->values[b.y];
	} break;
	case BC_SETGLOBAL: {
		globals->values[b.x] = locals[b.y];
	} break;
	case BC_LOADNIL: {
		locals[b.x].tag    = TAG_NIL;
		locals[b.x].x_int  = 0;
	} break;
	case BC_LOADINT: {
		locals[b.x].tag   = TAG_INT;
		locals[b.x].x_int = M->integers[b.y];
	} break;
	case BC_LOADNUM: {
		locals[b.x].tag   = TAG_NUM;
		locals[b.x].x_num = M->numbers[b.y];
	} break;
	case BC_LOADCACHE: {
		elASSERT(elWITHIN(b.y,0,proto.nvalues));
		locals[b.x] = values[b.y];
	} break;
	case BC_CLOSURE: {
		elASSERT(elWITHIN(b.y,0,ARRAY_LENGTH(M->functions)));
		elFileProto proto = M->functions[b.y];
		elClosure *new_closure = elf_new_closure(R,proto);
		// elf_copy_memory(new_closure->values,locals + b.x, sizeof(elValue) * proto.nvalues);
		for (int i = 0; i < proto.nvalues; ++i) {
			new_closure->values[i] = locals[b.x+i];
		}
		locals[b.x].tag   = TAG_CLS;
		locals[b.x].x_cls = new_closure;
	} break;
	case BC_TABLE: {
		/* Quick Note:
		I had this bug once that took me a while to find...
		The problem was that I was doing this:

		#line 0 locals[b.x].tag = TAG_TAB;
		#line 1 locals[b.x].x_tab = elf_new_table(R);

		It was pretty hard to spot back then, here's what
		happens, if GC triggers, in line 1, it will then
		traverse the locals (as it marks every object),
		and think that it is a table (because we modified
		the local in line 0), but in reality, we've
		replaced whatever object was there before.

		So in essence, ensure locals are modified
		"atomically", NEVER in between GC cycles,
		otherwise GC will get confused...
		*/
		elTable *tab = elf_new_table(R);
		locals[b.x].tag   = TAG_TAB;
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
				elf_throw(R,bc,"this feature is not implemented yet, metatables for numeric types");
			} goto _lookup;
			case TAG_INT: {
				metatable = R->metatables.integer;
				elf_throw(R,bc,"this feature is not implemented yet, metatables for numeric types");
			} goto _lookup;
			default: {
				elf_throw(R,bc,elf_tpf("'%s': not an object", tag2s[yy.tag]));
			} break;
		}
		_lookup:
		if (metatable == 0) {
			elf_throw(R,bc,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
		}
		locals[b.x] = elf_table_lookup(metatable,locals[b.z]);
	} break;
	/* todo: why are these two so similar ... */
	case BC_INDEX: case BC_FIELD: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (yy.tag == TAG_NIL) {
			/* I don't know whether this worth throwing an error
			over, but we can't have nil keys, therefore nil fields
			are impossible, and most of the time I do like the
			interpreter complaining about this because I have never
			actually wanted to access a nil field... */
			elf_throw(R,bc,"attempted to get nil field");
		} else if (xx.tag == TAG_TAB) {
			locals[b.x] = elf_table_lookup(xx.x_tab,yy);
		} else if (xx.tag == TAG_OBJ) {
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
			locals[b.x].tag   = TAG_INT;
			locals[b.x].x_int = locals[b.y].x_str->contents[locals[b.z].i];
		} else if (xx.tag == TAG_NIL) {
			elf_throw(R,bc,"attempted to get field of 'nil' value");
		} else elf_throw(R,bc,"invalid object to perform this operator on");
	} break;
	case BC_SETINDEX: case BC_SETFIELD: {
		elValue xx,yy,zz;
		xx = locals[b.x];
		yy = locals[b.y];
		zz = locals[b.z];
		if (yy.tag == TAG_NIL) {
			elf_throw(R,bc,"attempted to set 'nil' field");
		}
		if (xx.tag == TAG_TAB) {
			elf_table_set(xx.x_tab,yy,zz);
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
	case BC_METACALL: {
		elf_call_function2(R,locals[b.x].x_obj,b.x+1,b.x,b.y,b.z);
		elASSERT(elWITHIN((elInteger)(R->top - locals), proto.nlocals, proto.nlocals + 1024)); // 1024 is just some arbitrary value to detect extraneous errors...
	} break;
	case BC_CALL: {
		elf_call_function(R,b.x,b.y,b.z);
		elASSERT(elWITHIN((elInteger)(R->top - locals), proto.nlocals, proto.nlocals + 1024)); // 1024 is just some arbitrary value to detect extraneous errors...
	} break;
	case BC_ISNIL: {
		elValue x = locals[b.y];
		elBool nan = x.tag != TAG_INT && x.tag != TAG_NUM;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = x.tag == TAG_NIL || (nan && x.x_int == 0);
	} break;
	case BC_EQ: case BC_NEQ: {
		elValue x = locals[b.y];
		elValue y = locals[b.z];
		elBool eq = false;
		if ((x.tag == TAG_NIL) || (y.tag == TAG_NIL)) {
			eq = elISNIL(x) == elISNIL(y);
		} else if ((x.tag == TAG_STR) && (y.tag == TAG_STR)) {
			eq = elf_streq(x.x_str,y.x_str);
		} else if (elISNUMTAG(x.tag) && elISNUMTAG(y.tag)) {
			eq = x.x_int == y.x_int;
		} else eq = (x.tag == y.tag) && (x.x_int == y.x_int);

		if (b.k == BC_NEQ) eq = !eq;
		locals[b.x].tag = TAG_INT;
		locals[b.x].i   = eq;
	} break;
	case BC_POW: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {
			elNOCODE;
		} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
			locals[b.x].tag   = TAG_NUM;
			locals[b.x].x_num = pow(elTONUM(xx),elTONUM(yy));
		} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {
			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
			locals[b.x].tag   = TAG_INT;
			locals[b.x].x_int = pow(elTOINT(xx),elTOINT(yy));
		} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));
	} break;
	case BC_MOD: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {
			elNOCODE;
		} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
			elf_check_division_by_zero(R,xx,yy);

			elNumber x = elTONUM(xx);
			elNumber y = elTONUM(yy);
			elNumber z = x - (elInteger)(x / y) * y;

			locals[b.x].tag   = TAG_NUM;
			locals[b.x].x_num = z;
		} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {

			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
			elf_check_division_by_zero(R,xx,yy);

			locals[b.x].tag   = TAG_INT;
			locals[b.x].x_int = elTOINT(xx) % elTOINT(yy);

		} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));
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
		locals[b.x].x_int = elTOINT(xx) OP elTOINT(yy);\
	} break
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
	case OPCODE : {\
		elValue xx = locals[b.y];\
		elValue yy = locals[b.z];\
		if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {\
			if (!elISOBJTAG(xx.tag)) elf_throw(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
			/* Could we redefine this? */ \
			if (b.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
			elf_call_overload(R,xx.x_obj,elISOBJTAG(yy.tag)?FN:FN1,b.x,yy);\
		} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {\
			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			/* Could we redefine this? */ \
			if (b.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
			locals[b.x].tag = TAG_NUM;\
			locals[b.x].x_num = elTONUM(xx) OP elTONUM(yy);\
		} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {\
			if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			/* Could we redefine this? */ \
			if (b.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
			locals[b.x].tag = TAG_INT;\
			locals[b.x].x_int = elTOINT(xx) OP elTOINT(yy);\
		} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));\
	} break
	case BC_LTEQ: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elTONUM(xx) <= elTONUM(yy);
		} else {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elTOINT(xx) <= elTOINT(yy);
		}
	} break;
	case BC_LT: {
		elValue xx = locals[b.y];
		elValue yy = locals[b.z];
		if (xx.tag == TAG_NUM || yy.tag == TAG_NUM) {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elTONUM(xx) < elTONUM(yy);
		} else {
			locals[b.x].tag = TAG_INT;
			locals[b.x].x_int = elTOINT(xx) < elTOINT(yy);
		}
	} break;
	CASE_IBOP(BC_SHL,  <<);
	CASE_IBOP(BC_SHR,  >>);
	CASE_IBOP(BC_BIT_XOR, ^);
	CASE_IBOP(BC_BIT_AND, &);
	CASE_IBOP(BC_BIT_OR,  |);
	/* todo: could we cache these strings */
	CASE_BOP(BC_ADD, +, R->cache.__add, R->cache.__add1);
	CASE_BOP(BC_SUB, -, R->cache.__sub, R->cache.__sub1);
	CASE_BOP(BC_MUL, *, R->cache.__mul, R->cache.__mul1);
	CASE_BOP(BC_DIV, /, R->cache.__div, R->cache.__div1);
	#undef CASE_BOP
	default: {
		elNOCODE;
	} break;
		}
	}

	leave:
	return call->ny;
}

