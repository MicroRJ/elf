/*
** See Copyright Notice In elf.h
** shell.c
*/


static int elf_run(elf_Shell *R);


elf_Closure *elf_alloc_closure(elf_Shell *S, elf_Function proto) {
	elf_Closure *cls = (elf_Closure *) elf_alloc_object(S,GC_CLS,sizeof(elf_Closure) + sizeof(elf_Value) * (proto.nlocals-1));
	cls->proto=proto;
	return cls;
}


void elf_begin(elf_Shell *R, elf_Module *M) {
	R->M=M;
	R->stack_max=elDEFAULT_STACK_SIZE;
	R->stack    =calloc_memory(GLOBAL_ALLOCATOR,sizeof(elf_Value)*R->stack_max);
	R->stack_ptr=R->stack;
	R->first_frame.locals=R->stack_ptr;
	R->frame=&R->first_frame;
	R->nframe=0;
	R->metatables.string=elf_new_string_lib(R);
	R->metatables.table =new_table_lib(R);
	M->globals=elf_new_table(R);
	M->strings=elf_new_table(R);
	elf_include_core_lib(R);
}


int elf_call_function(elf_Shell *R, int nargs, int nregs) {
	int nyield=0;
	/* todo: do not make this recursive dude! */
	elf_StackFrame F = {0};
	F.caller=R->frame;
	F.origin=R->byte;
	F.locals=R->stack_ptr - nargs;
	F.nargs=nargs;
	F.nregs=nregs;

	elf_Value fn = F.locals[-1];

	if (!ISFUNT(fn.tag)) {
		elf_fail(R,NO_BYTE,elf_tpf("cannot call '%s'", tag2s[fn.tag]));
	}

	if (fn.tag==elf_TAG_CLS) {
		F.closure=fn.x_cls;
		F.nlocals=fn.x_cls->proto.nlocals;
		clear_memory(F.locals+F.nargs,(F.nlocals-F.nargs)*sizeof(elf_Value));
		SET_TOP(R,F.locals+F.nlocals);
	} else {
		SET_TOP(R,F.locals+F.nargs);
	}

	GET_FRAME(R) = &F;
	R->nframe ++;

	if (R->flags & FLAG_DEBUGGER_ONCALL) {
		elf_debugger("debugger 'FLAG_DEBUGGER_ONCALL'");
	}
	elf_Value *results,*ptr;
	if (fn.tag == elf_TAG_CLS) {
		nyield=elf_run(R);
	} else if (fn.tag == elf_TAG_CFN) {
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
		clear_memory(F.locals-1,nregs*sizeof(elf_Value));
		copy_memory(F.locals-1,results,MIN(nyield,nregs)*sizeof(elf_Value));
	} else {
		nyield = -1;
		elf_fail(R,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[fn.tag]));
	}
	GET_FRAME(R) = F.caller;
	R->nframe -= 1;
	ASSERT(R->nframe > -1);
	return nyield;
}


int elf_parse_expr3_fs(elf_Shell *R, FileState *fs, elf_String *filename, elf_StackId rxy, int ny, elf_String *contents) {
#if 0
	NodeId id;
	id=parse_expr(fs,0,0);
	emit_yield(fs,fs->this_token.line,id);
#endif
	NO_CODE;
	return -1;
}


elf_Closure *elf_load_code_closure(elf_Shell *R, FileState *fs, elf_String *filename, elf_String *contents) {
	if ((filename==0)||(contents==0)) {
		return 0;
	}
	elf_Module *M = R->M;
	fs->R=R;
	fs->M=M;
	begin_file_state(fs,filename->text,contents->text);
	while (parse_stat(fs));
	close_file_state(fs);

	elf_Function fp = {0};
	fp.name=filename;
	fp.contents=contents;
	fp.bytes=fs->function.bytes;
	fp.nbytes=M->nbytes-fs->function.bytes;
	fp.nlocals=fs->function.nlocals;
	ARRAY_ADD(M->files,fp);
	return elf_new_closure(R,fp);
}


elf_Closure *elf_load_file_closure(elf_Shell *R, FileState *fs, elf_String *name) {
	Error error;
	elf_String *string;
	char *text;

	/* todo: use string allocator instead?... */
	error=sys_load_file_data(GLOBAL_ALLOCATOR,(void**)&text,name->text);
	if (FAILED(error)) {
		elf_error_log("'%s': could not load file",name->text);
		goto error;
	}

	elf_debug_log("'%s': file loaded successfully",name->text);

	string=elf_new_string(R,text);
	dealloc_memory(GLOBAL_ALLOCATOR,text);
	return elf_load_code_closure(R,fs,name,string);
	error:
	return 0;
}


