/*
** See Copyright Notice In elf.h
** core.c
*/


elf_Value *elf_get_stack(elf_State *S){
	return S->stack;
}
elf_Value *elf_get_stack_ptr(elf_State *S){
	return S->stack_ptr;
}


static int elf_run(elf_State *R);

void elf_init(elf_State *R, elf_Module *M) {
	R->M=M;
	elf_entry_chunk_arena = sys_virtual_alloc(GIGABYTES(1));
	elf_entry_chunk_arena_index = 0;
	R->G.table_objects  = sys_virtual_alloc(GIGABYTES(1));
	R->G.string_objects = sys_virtual_alloc(GIGABYTES(1));

	R->stack_max = DEFAULT_STACK_SIZE;
	R->stack = calloc_memory(GLOBAL_ALLOCATOR,sizeof(elf_Value)*R->stack_max);
	R->stack_ptr = R->stack;
	R->first_frame.locals = R->stack_ptr;
	R->frame = &R->first_frame;
	R->nframe = 0;
	R->metatables.string = elf_new_string_lib(R);
	R->metatables.table = new_table_lib(R);
	M->globals = elf_new_table(R);
	M->strings = elf_new_table(R);

	elf_gsetx_int(R,"elf.VERSION",0);
	#if defined(PLATFORM_WEB)
	elf_gsetx_str(R,"elf.PLATFORM","WEB");
	elf_gsetx_str(R,"elf.OS","UNKNOWN");
	#else
	elf_gsetx_str(R,"elf.PLATFORM","DESKTOP");
		#if defined(_WIN32)
	elf_gsetx_str(R,"elf.OS","WINDOWS");
		#else
	elf_gsetx_str(R,"elf.OS","UNKNOWN");
		#endif
	#endif

	static elf_CBinding lib_base[] = {
		{"ntoi", core_lib_ntoi},
		{"iton", core_lib_iton},
	};
	elf_add_lib(R,0,lib_base,_countof(lib_base));
	elf_add_lib(R,"elf",_lib_time,_countof(_lib_time));
	elf_add_lib(R,0,lib_math,_countof(lib_math));
	elf_add_lib(R,"elf",lib_core,_countof(lib_core));
}

// functions that we use internally for profiling
elf_i64 elf_get_clock_time() {
	return sys_get_clock_time();
}
elf_f64 elf_time_diff_s(elf_i64 time) {
	return (sys_get_clock_time() - time) / (elf_f64) sys_get_clock_freq();
}
elf_f64 elf_time_diff_ms(elf_i64 time) {
	return elf_time_diff_s(time) * 1000.;
}


static void _debug_stack_push(elf_State *S, elf_Value v) {
	ASSERT(S->stack_ptr - S->stack < S->stack_max);
	* GET_TOP(S) ++ = v;
}

