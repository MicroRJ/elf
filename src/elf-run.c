/*
** See Copyright Notice In elf.h
** elf-run.c
** Runtime
*/


void elf_begin(elState *R, elModule *M) {
	R->M = M;
	R->Z = elDEFAULT_STACK_SIZE;
	R->K = elf_calloc(elHEAP_ALLOCATOR,sizeof(elValue)*R->Z);
	R->T = R->K;
	R->call_level = 0;

	R->metatables.string = elf_new_string_metatable(R);
	R->metatables.table  = elf_new_table_metatable(R);

	/* Global table is on the stack, so we don't really
	to need treat it independently when GC'ing... */
	M->globals = elf_add_new_table(R);
	M->strings = elf_add_new_table(R);
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

	elf_lib_loadfunctions(R);
	// crtlib_load(R);
	// elf_netlib_loadfunctions(R);
}


/* todo: instead remove the object */
int elf_call_function3(elState *R, elObject *obj, int nx, int ny, elRegId ry) {
	elStackFrame call = {0};
	call.caller 		= R->call;
	call.call_instr 	= R->byte;
	call.locals 		= elGETTOP(R) - nx;
	call.ry           = ry;
	call.nx           = nx;
	call.ny           = ny;
	/* todo: remove */
	call.obj 			= obj;

	elValue fn = call.locals[-1];
	elASSERT(elISFUNTAG(fn.tag) && !elISNIL(fn));

	elValue *top = call.locals + nx;
	if (fn.tag == TAG_CLS) {
		call.cls = fn.x_cls;
		elf_clear_memory(call.locals + nx,call.cls->proto.nlocals*sizeof(elValue));
		top = call.locals + call.cls->proto.nlocals;
	}

	elSETTOP(R,top);
	R->call = &call;
	R->call_level ++;

	if (R->flags & FLAG_DEBUGGER_ONCALL) {
		elf_debugger("debugger 'FLAG_DEBUGGER_ONCALL'");
	}
	elRegId nyield = 0;
	if (fn.tag == TAG_CLS) {
		nyield = elf_run(R);
	} else
	if (fn.tag == TAG_CFN) {
		nyield = fn.x_cfn(R);
		/* ensure that the results were pushed to the stack */
		if ((elGETTOP(R) - top) < nyield) {
			elf_throw(R,NO_BYTE,elf_tpf("number of values on stack '%i', is incoherent with specified number of yielded values '%i'",(int)(elGETTOP(R) - top),nyield));
		}
		/* todo: replace with memcpy? */
		for (int i = 0; i < MIN(nyield,ny); ++ i) {
			/* todo: ry will be removed once we unify the calling
			convention, since object will no longer interfere with
			where we want to put the results */
			call.locals[ry+i] = elGETTOP(R)[i-nyield];
		}
	} else {
		nyield = -1;
		elf_throw(R,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[fn.tag]));
	}
	R->call 	= call.caller;
	R->call_level -= 1;
	elASSERT(R->call_level > -1);
	elValue *T = elGETTOP(R);
	elSETTOP(R,call.locals+ry+nyield);
	// elf_debug_log("at instruction: %i, ry: %i, ny: %i, nlocals: %i, T-: %lli, V: %s, %s"
	// , R->byte, ry, nyield, call.cls ? call.cls->proto.nlocals : 0
	// , (elInteger) (T - R->T), tag2s[(R->T-2)->tag], tag2s[(R->T-1)->tag]);
	return nyield;
}



int elf_load_expr3_fs(elState *R, elFileState *fs, elString *filename, elRegId rxy, int ny, elString *contents) {

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

	// elValue cls = elf_closure_value(elf_new_closure(R,fp));
	// return elf_call_function(R,0,cls,rxy,rxy,0,ny);
	elNOCODE;
	return -1;
}


