/*
** See Copyright Notice In elf.h
** runtime.c
*/


elObject *elGETTHIS(elState *S) {
	return elGETLOCAL(S,0).x_obj;
}
int elGETNARGS(elState *S) {
	return (elGETFRAME(S)->nargs-1);
}
elValue elGETARG(elState *S, int X) {
	return (elGETLOCAL(S,X+1));
}
void elf_put_nil(elState *S) {
	elPUSH(S,elNIL());
}
void elf_put_closure(elState *S, elClosure *V) {
	elPUSH(S,elCLS(V));
}
void elf_put_object(elState *S, elObject *V) {
	elPUSH(S,elOBJ(V));
}
void elf_put_cfunction(elState *S, elCFunction V) {
	elPUSH(S,elCFN(V));
}
void elf_put_table(elState *S, elTable *V) {
	elPUSH(S,elTAB(V));
}
void elf_put_integer(elState *S, elInteger V) {
	elPUSH(S,elINT(V));
}
void elf_put_number(elState *S, elNumber V) {
	elPUSH(S,elNUM(V));
}
void elf_put_string(elState *S, elString *V) {
	elPUSH(S,elSTR(V));
}
void elf_put_handle(elState *S, elHandle V) {
	elPUSH(S,elSYS(V));
}


void elf_begin(elState *R, elModule *M) {
	R->M = M;
	R->stack_max = elDEFAULT_STACK_SIZE;
	R->stack = elf_calloc(HEAP_ALLOCATOR,sizeof(elValue)*R->stack_max);
	R->stack_ptr = R->stack;
	R->nframe = 0;
	R->metatables.string = elf_new_string_metatable(R);
	R->metatables.table  = elf_new_table_metatable(R);
	/* global table is on the stack, so we don't really
	to need treat it independently when GC'ing... */
	M->globals = elf_put_new_table(R);
	M->strings = elf_put_new_table(R);
	elf_lib_load_functions(R);
}


int elf_call_function(elState *R, int nargs, int nregs) {
	int nyield=0;
	/* todo: do not make this recursive dude! */
	elStackFrame F = {0};
	F.caller = R->frame;
	F.origin	= R->byte;
	F.locals = R->stack_ptr - nargs;
	F.nargs = nargs;
	F.nregs = nregs;
	elValue fn = F.locals[-1];

	if (!elISFUNTAG(fn.tag)) {
		elf_fail(R,NO_BYTE,elf_tpf("cannot call '%s'", tag2s[fn.tag]));
	}

	if (fn.tag==TAG_CLS) {
		F.closure=fn.x_cls;
		F.nlocals=fn.x_cls->proto.nlocals;
		elf_clear_memory(F.locals+F.nargs,(F.nlocals-F.nargs)*sizeof(elValue));
		elSETTOP(R,F.locals+F.nlocals);
	} else {
		elSETTOP(R,F.locals+F.nargs);
	}

	elGETFRAME(R) = &F;
	R->nframe ++;

	if (R->flags & FLAG_DEBUGGER_ONCALL) {
		elf_debugger("debugger 'FLAG_DEBUGGER_ONCALL'");
	}
	elValue *results,*ptr;
	if (fn.tag == TAG_CLS) {
		nyield=elf_run(R);
	} else if (fn.tag == TAG_CFN) {
		results=R->stack_ptr;
		nyield=fn.x_cfn(R);
		ptr=R->stack_ptr;
		if ((ptr-results) < nyield) {
			elf_fail(R,NO_BYTE,elf_tpf("number of values on stack '%i', is incoherent with specified number of yielded values '%i'",(int)(ptr - results),nyield));
		}
		// if ((nyield==0)&&(nregs>0)){
		// 	elf_debug_log("seems like this function does not return anything");
		// }
		/* todo: clear only the part we didn't write */
		elf_clear_memory(F.locals-1,nregs*sizeof(elValue));
		elf_copy_memory(F.locals-1,results,MIN(nyield,nregs)*sizeof(elValue));
	} else {
		nyield = -1;
		elf_fail(R,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[fn.tag]));
	}
	elGETFRAME(R) = F.caller;
	R->nframe -= 1;
	ASSERT(R->nframe > -1);
	return nyield;
}