/*
Note: the function is expected to be the first argument, however
the number of arguments does not include the function itself.
Note: The function's prototype tells how many locals to allocate
space for, this includes the arguments, the remaining space is
allocated and initialized to zero.
Note: if the number of arguments is greater than the number of
locals (too many arguments) then the number of locals for this
call frame is the number of arguments.
Todo: why is the function itself required to be adjacent to
the arguments? Couldn't we just call a function at any given
register.
Todo: why do we require the user to push the return values to
the stack, to then copy them?
*/
int elf_call(elf_State *S, int nargs, int nrets) {
	int nyield = 0;

	elf_Value *locals = S->stack_ptr - nargs;
	elf_Value value = locals[-1];
	if (!CAN_CALL(value.tag)) {
		elf_fail(S,NO_BYTE,elf_tpf("cannot call '%s'", tag2s[value.tag]));
	}

	int num_locals = nargs;
	elf_Closure *closure = 0;

	if (value.tag == elf_tag_closure) {
		closure = value.x_cls;
		elf_Proto proto = closure->proto;
		if (num_locals < proto.nlocals) {
			num_locals = proto.nlocals;
		}
		int stack_size = num_locals - nargs;
		ASSERT(stack_size >= 0);
		clear_memory(locals+nargs,stack_size * sizeof(elf_Value));
	}

	/* todo: do not make this recursive dude! */
	elf_Stack_Frame F = {0};
	/* Todo: remove this */
	F.caller = S->frame;
	/* Todo: this is for debugging */
	F.origin = S->byte;
	F.locals = locals;
	F.nargs = nargs;
	F.nrets = nrets;
	F.closure = closure;
	F.nlocals = num_locals;
	GET_FRAME(S) = &F;
	SET_TOP(S,locals+num_locals);

	S->nframe ++;

	if (S->flags & FLAG_DEBUGGER_ONCALL) {
		elf_debugger("debugger 'FLAG_DEBUGGER_ONCALL'");
	}

	if (value.tag == elf_tag_closure) {
		nyield = elf_run(S);
	} else if (value.tag == elf_tag_proc) {
		elf_Value *res, *ptr;
		res = S->stack_ptr;
		nyield = value.x_fun(S);
		ptr = S->stack_ptr;
		if ((ptr-res) < nyield) {
			elf_fail(S,NO_BYTE,elf_tpf("number of values on stack '%i', is incoherent with specified number of yielded values '%i'",(int)(ptr - res),nyield));
		}
		res=ptr-nyield;
		// if ((nyield==0)&&(nrets>0)){
		// 	elf_debug_log("seems like this function does not return anything");
		// }
		/* todo: clear only the part we didn't write */
		clear_memory(F.locals-1,nrets*sizeof(elf_Value));
		copy_memory(F.locals-1,res,MIN(nyield,nrets)*sizeof(elf_Value));
	} else {
		nyield = -1;
		elf_fail(S,NO_BYTE,elf_tpf("'%s': is not a function",tag2s[value.tag]));
	}
	GET_FRAME(S) = F.caller;
	S->nframe -= 1;
	ASSERT(S->nframe > -1);
	return nyield;
}


// todo: make this legit!
static void _check_zero_div(elf_State *S, elf_Value xx, elf_Value yy) {
	if ((yy.tag == elf_tag_num) && (yy.x_num == 0.)) elf_fail(S,NO_BYTE,"division by zero"); else
	if ((yy.tag == elf_tag_int) && (yy.x_int == 0)) elf_fail(S,NO_BYTE,"integer division by zero");
}


static int call_overload(elf_State *S, elf_Object *obj, char const *name, int reg, int nargs, elf_Value *args) {
	if (obj->meta == 0) {
		elf_fail(S,NO_BYTE,"object does not have a metatable, cannot use overload");
	}
	elf_Value field;
	field=elf_tgetx_any(obj->meta,name);
	if (!CAN_CALL(field.tag)) {
		elf_fail(S,NO_BYTE,elf_tpf("'%s': overload is %s, not a function",name,tag2s[field.tag]));
	}
	elf_Value *top;
	top=GET_TOP(S);
	PUSHV(S,field);
	PUSHV(S,VOBJ(obj));
	copy_memory(GET_TOP(S),args,sizeof(elf_Value)*nargs);
	GET_TOP(S) += nargs;
	int ny = elf_call(S,nargs+1,1);
	GET_LOCAL(S,reg)=*top;
	return ny;
}