int elf_load_code3_fs(elState *R, elFileState *fs, elString *filename, elRegId ry, int ny, elString *contents) {
	if ((filename == 0) || (contents == 0)) {
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

	elASSERT(elOBJCOLOR(filename) != GC_RED);
	elASSERT(elOBJCOLOR(contents) != GC_RED);
	elFileProto fp = {0};
	fp.name     = filename;
	fp.contents = contents;
	fp.bytes    = fn.bytes;
	fp.nbytes   = M->nbytes - fn.bytes;
	fp.nlocals  = fn.nlocals;
	ARRAY_ADD(M->files,fp);

	/* todo: this is such a hack!
	This is to prevent the closure from getting collected,
	if the closure gets collected then we can't do error
	reporting...
	Although, if we load a file, we also load a bunch of
	closures and objects, so they should keep the file
	alive, but we don't have that system in place yet... */
	elClosure *cls = elf_new_closure(R,fp);
	elf_add_closure(R,cls);
	elf_table_add(M->globals,elf_closure_value(cls));

	return elf_call_function3(R,0,0,ny,ry);
}


/* todo: add support for arguments */
int elf_load_file3_fs(elState *R, elFileState *fs, elString *name, elRegId ry, int ny) {

	char *contents;
	elError error = sys_load_file_contents(elHEAP_ALLOCATOR,(void**)&contents,name->contents);

	if (elFAILED(error)) {
		elf_logerror("'%s': could not load file",name->contents);
		return -1;
	}
	/* todo: could we instead allocate the string beforehand
	and read the file into it, like by passing in a string
	allocator... */
	elString *string = elf_add_new_string(R,contents);
	int nyield = elf_load_code3_fs(R,fs,name,ry,ny,string);
	return nyield;
}


int elf_load_code3(elState *R, elString *filename, elRegId ry, int ny, elString *contents) {
	elFileState fs = {0};
	return elf_load_code3_fs(R,&fs,filename,ry,ny,contents);
}


int elf_load_expr3(elState *R, elString *filename, elRegId ry, int ny, elString *contents) {
	elFileState fs = {0};
	return elf_load_expr3_fs(R,&fs,filename,ry,ny,contents);
}

int elf_load_file3(elState *R, elString *name, elRegId ry, int ny) {
	elFileState fs = {0};
	return elf_load_file3_fs(R,&fs,name,ry,ny);
}


void elf_check_division_by_zero(elState *S, elValue xx, elValue yy) {
	if ((yy.tag == TAG_NUM) && (yy.x_num == 0.)) elf_throw(S,NO_BYTE,"division by zero"); else
	if ((yy.tag == TAG_INT) && (yy.x_int == 0)) elf_throw(S,NO_BYTE,"integer division by zero");
}


/* todo: remove this, instead make a function that simply gets the value
and checks that is is a function... */
int elf_call_overload(elState *S, elObject *obj, elString *name, elRegId io, int nargs, elValue *args) {

	if (obj->metatable == 0) {
		elf_throw(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}

	elValue field = elf_table_get_field(obj->metatable,name);
	if (!elISFUNTAG(field.tag)) {
		elf_throw(S,NO_BYTE,elf_tpf("'%s': overload is %s, not a function",name->c,tag2s[field.tag]));
	}

	elPUSH(S,field);
	elf_copy_memory(elGETTOP(S),args,sizeof(elValue)*nargs);
	elGETTOP(S) += nargs;

	int ny = elf_call_function3(S,obj,nargs,1,io);
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
	latter... Pink objects are rare though...
	I think we only use them for debugging... */
	if (obj->color == GC_WHITE) {
		obj->color = GC_BLACK;
	}
	if (obj->metatable) {
		num += elf_mark_object((elObject*)obj->metatable);
	}
	if (obj->type == GC_CLS) {
		elClosure *cls = (elClosure*) obj;
		if (cls->proto.name != 0) {
			elf_mark_object(elTOOBJ(cls->proto.name));
		}
		if (cls->proto.contents != 0) {
			elf_mark_object(elTOOBJ(cls->proto.contents));
		}
		if (cls->proto.parent != -1) {
			/* todo: implement this */
		}
		FOR_RANGE(i, 0, cls->proto.nlocals) {
			if (elISOBJTAG(cls->values[i].tag)) {
				num += elf_mark_object(cls->values[i].x_obj);
			}
		}
	} else if (obj->type == GC_TAB) {
		elTable *table = (elTable*) obj;
		elValue *values = table->values;
		elEntry *entries = table->entries;
		FOR_RANGE(k,0,table->ntotal) {
			if (elISOBJTAG(entries[k].key.tag)) {
				num += elf_mark_object(entries[k].key.x_obj);
			}
		}
		FOR_RANGE(k,0,ARRAY_LENGTH(values)) {
			if (elISOBJTAG(values[k].tag)) {
				num += elf_mark_object(values[k].x_obj);
			}
		}
	}
	return num;
}


elInteger elf_hold_phase(elState *R) {
	elASSERT(R->memory.phase == elGC_PHASE_HOLD);
	R->memory.phase ^= 1;

	elInteger Ni = 0;
	elValue *Ki;
	for (Ki = R->K; Ki < elGETTOP(R); ++ Ki) {
		if (elISOBJTAG(Ki->tag)) {
			Ni += elf_mark_object(Ki->x_obj);
		}
	}
	return Ni;
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
			if (it->type == GC_STR && ((elString*)(it))->length > 512) {
				elf_debug_log("deallocated fairly large string: %p, %i", it, ((elString*)(it))->length);
			}
			if (it->type == GC_TAB) {
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


void *elf_new_object(elState *R, elGCTy type, elInteger tell) {
	R->memory.memory_allocated += tell;
	elf_collect(R);

	elObject *obj = elf_calloc(elHEAP_ALLOCATOR,tell);
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
	elModule *M = R->M;
	elStackFrame *call = R->call;
	elValue *locals = call->locals;
	elValue *values = call->cls->values;
	elTable *globals = M->globals;
	elFileProto proto = call->cls->proto;

	elByteId next_instr = 0;
	while (next_instr < proto.nbytes) {
		elInteger instr = next_instr ++;
		elInteger module_instr = proto.bytes + instr;
		elBytecode byte = M->bytes[module_instr];
		/* todo: eventually, maybe not do this per instruction */
		R->byte = module_instr;

#if defined(_DEBUG)
		if (R->flags & FLAG_BYTELOGGING || call->logging) {
			elf_bytefpf(stdout,M,-1,instr,byte);
		}
		if (R->flags & FLAG_DEBUGGER) {
			elf_debugger("debugger 'FLAG_DEBUGGER'");
		}
#endif

#if defined(ELF_EXPERIMENTAL_FEATURES)
		if (R->bytetracking) {
			elInteger track = ++ M->track[module_instr];
			if (track == 64) {
				elFileProto file = M->files[elf_get_file_for_byte(M,module_instr)];
				elFileLine line = M->lines[module_instr];
				int linenum;
				elf_get_line_location_info(file.lines,line,&linenum,0);
				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,module_instr,track);
			}
		}
#endif
		switch (byte.k) {
			case BC_LEAVE: {
				if (call->delay_list != 0) {
					next_instr = call->delay_list->j;
					call->delay_list = call->delay_list->n;
				} else goto leave;
			} break;
			case BC_DELAY: {
		/* todo: can we make this better */
				elDelaylist *delay = elf_alloc(elHEAP_ALLOCATOR,sizeof(elDelaylist));
				delay->n = call->delay_list;
				delay->j = next_instr;
				call->delay_list = delay;

				elASSERT(byte.i >= 0);
				next_instr = instr + byte.i;
			} break;
			case BC_YIELD: {
				elASSERT(byte.x >= 0);
				/* check that we don't exceed number of
				expected outputs */
				int ny = MIN(byte.z,call->ny);
				for (elRegId y = 0; y < ny; ++y) {
					locals[call->ry+y] = locals[byte.y+y];
				}
				call->ny = ny;
				next_instr = instr + byte.x;
			} break;
			case BC_J: {
				next_instr = instr + byte.i;
			} break;
			case BC_JZ: {
				if (locals[byte.y].x_int == 0) {
					next_instr = instr + byte.x;
				}
			} break;
			case BC_JNZ: {
				if (locals[byte.y].x_int != 0) {
					next_instr = instr + byte.x;
				}
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
				if (call->obj == 0) {
					elf_throw(R,NO_BYTE,"'this' is invalid for this function, not a meta-call");
				}
				locals[byte.x].tag   = elOBJTOTAG(call->obj->type);
				locals[byte.x].x_obj = call->obj;
			} break;
			case BC_RELOAD: {
				locals[byte.x] = locals[byte.y];
			} break;
			/* todo: prob cache 'globals->values' as globals
			instead, but then we'd have to sync after every
			call point or table field set... */
			case BC_LOADGLOBAL: {
				locals[byte.x] = globals->values[byte.y];
			} break;
			case BC_SETGLOBAL: {
				globals->values[byte.x] = locals[byte.y];
			} break;
			case BC_LOADNIL: {
				locals[byte.x].tag    = TAG_NIL;
				locals[byte.x].x_int  = 0;
			} break;
			case BC_LOADINT: {
				locals[byte.x].tag   = TAG_INT;
				locals[byte.x].x_int = M->integers[byte.y];
			} break;
			case BC_LOADNUM: {
				locals[byte.x].tag   = TAG_NUM;
				locals[byte.x].x_num = M->numbers[byte.y];
			} break;
			case BC_LOADCACHE: {
				elASSERT(elWITHIN(byte.y,0,proto.nvalues));
				locals[byte.x] = values[byte.y];
			} break;
			case BC_CLOSURE: {
				elASSERT(elWITHIN(byte.y,0,ARRAY_LENGTH(M->functions)));
				elFileProto proto = M->functions[byte.y];
				elClosure *new_cls = elf_new_closure(R,proto);
				elf_copy_memory(new_cls->values,locals+byte.x,proto.nvalues*sizeof(elValue));
				locals[byte.x].tag   = TAG_CLS;
				locals[byte.x].x_cls = new_cls;
			} break;
			case BC_TABLE: {
				/* Quick Note:
				I had this bug once that took me a while to find...
				The problem was that I was doing this:

				#line 0 locals[byte.x].tag = TAG_TAB;
				#line 1 locals[byte.x].x_tab = elf_new_table(R);

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
				locals[byte.x].tag   = TAG_TAB;
				locals[byte.x].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				elf_type_check(R,module_instr,byte.x,byte.y,locals[byte.x].tag);
			} break;
			case BC_METAFIELD: {
				elValue yy = locals[byte.y];
				elTable *metatable = {0};
				switch (yy.tag) {
					case TAG_STR: case TAG_TAB:
					case TAG_OBJ: case TAG_CLS: {
						metatable = yy.x_obj->metatable;
					} goto _lookup;
					case TAG_NUM: {
						metatable = R->metatables.number;
						elf_throw(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case TAG_INT: {
						metatable = R->metatables.integer;
						elf_throw(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					default: {
						elf_throw(R,module_instr,elf_tpf("'%s': not an object", tag2s[yy.tag]));
					} break;
				}
				_lookup:
				if (metatable == 0) {
					elf_throw(R,module_instr,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
				}
				locals[byte.x] = elf_table_lookup(metatable,locals[byte.z]);
			} break;
			/* todo: why are these two so similar ... */
			case BC_INDEX: case BC_FIELD: {
				elValue xx = locals[byte.y];
				elValue yy = locals[byte.z];
				if (yy.tag == TAG_NIL) {
					/* I don't know whether this worth throwing an error
					over, but we can't have nil keys, therefore nil fields
					are impossible, and most of the time I do like the
					interpreter complaining about this because I have never
					actually wanted to access a nil field... */
					elf_throw(R,module_instr,"attempted to get nil field");
				} else if (xx.tag == TAG_TAB) {
					locals[byte.x] = elf_table_lookup(xx.x_tab,yy);
				} else if (xx.tag == TAG_OBJ) {
					elf_call_overload(R,xx.x_obj,R->cache.__getfield,byte.x,1,&yy);
				} else if (xx.tag == TAG_STR) {
					// todo: allow for strings to find substrings,
					// and return the index of the substring!
					// for instance, "my name is"["name"].
					elf_type_check(R,module_instr,0,TAG_INT,yy.tag);
					locals[byte.x].tag   = TAG_INT;
					locals[byte.x].x_int = locals[byte.y].x_str->contents[locals[byte.z].i];
				} else if (xx.tag == TAG_NIL) {
					elf_throw(R,module_instr,"attempted to get field of 'nil' value");
				} else elf_throw(R,module_instr,"invalid object to perform this operator on");
			} break;
			case BC_SETINDEX: case BC_SETFIELD: {
				elValue xx = locals[byte.x];
				elValue yy = locals[byte.y];
				elValue zz = locals[byte.z];
				if (xx.tag == TAG_TAB) {
					elf_table_set(xx.x_tab,yy,zz);
				} else if (xx.tag == TAG_OBJ) {
					elValue args[] = { yy, zz };
					elf_call_overload(R,xx.x_obj,R->cache.__setfield,byte.x,2,args);
				} else {
					elf_throw(R,module_instr,elf_tpf("attempted to set field of '%s' value", tag2s[xx.tag]));
				}
			} break;
			// obj -> cls -> args -> top
			case BC_METACALL: {
				R->byte = module_instr;
				elSETTOP(R,locals+byte.x+byte.y+2);
				elf_call_function3(R,locals[byte.x].x_obj,byte.y,byte.z,-2);
			} break;
			// cls -> args -> top
			case BC_CALL: {
				R->byte = module_instr;
				elSETTOP(R,locals+byte.x+byte.y+1);
				elf_call_function3(R,0,byte.y,byte.z,-1);
			} break;
			case BC_ISNIL: {
				elValue x = locals[byte.y];
				elBool nan = x.tag != TAG_INT && x.tag != TAG_NUM;
				locals[byte.x].tag   = TAG_INT;
				locals[byte.x].x_int = x.tag == TAG_NIL || (nan && x.x_int == 0);
			} break;
			case BC_EQ: case BC_NEQ: {
				elValue x = locals[byte.y];
				elValue y = locals[byte.z];
				elBool eq = false;
				if ((x.tag == TAG_NIL) || (y.tag == TAG_NIL)) {
					eq = elISNIL(x) == elISNIL(y);
				} else if ((x.tag == TAG_STR) && (y.tag == TAG_STR)) {
					eq = elf_streq(x.x_str,y.x_str);
				} else if (elISNUMTAG(x.tag) && elISNUMTAG(y.tag)) {
					eq = x.x_int == y.x_int;
				} else {
					eq = (x.tag == y.tag) && (x.x_int == y.x_int);
				}
				if (byte.k == BC_NEQ) {
					eq = !eq;
				}
				locals[byte.x].tag = TAG_INT;
				locals[byte.x].i   = eq;
			} break;
			case BC_POW: {
				elValue xx = locals[byte.y];
				elValue yy = locals[byte.z];
				if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {
					elNOCODE;
				} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_throw(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[byte.x].tag   = TAG_NUM;
					locals[byte.x].x_num = pow(elTONUM(xx),elTONUM(yy));
				} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_throw(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[byte.x].tag   = TAG_INT;
					locals[byte.x].x_int = pow(elTOINT(xx),elTOINT(yy));
				} else {
					elf_throw(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));
				}
			} break;
			case BC_MOD: {
				elValue xx = locals[byte.y];
				elValue yy = locals[byte.z];
				if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {
					elNOCODE;
				} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_throw(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					elf_check_division_by_zero(R,xx,yy);
					elNumber x = elTONUM(xx);
					elNumber y = elTONUM(yy);
					locals[byte.x].tag   = TAG_NUM;
					locals[byte.x].x_num = x - (elInteger)(x / y) * y;
				} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_throw(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}

					elf_check_division_by_zero(R,xx,yy);

					locals[byte.x].tag   = TAG_INT;
					locals[byte.x].x_int = elTOINT(xx) % elTOINT(yy);
				} else {
					elf_throw(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));
				}
			} break;
	/* todo: make this better */
	#define CASE_IBOP(OPNAME,OP) \
			case OPNAME : {\
				elValue xx = locals[byte.y];\
				elValue yy = locals[byte.z];\
				if (((byte.k == BC_DIV) || (byte.k == BC_MOD))) {\
					elf_check_division_by_zero(R,xx,yy);\
				}\
				locals[byte.x].tag = TAG_INT;\
				locals[byte.x].x_int = elTOINT(xx) OP elTOINT(yy);\
			} break
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
			case OPCODE : {\
				elValue xx = locals[byte.y];\
				elValue yy = locals[byte.z];\
				if (elISOBJTAG(xx.tag) || elISOBJTAG(yy.tag)) {\
					if (!elISOBJTAG(xx.tag)) elf_throw(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
			/* Could we redefine this? */ \
					if (byte.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					elf_call_overload(R,xx.x_obj,elISOBJTAG(yy.tag)?FN:FN1,byte.x,1,&yy);\
				} else if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {\
					if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			/* Could we redefine this? */ \
					if (byte.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					locals[byte.x].tag = TAG_NUM;\
					locals[byte.x].x_num = elTONUM(xx) OP elTONUM(yy);\
				} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {\
					if (!elISNUMTAG(yy.tag)) elf_throw(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
			/* Could we redefine this? */ \
					if (byte.k == BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					locals[byte.x].tag = TAG_INT;\
					locals[byte.x].x_int = elTOINT(xx) OP elTOINT(yy);\
				} else elf_throw(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],elTOTEXT(OP)));\
			} break
			case BC_LTEQ: {
				elValue xx = locals[byte.y];
				elValue yy = locals[byte.z];
				if ((xx.tag == TAG_NUM) || (yy.tag == TAG_NUM)) {
					locals[byte.x].tag = TAG_INT;
					locals[byte.x].x_int = elTONUM(xx) <= elTONUM(yy);
				} else {
					locals[byte.x].tag = TAG_INT;
					locals[byte.x].x_int = elTOINT(xx) <= elTOINT(yy);
				}
			} break;
			case BC_LT: {
				elValue xx = locals[byte.y];
				elValue yy = locals[byte.z];
				if (xx.tag == TAG_NUM || yy.tag == TAG_NUM) {
					locals[byte.x].tag = TAG_INT;
					locals[byte.x].x_int = elTONUM(xx) < elTONUM(yy);
				} else {
					locals[byte.x].tag = TAG_INT;
					locals[byte.x].x_int = elTOINT(xx) < elTOINT(yy);
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