int elf_parse_expr3_fs(elState *R, FileState *fs, elString *filename, elRegId rxy, int ny, elString *contents) {
#if 0
	NodeId id;
	id=parse_expr(fs,0,0);
	emit_yield(fs,fs->this_token.line,id);
#endif
	NO_CODE;
	return -1;
}


elClosure *elf_fs_load_code(elState *R, FileState *fs, elString *filename, int nargs, elString *contents) {
	if ((filename == 0) || (contents == 0)) {
		return 0;
	}
	elModule *M = R->M;
	fs->R = R;
	fs->M = M;
	begin_file_state(fs,filename->text,contents->text);
	while (parse_stat(fs));
	close_file_state(fs);

	elFunction fp = {0};
	fp.name     = filename;
	fp.contents = contents;
	fp.bytes    = fs->function.bytes;
	fp.nbytes   = M->nbytes - fs->function.bytes;
	fp.nlocals  = fs->function.nlocals;
	ARRAY_ADD(M->files,fp);
	elClosure *cls = elf_new_closure(R,fp);
	/* todo: hack! */
	elf_tadd(M->globals,elCLS(cls));
	return cls;
}


/* todo: rename to call file or something... */
int elf_fs_load_file(elState *R, FileState *fs, elString *name, int nargs, int nregs) {

	elError error;
	elString *string;
	elClosure *cls;
	char *text;

	/* todo: please instead allocate the string and read
	the file into it... */
	error=sys_load_file_data(HEAP_ALLOCATOR,(void**)&text,name->text);
	if (FAILED(error)) {
		elf_error_log("'%s': could not load file",name->text);
		return -1;
	} else {
		elf_debug_log("'%s': file loaded successfully",name->text);
	}

	string=elf_put_new_string(R,text);
	elf_dealloc(HEAP_ALLOCATOR,text);

	cls=elf_fs_load_code(R,fs,name,nargs,string);
	elf_put_closure(R,cls);
	return elf_call_function(R,0,nregs);
}


int elf_load_file(elState *R, elString *name, int nargs, int nregs) {
	FileState fs = {0};
	return elf_fs_load_file(R,&fs,name,nargs,nregs);
}


void elf_check_division_by_zero(elState *S, elValue xx, elValue yy) {
	if ((yy.tag == TAG_NUM) && (yy.x_num == 0.)) elf_fail(S,NO_BYTE,"division by zero"); else
	if ((yy.tag == TAG_INT) && (yy.x_int == 0)) elf_fail(S,NO_BYTE,"integer division by zero");
}