int elf_exec_file(elf_Shell *R, elf_String *name, int nargs, int nregs) {
	FileState fs = {0};
	elf_Closure *cls;

	cls=elf_load_file_closure(R,&fs,name);
	/* todo: HACK! */
	elf_gsets(R->M,name,VCLS(cls));
	elf_add_cls(R,cls);
	elf_add_this(R);
	return elf_call_function(R,2,nregs);
}


INTERNAL void check_division_by_zero(elf_Shell *S, elf_Value xx, elf_Value yy) {
	if ((yy.tag == elf_TAG_NUM) && (yy.x_num == 0.)) elf_fail(S,NO_BYTE,"division by zero"); else
	if ((yy.tag == elf_TAG_INT) && (yy.x_int == 0)) elf_fail(S,NO_BYTE,"integer division by zero");
}


static int call_overload(elf_Shell *S, elf_Object *obj, char const *name, int reg, int nargs, elf_Value *args) {
	if (obj->metatable == 0) {
		elf_fail(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}
	elf_Value field;
	field=elf_tgetx_any(obj->metatable,name);
	if (!ISFUNT(field.tag)) {
		elf_fail(S,NO_BYTE,elf_tpf("'%s': overload is %s, not a function",name,tag2s[field.tag]));
	}
	elf_Value *top;
	top=GET_TOP(S);
	PUSHV(S,field);
	PUSHV(S,VOBJ(obj));
	copy_memory(GET_TOP(S),args,sizeof(elf_Value)*nargs);
	GET_TOP(S) += nargs;
	int ny = elf_call_function(S,nargs+1,1);
	GET_LOCAL(S,reg)=*top;
	return ny;
}


/* returns number of objects uniquely marked */
elf_Int elf_mark_object(elf_Object *obj) {
	ASSERT(obj != 0);
	ASSERT(obj->color != elf_GC_RED);
	/* black object simply means it was
	already marked and we found another
	path to it, since the object was
	already accounted for, return 0 */
	if (obj->color == elf_GC_BLACK) {
		return 0;
	}
	elf_Int num = 1;
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
	if ((OBJ_COLOR(obj) == elf_GC_WHITE) || (OBJ_COLOR(obj) == elf_GC_TRAP)) {
		OBJ_COLOR(obj) = elf_GC_BLACK;
	}
	if (obj->metatable) {
		num += elf_mark_object((elf_Object*)obj->metatable);
	}
	if (obj->type == GC_CLS) {
		elf_Closure *cls = (elf_Closure*) obj;
		if (cls->proto.name != 0) {
			elf_mark_object(POBJ(cls->proto.name));
		}
		if (cls->proto.contents != 0) {
			elf_mark_object(POBJ(cls->proto.contents));
		}
		if (cls->proto.parent != -1) {
			/* todo: implement this */
		}
		FOR_RANGE(i, 0, cls->proto.nlocals) {
			if (ISOBJT(cls->values[i].tag)) {
				num += elf_mark_object(cls->values[i].x_obj);
			}
		}
	} else if (obj->type == GC_TAB) {
		elf_Table *table;
		elf_Value *array;
		elf_Entry *slots;

		table=(elf_Table*)obj;
		array=table->array;
		slots=table->slots;

		FOR_RANGE(k,0,table->ntotal) {
			if (ISOBJT(slots[k].key.tag)) {
				num += elf_mark_object(slots[k].key.x_obj);
			}
		}
		FOR_RANGE(k,0,ARRAY_LENGTH(array)) {
			if (ISOBJT(array[k].tag)) {
				num += elf_mark_object(array[k].x_obj);
			}
		}
	}
	return num;
}


elf_Int elf_hold_phase(elf_Shell *R) {
	ASSERT(R->collector.phase == elf_GC_PHASE_MARK);
	R->collector.phase ^= 1;

	elf_Int Ni = 0;
	elf_Value *Ki;
	for (Ki = R->stack; Ki < GET_TOP(R); ++ Ki) {
		if (ISOBJT(Ki->tag)) {
			Ni += elf_mark_object(Ki->x_obj);
		}
	}
	return Ni;
}


elf_Int elf_free_phase(elf_Shell *R) {
	ASSERT(R->collector.phase==elf_GC_PHASE_FREE);
	R->collector.phase^=1;


	elf_Object **new_objects=R->collector.new_objects;
	elf_Object **objects=R->collector.objects;
	if (new_objects) {
		ARRAY_SET_MIN(new_objects,0);
	}
	elf_Int n = 0;
	FOR_ARRAY(i,objects) {
		elf_Object *it = objects[i];
		ASSERT(it != 0);
		if ((OBJ_COLOR(it) == elf_GC_RED) || (OBJ_COLOR(it) == elf_GC_TRAP)) {
#if 0
			for(elf_Value *Ki = R->stack; Ki < R->T; Ki += 1) {
				if (Ki->x_obj == it) {
					elf_debug_log("Object '%p' found in stack at: '%p'. From top '%p' -> %lli", it, Ki, R->T, (R->T - Ki));
				}
			}
			elf_fail(R,it->byte,elf_tpf("internal error, GC failed, attempted to collect object '%p'", it));
#endif
		}
		if (OBJ_COLOR(it) == elf_GC_BLACK) {
			OBJ_COLOR(it) = elf_GC_WHITE;
			/* todo: instead simply ensure 'new_objects' is big enough */
			ARRAY_ADD(new_objects,it);
		} else if (OBJ_COLOR(it) == elf_GC_WHITE) {
			n += 1;
			OBJ_COLOR(it) = elf_GC_RED;
			R->collector.memory_allocated -= it->size;
			if (it->type == GC_TAB) {
				elf_free_table_contents((elf_Table*)it);
			}
			dealloc_memory(GLOBAL_ALLOCATOR,it);
		} else n += 1;
	}
	R->collector.objects = new_objects;
	R->collector.new_objects = objects;
	return n;
}


elf_Int elf_trigger_collection_cycle(elf_Shell *R) {
	elf_Int time_, num_marked, num_objects, num_to_collect, obj_trigger_threshold, num_collected;

	time_ = elf_get_clock_time();
	num_marked = elf_hold_phase(R);
	num_objects = ARRAY_LENGTH(R->collector.objects);
	num_to_collect = num_objects - num_marked;
	obj_trigger_threshold = R->collector.object_trigger_threshold;

	// elf_debug_log("GC: %lli - %lli -> %lli (%lli), (total - marked = expected) (threshold)",num_objects,num_to_collect,num_marked,obj_trigger_threshold);

	num_collected = elf_free_phase(R);
	num_to_collect -= num_collected;

	// elf_debug_log("	(%f) => leaked: %lli", elf_time_diff_ms(time_),num_to_collect);
	return num_collected;
}


void elf_collect(elf_Shell *R) {
	if (R->collector.paused) {
		return;
	}
	if (R->collector.memory_threshold <= 0) {
		R->collector.memory_threshold = elGC_MEM_THRESHOLD_MIN;
	}
	if (R->collector.object_trigger_threshold <= 0) {
		R->collector.object_trigger_threshold = elGC_OBJ_THRESHOLD_MIN;
	}

	elf_Int num_objects, obj_threshold, num_collected;

	num_objects = ARRAY_LENGTH(R->collector.objects);
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


void *elf_alloc_object(elf_Shell *R, elf_GCTy type, elf_Int size) {
	if (R->collector.phase!=elf_GC_WHITE) {
		elf_fail(R,NO_BYTE,"object allocation out of phase");
	}

	R->collector.memory_allocated+=size;
	elf_collect(R);

	elf_Object *obj;

	obj=calloc_memory(GLOBAL_ALLOCATOR,size);
	obj->color=R->collector.phase;
	obj->type=type;
	obj->size=size;
	// obj->byte=R->byte;
	ARRAY_ADD(R->collector.objects,obj);
	return obj;
}


int elf_run(elf_Shell *R) {

	elf_Module *M;
	elf_Table *globals;
	elf_StackFrame *F;
	elf_Value *locals;
	elf_Function proto;
	elf_Value *values;
	elf_Int module_instr,instr,next_instr;
	elf_Bytecode byte;
	delaylist *delay;
	/* operands .(z,y) */
	elf_Value xx,yy;


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
		ASSERT(GET_TOP(R)>=locals+proto.nlocals);

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
			elf_Int track = ++ M->track[module_instr];
			if (track == 64) {
				elf_Function file;
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
			case BC_NOP: {
			} break;
			case BC_LEAVE: {
				delay = F->delay_list;
				if (delay != 0) {
					next_instr    = delay->j;
					F->delay_list = delay->n;
					dealloc_memory(GLOBAL_ALLOCATOR,delay);
				} else goto esc;
			} break;
			case BC_DELAY: {
				ASSERT(BC_ARGX(byte) >= 0);

				/* todo: make this better??? */
				delay = alloc_memory(GLOBAL_ALLOCATOR,sizeof(delaylist));
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
			/* todo: prob cache 'globals->array' as globals
			instead... */
			case BC_GETGLOBAL: {
				locals[BC_ARGX(byte)]=globals->array[BC_ARGY(byte)];
			} break;
			case BC_SETGLOBAL: {
				globals->array[BC_ARGX(byte)]=locals[BC_ARGY(byte)];
			} break;
			case BC_LOADNIL: {
				locals[BC_ARGX(byte)].tag    = elf_TAG_NIL;
				locals[BC_ARGX(byte)].x_int  = 0;
			} break;
			case BC_GETKINT: {
				locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
				locals[BC_ARGX(byte)].x_int = M->integers[BC_ARGY(byte)];
			} break;
			case BC_GETKNUM: {
				locals[BC_ARGX(byte)].tag   = elf_TAG_NUM;
				locals[BC_ARGX(byte)].x_num = M->numbers[BC_ARGY(byte)];
			} break;
			case BC_GETCLOSED: {
				ASSERT(WITHIN(BC_ARGY(byte),0,proto.nvalues));
				locals[BC_ARGX(byte)] = values[BC_ARGY(byte)];
			} break;
			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte),0,ARRAY_LENGTH(M->functions)));
				elf_Function proto = M->functions[BC_ARGY(byte)];
				elf_Closure *new_cls = elf_alloc_closure(R,proto);
				copy_memory(new_cls->values,locals+BC_ARGX(byte),proto.nvalues*sizeof(elf_Value));
				locals[BC_ARGX(byte)].tag   = elf_TAG_CLS;
				locals[BC_ARGX(byte)].x_cls = new_cls;
			} break;
			case BC_TABLE: {
				elf_Table *tab = elf_alloc_table(R);
				locals[BC_ARGX(byte)].tag   = elf_TAG_TAB;
				locals[BC_ARGX(byte)].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				elf_type_check(R,module_instr,BC_ARGX(byte),BC_ARGY(byte),locals[BC_ARGX(byte)].tag);
			} break;
			case BC_GETMETAFIELD: {
				elf_Value yy = locals[BC_ARGY(byte)];
				elf_Table *metatable = {0};
				switch (yy.tag) {
					case elf_TAG_STR: case elf_TAG_TAB:
					case elf_TAG_OBJ: case elf_TAG_CLS: {
						metatable = yy.x_obj->metatable;
					} goto _lookup;
					case elf_TAG_NUM: {
						metatable = R->metatables.number;
						elf_fail(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case elf_TAG_INT: {
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
				locals[BC_ARGX(byte)] = elf_tget_any(metatable,locals[BC_ARGZ(byte)]);
			} break;
			/* todo: why are these two identical bro... */
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (yy.tag==elf_TAG_NIL) {
					/* I don't know whether this worth throwing an error
					over, it's just my preference and use cases, maybe
					tables can have a default field which gets returned
					when the value is nil?  */
					elf_fail(R,module_instr,"attempted to get nil field");
				} else if (xx.tag==elf_TAG_FLOAT2) {
					ASSERT(yy.tag==elf_TAG_STR);
					if (text_eq(yy.x_str->text,"x")){
						locals[BC_ARGX(byte)].tag=elf_TAG_NUM;
						locals[BC_ARGX(byte)].x_num=xx.x_f32;
					} else if (text_eq(yy.x_str->text,"y")){
						locals[BC_ARGX(byte)].tag=elf_TAG_NUM;
						locals[BC_ARGX(byte)].x_num=xx.y_f32;
					} else elf_fail(R,module_instr,"type 'float2' only has x and y fields");
				} else if (xx.tag==elf_TAG_TAB) {
					locals[BC_ARGX(byte)]=elf_tget_any(xx.x_tab,yy);
				} else if (xx.tag==elf_TAG_OBJ) {
					call_overload(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
				} else if (xx.tag==elf_TAG_STR) {
					elf_Int index;
					elf_String *string;
					/* todo: allow for indexing for substrings,
					for instrance, "my name is"["name"] */
					elf_type_check(R,module_instr,0,elf_TAG_INT,yy.tag);
					string=xx.x_str;
					index=yy.x_int;
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = string->text[index];
				} else if (xx.tag==elf_TAG_NIL) {
					elf_fail(R,module_instr,"attempted to get field of 'nil' value");
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid object '%s' to perform this operator on", tag2s[yy.tag]));
				}
			} break;
			elf_Value xx,yy,zz;
			case BC_SETINDEX: case BC_SETFIELD: {
				xx=locals[BC_ARGX(byte)];
				yy=locals[BC_ARGY(byte)];
				zz=locals[BC_ARGZ(byte)];
				if (xx.tag==elf_TAG_FLOAT2) {
					ASSERT(yy.tag==elf_TAG_STR);
					if (text_eq(yy.x_str->text,"x")){
						locals[BC_ARGX(byte)].x_f32=zz.x_num;
					} else if (text_eq(yy.x_str->text,"y")){
						locals[BC_ARGX(byte)].y_f32=zz.x_num;
					} else elf_fail(R,module_instr,"type 'float2' only has x and y fields");
				} else if (xx.tag==elf_TAG_TAB) {
					elf_tset(xx.x_tab,yy,zz);
				} else if (xx.tag==elf_TAG_OBJ) {
					elf_Value args[] = { yy, zz };
					call_overload(R,xx.x_obj,"__setfield",BC_ARGX(byte),2,args);
				} else {
					elf_fail(R,module_instr,elf_tpf("attempted to set field of (%lli) '%s' value",xx.tag,tag2s[xx.tag]));
				}
			} break;
			case BC_CALL: {
				R->byte = module_instr;
				SET_TOP(R,locals+BC_ARGX(byte)+1+BC_ARGY(byte));
				elf_call_function(R,BC_ARGY(byte),BC_ARGZ(byte));
				SET_TOP(R,locals+proto.nlocals);
			} break;
			case BC_ISNIL: {
				elf_Value xx;
				elf_Bool nan;

				xx = locals[BC_ARGY(byte)];
				nan = xx.tag != elf_TAG_INT && xx.tag != elf_TAG_NUM;
				locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
				locals[BC_ARGX(byte)].x_int = xx.tag == elf_TAG_NIL || (nan && xx.x_int == 0);
			} break;
			case BC_N2I: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
				locals[BC_ARGX(byte)].x_int = VN2I(xx);
			} break;
			case BC_I2N: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_TAG_NUM;
				locals[BC_ARGX(byte)].x_num = VI2N(xx);
			} break;
			elf_Bool eq;
			case BC_EQ: case BC_NEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				eq=0;
				if ((xx.tag==elf_TAG_NIL)||(yy.tag==elf_TAG_NIL)) {
					eq=ISNILV(xx)==ISNILV(yy);
				} else if ((xx.tag==elf_TAG_STR)&&(yy.tag==elf_TAG_STR)) {
					eq=elf_seq(xx.x_str,yy.x_str);
				} else if ((ISNUMT(xx.tag))&&(ISNUMT(yy.tag))) {
					eq=xx.x_int==yy.x_int;
				} else {
					eq=(xx.tag==yy.tag)&&(xx.x_int==yy.x_int);
				}
				if (BC_OP(byte)==BC_NEQ) {
					eq=!eq;
				}
				locals[BC_ARGX(byte)].tag  =elf_TAG_INT;
				locals[BC_ARGX(byte)].x_int=eq;
			} break;
			case BC_POW: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag==elf_TAG_NUM)||(yy.tag==elf_TAG_NUM)) {
					if (!ISNUMT(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_TAG_NUM;
					locals[BC_ARGX(byte)].x_num = pow(VI2N(xx),VI2N(yy));
				} else if ((xx.tag==elf_TAG_INT)||(yy.tag==elf_TAG_INT)) {
					if (!ISNUMT(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = pow(VN2I(xx),VN2I(yy));
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;
			case BC_MOD: {
				elf_Num x,y;
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag == elf_TAG_NUM)||(yy.tag == elf_TAG_NUM)) {
					if (!ISNUMT(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					check_division_by_zero(R,xx,yy);
					x=VI2N(xx);
					y=VI2N(yy);
					locals[BC_ARGX(byte)].tag   = elf_TAG_NUM;
					locals[BC_ARGX(byte)].x_num = x - (elf_Int)(x / y) * y;
				} else if ((xx.tag==elf_TAG_INT)||(yy.tag==elf_TAG_INT)) {
					if (!ISNUMT(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					check_division_by_zero(R,xx,yy);
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = VN2I(xx) % VN2I(yy);
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;
	/* todo: make this better */
	#define CASE_IBOP(OPNAME,OP) \
			case OPNAME : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if ((BC_OP(byte)==BC_DIV)||(BC_OP(byte)==BC_MOD)) {\
					check_division_by_zero(R,xx,yy);\
				}\
				locals[BC_ARGX(byte)].tag   = elf_TAG_INT;\
				locals[BC_ARGX(byte)].x_int = VN2I(xx) OP VN2I(yy);\
			} break
	#define CASE_BOP(OPCODE,OP,FN,FN1) \
			case OPCODE : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				float x0,y0,x1,y1;\
				if ((xx.tag==elf_TAG_FLOAT2)||(yy.tag==elf_TAG_FLOAT2)) {\
					\
					if (xx.tag==elf_TAG_NUM)x0=y0=(float)xx.x_num;\
					else if(xx.tag==elf_TAG_INT)x0=y0=(float)xx.x_int;\
					else x0=xx.x_f32,y0=xx.y_f32;\
					\
					if (yy.tag==elf_TAG_NUM)x1=y1=(float)yy.x_num;\
					else if(yy.tag==elf_TAG_INT)x1=y1=(float)yy.x_int;\
					else x1=yy.x_f32,y1=yy.y_f32;\
					\
					locals[BC_ARGX(byte)].tag   = elf_TAG_FLOAT2;\
					locals[BC_ARGX(byte)].x_f32 = x0 OP x1;\
					locals[BC_ARGX(byte)].y_f32 = y0 OP y1;\
				} else if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {\
					if (!ISOBJT(xx.tag)) elf_fail(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) check_division_by_zero(R,xx,yy);\
					call_overload(R,xx.x_obj,ISOBJT(yy.tag)?FN:FN1,BC_ARGX(byte),1,&yy);\
				} else if ((xx.tag==elf_TAG_NUM) || (yy.tag==elf_TAG_NUM)) {\
					if (!ISNUMT(yy.tag)) elf_fail(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) check_division_by_zero(R,xx,yy);\
					locals[BC_ARGX(byte)].tag   = elf_TAG_NUM;\
					locals[BC_ARGX(byte)].x_num = VI2N(xx) OP VI2N(yy);\
				} else if ((xx.tag==elf_TAG_INT)||(yy.tag==elf_TAG_INT)) {\
					if (!ISNUMT(yy.tag)) elf_fail(R,NO_BYTE,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));\
					/* Could we redefine this? */ \
					if (BC_OP(byte)==BC_DIV) check_division_by_zero(R,xx,yy);\
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;\
					locals[BC_ARGX(byte)].x_int = VN2I(xx) OP VN2I(yy);\
				} else elf_fail(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));\
			} break
			case BC_LTEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if ((xx.tag==elf_TAG_NUM)||(yy.tag==elf_TAG_NUM)) {
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = VI2N(xx) <= VI2N(yy);
				} else {
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = VN2I(xx) <= VN2I(yy);
				}
			} break;
			case BC_LT: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if ((xx.tag==elf_TAG_NUM)||(yy.tag==elf_TAG_NUM)) {
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = VI2N(xx) < VI2N(yy);
				} else {
					locals[BC_ARGX(byte)].tag   = elf_TAG_INT;
					locals[BC_ARGX(byte)].x_int = VN2I(xx) < VN2I(yy);
				}
			} break;
			CASE_IBOP(BC_SHL,  <<);
			CASE_IBOP(BC_SHR,  >>);
			CASE_IBOP(BC_BIT_XOR, ^);
			CASE_IBOP(BC_BIT_AND, &);
			CASE_IBOP(BC_BIT_OR,  |);
			CASE_BOP(BC_ADD, +, "__add", "__add1");
			CASE_BOP(BC_SUB, -, "__sub", "__sub1");
			CASE_BOP(BC_MUL, *, "__mul", "__mul1");
			CASE_BOP(BC_DIV, /, "__div", "__div1");
	#undef CASE_BOP
			default: {
				elf_fail(R,module_instr,elf_tpf("unsupported instruction: %s", get_byte_label(BC_OP(byte))));
			} break;
		}
	}

	esc:
	return F->nregs;
}