int elf_run(elf_State *R) {

	elf_Module *M;
	elf_Table *globals;
	elf_Stack_Frame *F;
	elf_Value *locals;
	elf_Proto proto;
	elf_Value *values;
	elf_Int module_instr,instr,next_instr;
	elf_Bytecode byte;
	#if 0
	delaylist *delay;
	#endif
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
		if (R->flags & FLAG_BYTELOGGING || F->logging)
		{
			fpf_byte(stdout,M,-1,module_instr,byte);
		}
		if (R->flags & FLAG_DEBUGGER) {
			elf_debugger("debugger 'FLAG_DEBUGGER'");
		}
#endif

#if defined(ELF_EXPERIMENTAL_FEATURES)
		if (R->bytetracking) {
			elf_Int track = ++ M->track[module_instr];
			if (track == 64) {
				elf_Proto file;
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
			case BC_RET: {
				int reg,nrets;
				nrets = MIN(BC_ARGY(byte),F->nrets);
				for (reg=0; reg<nrets; ++reg) {
					locals[reg-1] = locals[reg+BC_ARGX(byte)];
				}
				F->nrets = nrets;
				goto esc;
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
				locals[BC_ARGX(byte)].tag    = elf_tag_nil;
				locals[BC_ARGX(byte)].x_int  = 0;
			} break;
			case BC_GETKINT: {
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = M->integers[BC_ARGY(byte)];
			} break;
			case BC_GETKNUM: {
				locals[BC_ARGX(byte)].tag   = elf_tag_num;
				locals[BC_ARGX(byte)].x_num = M->numbers[BC_ARGY(byte)];
			} break;
			case BC_GETUPVAL: {
				ASSERT(WITHIN(BC_ARGY(byte),0,proto.nvalues));
				locals[BC_ARGX(byte)] = values[BC_ARGY(byte)];
			} break;
			case BC_CLOSURE: {
				ASSERT(WITHIN(BC_ARGY(byte),0,ARRAY_LENGTH(M->protos)));
				elf_Proto proto = M->protos[BC_ARGY(byte)];
				elf_Closure *new_cls = elf_alloc_closure(R,proto);
				copy_memory(new_cls->values,locals+BC_ARGX(byte),proto.nvalues*sizeof(elf_Value));
				locals[BC_ARGX(byte)].tag   = elf_tag_closure;
				locals[BC_ARGX(byte)].x_cls = new_cls;
			} break;
			case BC_TABLE: {
				elf_Table *tab = elf_alloc_table(R);
				locals[BC_ARGX(byte)].tag   = elf_tag_tab;
				locals[BC_ARGX(byte)].x_tab = tab;
			} break;
			case BC_TYPEGUARD: {
				elf_type_check(R,module_instr,BC_ARGX(byte),BC_ARGY(byte),locals[BC_ARGX(byte)].tag);
			} break;
			case BC_GETMETAFIELD: {
				elf_Value yy = locals[BC_ARGY(byte)];
				elf_Table *metatable = {0};
				switch (yy.tag) {
					case elf_tag_str:
					case elf_tag_tab:
					case elf_tag_userobj:
					case elf_tag_closure: {
						metatable = yy.x_obj->meta;
					} goto _lookup;
					case elf_tag_num: {
						metatable = R->metatables.number;
						elf_fail(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					case elf_tag_int: {
						metatable = R->metatables.integer;
						elf_fail(R,module_instr,"this feature is not implemented yet, metatables for numeric types");
					} goto _lookup;
					default: {
						elf_fail(R,module_instr,elf_tpf("'%s': not an object", tag2s[yy.tag]));
					} break;
				}
				_lookup:
				if (!metatable) {
					elf_fail(R,module_instr,elf_tpf("'%s': invalid object, no metatable", tag2s[yy.tag]));
				}
				locals[BC_ARGX(byte)] = elf_table_get(metatable,locals[BC_ARGZ(byte)]);
			} break;
			/* todo: why are these two identical bro... */
			case BC_GETINDEX: case BC_GETFIELD: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (yy.tag==elf_tag_nil) {
					/* I don't know whether this worth throwing an error
					over, it's just my preference and use cases, maybe
					tables can have a default field which gets returned
					when the value is nil?  */
					elf_fail(R,module_instr,"attempted to get nil field");
				}
#if 0
				 else if (xx.tag==elf_tag_float2) {
					ASSERT(yy.tag==elf_tag_str);
					if (text_eq(yy.x_str->text,"x")){
						locals[BC_ARGX(byte)].tag=elf_tag_num;
						locals[BC_ARGX(byte)].x_num=xx.x_f32;
					} else if (text_eq(yy.x_str->text,"y")){
						locals[BC_ARGX(byte)].tag=elf_tag_num;
						locals[BC_ARGX(byte)].x_num=xx.y_f32;
					} else elf_fail(R,module_instr,"type 'float2' only has x and y fields");
				}
#endif
				else if (xx.tag==elf_tag_tab) {
					locals[BC_ARGX(byte)]=elf_table_get(xx.x_tab,yy);
				} else if (xx.tag==elf_tag_userobj) {
					call_overload(R,xx.x_obj,"__getfield",BC_ARGX(byte),1,&yy);
				} else if (xx.tag==elf_tag_str) {
					elf_Int index;
					elf_String *string;
					/* todo: allow for indexing for substrings,
					for instrance, "my name is"["name"] */
					elf_type_check(R,module_instr,0,elf_tag_int,yy.tag);
					string=xx.x_str;
					index=yy.x_int;
					locals[BC_ARGX(byte)].tag   = elf_tag_int;
					locals[BC_ARGX(byte)].x_int = string->text[index];
				} else if (xx.tag==elf_tag_nil) {
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
				// if(xx.tag==elf_tag_float2){
				// 	ASSERT(yy.tag==elf_tag_str);
				// 	if (text_eq(yy.x_str->text,"x")){
				// 		locals[BC_ARGX(byte)].x_f32=zz.x_num;
				// 	} else if (text_eq(yy.x_str->text,"y")){
				// 		locals[BC_ARGX(byte)].y_f32=zz.x_num;
				// 	} else elf_fail(R,module_instr,"type 'float2' only has x and y fields");
				// }else
				if(xx.tag==elf_tag_tab){
					if(!xx.x_tab) elf_fail(R,module_instr,elf_tpf("table is nil, how did this happen?"));
					if(yy.tag==elf_tag_nil) elf_fail(R,module_instr,elf_tpf("key is nil..."));
					elf_table_set(xx.x_tab,yy,zz);
				}else if(xx.tag==elf_tag_userobj){
					elf_Value args[] = { yy, zz };
					call_overload(R,xx.x_obj,"__setfield",BC_ARGX(byte),2,args);
				}else{
					elf_fail(R,module_instr,elf_tpf("attempted to set field of (%lli) '%s' value",xx.tag,tag2s[xx.tag]));
				}
			} break;
			case BC_CALL: {
				R->byte = module_instr;
				SET_TOP(R,locals+BC_ARGX(byte)+1+BC_ARGY(byte));
				elf_call(R,BC_ARGY(byte),BC_ARGZ(byte));
				SET_TOP(R,locals+proto.nlocals);
			} break;
#if 0
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
				__debugbreak();
				ASSERT(BC_ARGX(byte) >= 0);
				int reg,nrets;
				nrets = MIN(BC_ARGZ(byte),F->nrets);
				for (reg=0; reg<nrets; ++reg) {
					locals[reg-1] = locals[reg+BC_ARGY(byte)];
				}
				next_instr = instr+BC_ARGX(byte);

				F->nrets = nrets;
			} break;
			case BC_LOOP: {
			} break;
			case BC_ISNIL: {
				elf_Value xx;
				bool nan;

				xx = locals[BC_ARGY(byte)];
				nan = xx.tag != elf_tag_int && xx.tag != elf_tag_num;
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = xx.tag == elf_tag_nil || (nan && xx.x_int == 0);
			} break;
#endif
			case BC_N2I: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_tag_int;
				locals[BC_ARGX(byte)].x_int = VN2I(xx);
			} break;
			case BC_I2N: {
				xx=locals[BC_ARGY(byte)];
				locals[BC_ARGX(byte)].tag   = elf_tag_num;
				locals[BC_ARGX(byte)].x_num = VI2N(xx);
			} break;
			case BC_EQ: case BC_NEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				bool eq=0;
				if ((xx.tag==elf_tag_nil)||(yy.tag==elf_tag_nil)) {
					eq=ISNILV(xx)==ISNILV(yy);
				} else if ((xx.tag==elf_tag_str)&&(yy.tag==elf_tag_str)) {
					eq=elf_get_strings_eq(xx.x_str,yy.x_str);
				} else if ((INTORNUM(xx.tag))&&(INTORNUM(yy.tag))) {
					eq=xx.x_int==yy.x_int;
				} else {
					eq=(xx.tag==yy.tag)&&(xx.x_int==yy.x_int);
				}
				if (BC_OP(byte)==BC_NEQ) {
					eq=!eq;
				}
				locals[BC_ARGX(byte)].tag  =elf_tag_int;
				locals[BC_ARGX(byte)].x_int=eq;
			} break;
#if 0
			case BC_POW: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
					if (!INTORNUM(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_tag_num;
					locals[BC_ARGX(byte)].x_num = pow(VI2N(xx),VI2N(yy));
				} else if ((xx.tag==elf_tag_int)||(yy.tag==elf_tag_int)) {
					if (!INTORNUM(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					locals[BC_ARGX(byte)].tag   = elf_tag_int;
					locals[BC_ARGX(byte)].x_int = pow(VN2I(xx),VN2I(yy));
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;
#endif
			case BC_MOD: {
				elf_Num x,y;
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {
					NO_CODE;
				} else if ((xx.tag == elf_tag_num)||(yy.tag == elf_tag_num)) {
					if (!INTORNUM(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					_check_zero_div(R,xx,yy);
					x=VI2N(xx);
					y=VI2N(yy);
					locals[BC_ARGX(byte)].tag   = elf_tag_num;
					locals[BC_ARGX(byte)].x_num = x - (elf_Int)(x / y) * y;
				} else if ((xx.tag==elf_tag_int)||(yy.tag==elf_tag_int)) {
					if (!INTORNUM(yy.tag)) {
						elf_fail(R,module_instr,elf_tpf("'%s': incompatible with '%s'",tag2s[xx.tag],tag2s[yy.tag]));
					}
					_check_zero_div(R,xx,yy);
					locals[BC_ARGX(byte)].tag   = elf_tag_int;
					locals[BC_ARGX(byte)].x_int = VN2I(xx) % VN2I(yy);
				} else {
					elf_fail(R,module_instr,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[xx.tag],tag2s[yy.tag],XTEXT(OP)));
				}
			} break;


	#define INVALID_OPERANDS(X,Y,OP) elf_fail(R,NO_BYTE,elf_tpf("invalid types '%s' and '%s', for operator '%s'", tag2s[X],tag2s[Y],XTEXT(OP)))

	/* todo: make this better */
	#define INTBOP(OPNAME,OP) \
			case OPNAME : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if((xx.tag==elf_tag_int) && (yy.tag==elf_tag_int)) {\
					if ((BC_OP(byte)==BC_DIV)||(BC_OP(byte)==BC_MOD)) {\
						_check_zero_div(R,xx,yy);\
					}\
					locals[BC_ARGX(byte)].tag   = elf_tag_int;\
					locals[BC_ARGX(byte)].x_int = OP(VN2I(xx),VN2I(yy));\
				} else INVALID_OPERANDS(xx.tag,yy.tag,OP); \
			} break
#if 0
float x0,y0,x1,y1;\
if ((xx.tag==elf_tag_float2)||(yy.tag==elf_tag_float2)) {\
	\
	if (xx.tag==elf_tag_num)x0=y0=(float)xx.x_num;\
	else if(xx.tag==elf_tag_int)x0=y0=(float)xx.x_int;\
	else x0=xx.x_f32,y0=xx.y_f32;\
	\
	if (yy.tag==elf_tag_num)x1=y1=(float)yy.x_num;\
	else if(yy.tag==elf_tag_int)x1=y1=(float)yy.x_int;\
	else x1=yy.x_f32,y1=yy.y_f32;\
	\
	locals[BC_ARGX(byte)].tag   = elf_tag_float2;\
	locals[BC_ARGX(byte)].x_f32 = x0 OP x1;\
	locals[BC_ARGX(byte)].y_f32 = y0 OP y1;\
} else
#endif
	#define NUMBOP(OPCODE,OP,FN,FN1) \
			case OPCODE : {\
				xx=locals[BC_ARGY(byte)];\
				yy=locals[BC_ARGZ(byte)];\
				if (ISOBJT(xx.tag)||ISOBJT(yy.tag)) {\
					if (!ISOBJT(xx.tag)) elf_fail(R,NO_BYTE,"invalid ordering, object type must come first, (todo: call converter function on the object, __tonumber)");\
					call_overload(R,xx.x_obj,ISOBJT(yy.tag)?FN:FN1,BC_ARGX(byte),1,&yy);\
				} else if (INTORNUM(xx.tag) && INTORNUM(yy.tag)) {\
					if (BC_OP(byte)==BC_DIV) _check_zero_div(R,xx,yy);\
					if((xx.tag==elf_tag_num) || (yy.tag==elf_tag_num)) {\
						locals[BC_ARGX(byte)].tag   = elf_tag_num;\
						locals[BC_ARGX(byte)].x_num = OP(VI2N(xx),VI2N(yy));\
					}else{\
						locals[BC_ARGX(byte)].tag   = elf_tag_int;\
						locals[BC_ARGX(byte)].x_int = OP(VN2I(xx),VN2I(yy));\
					}\
				} else INVALID_OPERANDS(xx.tag,yy.tag,OP); \
			} break
			case BC_LTEQ: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if(INTORNUM(xx.tag) && INTORNUM(yy.tag)){
					if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VI2N(xx) <= VI2N(yy);
					} else {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VN2I(xx) <= VN2I(yy);
					}
				} else INVALID_OPERANDS(xx.tag,yy.tag,<=);
			} break;
			case BC_LT: {
				xx=locals[BC_ARGY(byte)];
				yy=locals[BC_ARGZ(byte)];
				if(INTORNUM(xx.tag) && INTORNUM(yy.tag)){
					if ((xx.tag==elf_tag_num)||(yy.tag==elf_tag_num)) {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VI2N(xx) < VI2N(yy);
					} else {
						locals[BC_ARGX(byte)].tag   = elf_tag_int;
						locals[BC_ARGX(byte)].x_int = VN2I(xx) < VN2I(yy);
					}
				} else INVALID_OPERANDS(xx.tag,yy.tag,<);
			} break;

#define __shl(a,b) ((a) << (b))
#define __shr(a,b) ((a) >> (b))
#define __add(a,b) ((a) + (b))
#define __sub(a,b) ((a) - (b))
#define __mul(a,b) ((a) * (b))
#define __div(a,b) ((a) / (b))
#define __and(a,b) ((a) & (b))
#define __or(a,b)  ((a) | (b))
#define __xor(a,b) ((a) ^ (b))
#define __pow(a,b) (pow((a),(b)))

			INTBOP(BC_SHL,  	 __shl);
			INTBOP(BC_SHR,  	 __shr);
			INTBOP(BC_BIT_XOR, __xor);
			INTBOP(BC_BIT_AND, __and);
			INTBOP(BC_BIT_OR,   __or);

			NUMBOP(BC_ADD, __add, "__add", "__add1");
			NUMBOP(BC_SUB, __sub, "__sub", "__sub1");
			NUMBOP(BC_MUL, __mul, "__mul", "__mul1");
			NUMBOP(BC_DIV, __div, "__div", "__div1");
			NUMBOP(BC_POW, __pow, "__pow", "__pow1");
	#undef NUMBOP
			default: {
				elf_fail(R,module_instr,elf_tpf("unsupported instruction: %s", byte2s[BC_OP(byte)]));
			} break;
		}
	}

	esc:
	return F->nrets;
}