/* todo: remove this, instead make a function that simply gets the value
and checks that is is a function... */
int elf_Scalloverload(elState *S, elObject *obj, elString *name, int reg, int nargs, elValue *args) {
	if (obj->metatable == 0) {
		elf_fail(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}
	elValue field;
	field=elf_tgets_any(obj->metatable,name);
	if (!elISFUNTAG(field.tag)) {
		elf_fail(S,NO_BYTE,elf_tpf("'%s': overload is %s, not a function",name->text,tag2s[field.tag]));
	}

	/* the function, the object, and the arguments */
	elValue *top;
	top=elGETTOP(S);
	elPUSH(S,field);
	elPUSH(S,elOBJ(obj));
	elf_copy_memory(elGETTOP(S),args,sizeof(elValue)*nargs);
	elGETTOP(S) += nargs;
	int ny = elf_call_function(S,nargs+1,1);
	elGETLOCAL(S,reg)=*top;
	return ny;
}


/* returns number of objects uniquely marked */
elInteger elf_mark_object(elObject *obj) {
	ASSERT(obj != 0);
	ASSERT(obj->color != GC_RED);
	/* black object simply means it was
	already marked and we found another
	path to it, since the object was
	already accounted for, return 0 */
	if (obj->color == GC_BLACK) {
		return 0;
	}
	elInteger num = 1;
	/* Here we check whether the object is explicitly white,
	because there are other colors that we don't want to get
	rid of.
	I suppose we don't propagate pink because if
	the object were to ever change color we'd have to also
	propagate those changes...
	Pink objects are rare though...
	And I think they are soon to be deprecated...
	Trap objects are for debugging only, they are meant to
	trigger a GC fault if not marked... */
	if ((elOBJCOLOR(obj) == GC_WHITE) || (elOBJCOLOR(obj) == GC_TRAP)) {
		elOBJCOLOR(obj) = GC_BLACK;
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
	ASSERT(R->memory.phase == elGC_PHASE_HOLD);
	R->memory.phase ^= 1;

	elInteger Ni = 0;
	elValue *Ki;
	for (Ki = R->stack; Ki < elGETTOP(R); ++ Ki) {
		if (elISOBJTAG(Ki->tag)) {
			Ni += elf_mark_object(Ki->x_obj);
		}
	}
	return Ni;
}


elInteger elf_free_phase(elState *R) {
	ASSERT(R->memory.phase == elGC_PHASE_FREE);
	R->memory.phase ^= 1;


	elObject **new_objects = R->memory.new_objects;
	elObject **objects = R->memory.objects;
	if (new_objects) {
		ARRAY(new_objects).min = 0;
	}
	elInteger n = 0;
	FOR_ARRAY(i,objects) {
		elObject *it = objects[i];
		ASSERT(it != 0);
		if ((elOBJCOLOR(it) == GC_RED) || (elOBJCOLOR(it) == GC_TRAP)) {
#if 0
			for(elValue *Ki = R->stack; Ki < R->T; Ki += 1) {
				if (Ki->x_obj == it) {
					elf_debug_log("Object '%p' found in stack at: '%p'. From top '%p' -> %lli", it, Ki, R->T, (R->T - Ki));
				}
			}
#endif
			elf_fail(R,it->byte,elf_tpf("internal error, GC failed, attempted to collect object '%p'", it));
		}
		if (elOBJCOLOR(it) == GC_BLACK) {
			elOBJCOLOR(it) = GC_WHITE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (elOBJCOLOR(it) == GC_WHITE) {
			n += 1;
			elOBJCOLOR(it) = GC_RED;
			R->memory.memory_allocated -= it->tell;
			if (it->type == GC_STR && ((elString*)(it))->length > 512) {
				elf_debug_log("deallocated fairly large string: %p, %i", it, ((elString*)(it))->length);
			}
			if (it->type == GC_TAB) {
				elf_dealloc_table((elTable*)it);
			}
			elf_dealloc(HEAP_ALLOCATOR,it);
		} else n += 1;
	}
	R->memory.objects = new_objects;
	R->memory.new_objects = objects;
	return n;
}


elInteger elf_trigger_collection_cycle(elState *R) {
	elInteger time_, num_marked, num_objects, num_to_collect, obj_trigger_threshold, num_collected;

	time_ = elf_get_clock_time();
	num_marked = elf_hold_phase(R);
	num_objects = ARRAY_LENGTH(R->memory.objects);
	num_to_collect = num_objects - num_marked;
	obj_trigger_threshold = R->collector.object_trigger_threshold;

	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	num_collected = elf_free_phase(R);
	num_to_collect -= num_collected;

	// elf_debug_log("	(%f) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
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

	elInteger num_objects, obj_threshold, num_collected;

	num_objects = ARRAY_LENGTH(R->memory.objects);
	obj_threshold = R->collector.object_trigger_threshold;

	if (num_objects > obj_threshold) {
		num_collected = elf_trigger_collection_cycle(R);
		ASSERT(num_collected <= num_objects);
		R->collector.object_trigger_threshold += elGC_OBJ_THRESHOLD_MIN - num_collected;
	} else if (R->collector.memory_allocated > R->collector.memory_threshold) {
		R->collector.memory_threshold <<= 1;
		if (R->collector.memory_threshold > elGC_MEM_THRESHOLD_MAX) {
			R->collector.memory_threshold = elGC_MEM_THRESHOLD_MAX;
		}
		elf_trigger_collection_cycle(R);
		if (R->collector.memory_allocated > R->collector.memory_threshold) {
			elf_fail(R,NO_BYTE,elf_tpf("out of memory, %lliMB allocated",R->collector.memory_allocated / MEGABYTES(1)));
		}
	}
}


void *elf_new_object(elState *R, elGCTy type, elInteger tell) {
	if (R->collector.phase!=GC_WHITE) {
		elf_fail(R,NO_BYTE,"object allocation out of phase");
	}

	R->collector.memory_allocated += tell;
	elf_collect(R);

	elObject *obj;

	obj=elf_calloc(HEAP_ALLOCATOR,tell);
	obj->color=R->collector.phase;
	obj->type=type;
	obj->tell=tell;
	obj->byte=R->byte;
	ARRAY_ADD(R->collector.objects,obj);
	return obj;
}


int elf_run(elState *R) {

	elModule *M;
	elTable *globals;
	elStackFrame *F;
	elValue *locals;
	elFunction proto;
	elValue *values;
	elInteger module_instr,instr,next_instr;
	Bytecode byte;
	elf_delaylist *delay;
	/* operands .(z,y) */
	elValue xx,yy;


	M       = R->M;
	globals = M->globals;
 	F       = R->frame;
 	locals  = F->locals;
	proto   = F->closure->proto;
	values  = F->closure->values;


	next_instr = 0;

	while (next_instr < proto.nbytes) {
		instr = next_instr ++;
		module_instr = proto.bytes + instr;
		byte = M->bytes[module_instr];
		/* todo: eventually, maybe not do this per instruction */
		R->byte = module_instr;

#if defined(_DEBUG)
		if (R->flags & FLAG_BYTELOGGING || F->logging) {
			fpf_byte(stdout,M,-1,instr,byte);
		}
		if (R->flags & FLAG_DEBUGGER) {
			elf_debugger("debugger 'FLAG_DEBUGGER'");
		}
#endif

#if defined(ELF_EXPERIMENTAL_FEATURES)
		if (R->bytetracking) {
			elInteger track = ++ M->track[module_instr];
			if (track == 64) {
				elFunction file;
				char *line;
				int linenum;
				file=M->files[elf_get_instr_file(M,module_instr)];
				line=M->lines[module_instr];
				elf_get_line_source_info(file.lines,line,&linenum,0);
				elf_debug_log("%s %i: %lli: %lli detected hot path",file.name,linenum,module_instr,track);
			}
		}
#endif
		switch (BC_OP(byte)) {
			case BC_LEAVE: {
				delay = F->delay_list;
				if (delay != 0) {
					next_instr    = delay->j;
					F->delay_list = delay->n;
					elf_dealloc(HEAP_ALLOCATOR,delay);
				} else goto esc;
			} break;
			case BC_DELAY: {
				ASSERT(BC_ARGX(byte) >= 0);

				/* todo: make this better??? */
				delay = elf_alloc(HEAP_ALLOCATOR,sizeof(elf_delaylist));
				delay->n = F->delay_list;
				delay->j = next_instr;
				F->delay_list = delay;

				next_instr = instr + BC_ARGX(byte);
			} break;
			case BC_YIELD: {
				ASSERT(BC_ARGX(byte) >= 0);
				int reg,nregs;
				nregs = MIN(BC_ARGZ(byte),F->nregs);
				for (reg=0; reg<nregs; ++reg) {
					locals[reg-1] = locals[reg+BC_ARGY(byte)];
				}
				next_instr = instr+BC_ARGX(byte);

				F->nregs = nregs;
			} break;
			case BC_NOP: {
			} break;
			case BC_LOOP: {
			} break;
			case BC_J: {
				next_instr = instr+BC_ARGX(byte);
			} break;
			case BC_JZ: {
				if (locals[BC_ARGY(byte)].x_int == 0) {
					next_instr = instr + BC_ARGX(byte);
				}
			} break;
			case BC_JNZ: {
				if (locals[BC_ARGY(byte)].x_int != 0) {
					next_instr = instr + BC_ARGX(byte);
				}
			} break;
			case BC_RELOAD: {
				locals[BC_ARGX(byte)] = locals[BC_ARGY(byte)];
			} break;
			/* todo: prob cache 'globals->values' as globals
			instead... */
			case BC_GETGLOBAL: {
				locals[BC_ARGX(byte)] = globals->values[BC_ARGY(byte)];
			} break;
			case BC_SETGLOBAL: {
				globals->values[BC_ARGX(byte)] = locals[BC_ARGY(byte)];
			} break;
			case BC_LOADNIL: {
				locals[BC_ARGX(byte)].tag    = TAG_NIL;
				locals[BC_ARGX(byte)].x_int  = 0;
			} break;
			case BC_GETKINT: {
				locals[BC_ARGX(byte)].tag   = TAG_INT;
				locals[BC_ARGX(byte)].x_int = M->integers[BC_ARGY(byte)];
			} break;
			case BC_GETKNUM: {
				locals[BC_ARGX(byte)].tag   = TAG_NUM;
				locals[BC_ARGX(byte)].x_num = M->numbers[BC_ARGY(byte)];
			} break;
			case BC_GETCLOSED: {
				ASSERT(WITHIN(BC_ARGY(byte),0,proto.nvalues));
				locals[BC_ARGX(byte)] = values[BC_ARGY(byte)];
			} break;
			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte),0,ARRAY_LENGTH(M->functions)));
				elFunction proto = M->functions[BC_ARGY(byte)];
				elClosure *new_cls = elf_new_closure(R,proto);
				elf_copy_memory(new_cls->values,locals+BC_ARGX(byte),proto.nvalues*sizeof(elValue));
				locals[BC_ARGX(byte)].tag   = TAG_CLS;
				locals[BC_ARGX(byte)].x_cls = new_cls;
			} break;
			case BC_TABLE: {
				/* Quick Note:
				I had this bug once that took me a while to find...
				The problem was that I was doing this:

				#line 0
				locals[BC_ARGX(byte)].tag = TAG_TAB;
				#line 1
				locals[BC_ARGX(byte)].x_tab = elf_new_table(R);

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
				locals[BC_ARGX(byte)].tag   = TAG_TAB;
				locals[BC_ARGX(byte)].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				elf_type_check(R,module_instr,BC_ARGX(byte),BC_ARGY(byte),locals[BC_ARGX(byte)].tag);
			} break;
			case BC_GETMETAFIELD: {
				elValue yy = locals[BC_ARGY(byte)];
				elTable *metatable = {0};
				switch (yy.tag) {
					case TAG_STR: case TAG_TAB:
					case TAG_OBJ: case TAG_CLS: {
						metatable = yy.x_obj->metatable;
					} goto _lookup;
					case TAG_NUM: {
						metatable = R->metatables.number;
						elf_fail(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case TAG_INT: {
						metatable = R->metatables.integer;
						elf_fail(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					default: {
						elf_fail(R,module_instr,elf_tpf("'%s': not an object", tag2s[yy.tag]));
					} break;
				}
				_lookup:
				if (metatable == 0) {
					elf_fail(R,module_instr,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
				}
				locals[BC_ARGX(byte)] = elf_table_lookup(metatable,locals[BC_ARGZ(byte)]);
			} break;
			/* todo: why are these two identical bro... */
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];

#if 0
				if (BC_OP(byte)==BC_GETFIELD) {
					Bytecode prev;
					prev=M->bytes[module_instr-1];
					if ((prev.k==BC_GETGLOBAL)&&(prev.x==BC_ARGZ(byte))) {
					}
				}
#endif
				if (yy.tag==TAG_NIL) {
					/* I don't know whether this worth throwing an error
					over, it's just my preference and use cases, maybe
					tables can have a default field which gets returned
					when the value is nil?  */
					elf_fail(R,module_instr,"attempted to get nil field");
				} else if (xx.tag==TAG_TAB) {
					locals[BC_ARGX(byte)]=elf_table_lookup(xx.x_tab,yy);
				} else if (xx.tag==TAG_OBJ) {
					elf_Scalloverload(R,xx.x_obj,R->cache.__getfield,BC_ARGX(byte),1,&yy);
				} else if (xx.tag==TAG_STR) {
					elInteger index;
					elString *string;
					/* todo: allow for indexing for substrings,
					for instrance, "my name is"["name"] */
					elf_type_check(R,module_instr,0,TAG_INT,yy.tag);
					string=xx.x_str;
					index=yy.x_int;
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = string->text[index];
				} else if (xx.tag==TAG_NIL) {
					elf_fail(R,module_instr,"attempted to get field of 'nil' value");
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid object '%s' to perform this operator on", tag2s[yy.tag]));
				}
			} break;
			case BC_SETINDEX: case BC_SETFIELD: {
				elValue xx = locals[BC_ARGX(byte)];
				elValue yy = locals[BC_ARGY(byte)];
				elValue zz = locals[BC_ARGZ(byte)];
				if (xx.tag == TAG_TAB) {
					elf_tset(xx.x_tab,yy,zz);
				} else if (xx.tag == TAG_OBJ) {
					elValue args[] = { yy, zz };
					elf_Scalloverload(R,xx.x_obj,R->cache.__setfield,BC_ARGX(byte),2,args);
				} else {
					elf_fail(R,module_instr,elf_tpf("attempted to set field of '%s' value", tag2s[xx.tag]));
				}
			} break;
			case BC_CALL: {
				elValue *T = elGETTOP(R);
				ASSERT(T >= locals+proto.nlocals);
				R->byte = module_instr;
				elSETTOP(R,locals+BC_ARGX(byte)+1+BC_ARGY(byte));
				elf_call_function(R,BC_ARGY(byte),BC_ARGZ(byte));
				elSETTOP(R,locals+proto.nlocals);
			} break;
			case BC_ISNIL: {
				elValue x = locals[BC_ARGY(byte)];
				elBool nan = x.tag != TAG_INT && x.tag != TAG_NUM;
				locals[BC_ARGX(byte)].tag   = TAG_INT;
				locals[BC_ARGX(byte)].x_int = x.tag == TAG_NIL || (nan && x.x_int == 0);
			} break;
			case BC_EQ: case BC_NEQ: {
				elValue x = locals[BC_ARGY(byte)];
				elValue y = locals[BC_ARGZ(byte)];
				elBool eq = 0;
				if ((x.tag == TAG_NIL) || (y.tag == TAG_NIL)) {
					eq = elISNIL(x) == elISNIL(y);
				} else if ((x.tag == TAG_STR) && (y.tag == TAG_STR)) {
					eq = elf_string_eq(x.x_str,y.x_str);
				} else if (elISNUMTAG(x.tag) && elISNUMTAG(y.tag)) {
					eq = x.x_int == y.x_int;
				} else {
					eq = (x.tag == y.tag) && (x.x_int == y.x_int);
				}
				if (BC_OP(byte) == BC_NEQ) {
					eq = !eq;
				}
				locals[BC_ARGX(byte)].tag   = TAG_INT;
				locals[BC_ARGX(byte)].x_int = eq;
			} break;
			case BC_POW: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (elISOBJTAG(xx.tag)||elISOBJTAG(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag==TAG_NUM)||(yy.tag==TAG_NUM)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = TAG_NUM;
					locals[BC_ARGX(byte)].x_num = pow(elTONUM(xx),elTONUM(yy));
				} else if ((xx.tag==TAG_INT)||(yy.tag==TAG_INT)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = pow(elTOINT(xx),elTOINT(yy));
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],TO_TEXT(OP)));
				}
			} break;
			case BC_MOD: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (elISOBJTAG(xx.tag)||elISOBJTAG(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag == TAG_NUM)||(yy.tag == TAG_NUM)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					elf_check_division_by_zero(R,xx,yy);
					elNumber x,y;
					x=elTONUM(xx);
					y=elTONUM(yy);
					locals[BC_ARGX(byte)].tag   = TAG_NUM;
					locals[BC_ARGX(byte)].x_num = x - (elInteger)(x / y) * y;
				} else if ((xx.tag == TAG_INT) || (yy.tag == TAG_INT)) {
					if (!elISNUMTAG(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					elf_check_division_by_zero(R,xx,yy);
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = elTOINT(xx) % elTOINT(yy);
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],TO_TEXT(OP)));
				}
			} break;
	/* todo: make this better */
	#define CASE_IBOP(OPNAME,OP) \
			case OPNAME : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if (((BC_OP(byte) == BC_DIV) || (BC_OP(byte) == BC_MOD))) {\
					elf_check_division_by_zero(R,xx,yy);\
				}\
				locals[BC_ARGX(byte)].tag = TAG_INT;\
				locals[BC_ARGX(byte)].x_int = elTOINT(xx) OP elTOINT(yy);\
			} break
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
			case OPCODE : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if (elISOBJTAG(xx.tag)||elISOBJTAG(yy.tag)) {\
					if (!elISOBJTAG(xx.tag)) elf_fail(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					elf_Scalloverload(R,xx.x_obj,elISOBJTAG(yy.tag)?FN:FN1,BC_ARGX(byte),1,&yy);\
				} else if ((xx.tag==TAG_NUM) || (yy.tag==TAG_NUM)) {\
					if (!elISNUMTAG(yy.tag)) elf_fail(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					locals[BC_ARGX(byte)].tag   = TAG_NUM;\
					locals[BC_ARGX(byte)].x_num = elTONUM(xx) OP elTONUM(yy);\
				} else if ((xx.tag==TAG_INT)||(yy.tag==TAG_INT)) {\
					if (!elISNUMTAG(yy.tag)) elf_fail(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) elf_check_division_by_zero(R,xx,yy);\
					locals[BC_ARGX(byte)].tag   = TAG_INT;\
					locals[BC_ARGX(byte)].x_int = elTOINT(xx) OP elTOINT(yy);\
				} else elf_fail(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],TO_TEXT(OP)));\
			} break
			case BC_LTEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if ((xx.tag==TAG_NUM)||(yy.tag==TAG_NUM)) {
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = elTONUM(xx) <= elTONUM(yy);
				} else {
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = elTOINT(xx) <= elTOINT(yy);
				}
			} break;
			case BC_LT: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if ((xx.tag==TAG_NUM)||(yy.tag==TAG_NUM)) {
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = elTONUM(xx) < elTONUM(yy);
				} else {
					locals[BC_ARGX(byte)].tag   = TAG_INT;
					locals[BC_ARGX(byte)].x_int = elTOINT(xx) < elTOINT(yy);
				}
			} break;
			CASE_IBOP(BC_SHL,  <<);
			CASE_IBOP(BC_SHR,  >>);
			CASE_IBOP(BC_BIT_XOR, ^);
			CASE_IBOP(BC_BIT_AND, &);
			CASE_IBOP(BC_BIT_OR,  |);
			CASE_BOP(BC_ADD, +, R->cache.__add, R->cache.__add1);
			CASE_BOP(BC_SUB, -, R->cache.__sub, R->cache.__sub1);
			CASE_BOP(BC_MUL, *, R->cache.__mul, R->cache.__mul1);
			CASE_BOP(BC_DIV, /, R->cache.__div, R->cache.__div1);
	#undef CASE_BOP
			default: {
				NO_CODE;
			} break;
		}
	}

	esc:
	return F->nregs;
}

