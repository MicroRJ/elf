//
// See Copyright Notice In elf.h
//

ELF_FUNCTION(core_lib_get_meta) {
	elf_push_table_raw(S, elf_get_object_arg_raw(S, 0)->meta);
	return 1;
}


ELF_FUNCTION(core_lib_set_meta) {
	elf_get_object_arg_raw(S,0)->meta = elf_get_table(S, 1);
	*S->stack_ptr ++ = elf_get_arg(S, 0);
	return 1;
}

ELF_FUNCTION(core_lib_tagof) {
	elf_push_string(S, tag2s[elf_get_argtag(S, 0)]);
	return 1;
}

ELF_FUNCTION(core_lib_iton) {
	elf_Value v = elf_get_arg(S, 0);
	if (v.tag == elf_tag_Int) {
		elf_push_num(S, (elf_Num) v.x_int);
	} else elf_push_num(S, v.x_num);
	return 1;
}

ELF_FUNCTION(core_lib_ntoi) {
	elf_Value v = elf_get_arg(S, 0);
	if (v.tag == elf_tag_Num) {
		elf_push_int(S, (elf_Int) v.x_num);
	} else elf_push_int(S, v.x_int);
	return 1;
}


ELF_FUNCTION(core_lib_load_file) {
	int nargs = 1; // elf_get_num_args(S) - 1;
	int nrets = elf_get_num_rets(S);

	elf_push_string(S, "noname");

	// todo: no like
	*S->stack_ptr ++ = elf_get_arg(S, 0);
	elf_read_file(S, -1);

	nrets = elf_exec(S,nargs,nrets,false);
	return nrets;
}


ELF_FUNCTION(core_lib_load_expr) {
	int nargs = 1; // elf_get_num_args(S) - 1;
	int nrets = elf_get_num_rets(S);

	elf_push_string(S, "noname");

	// todo: no like
	*S->stack_ptr ++ = elf_get_arg(S, 0);
	elf_read_file(S, -1);

	nrets = elf_exec(S,nargs,nrets,false);
	return nrets;
}


ELF_FUNCTION(core_lib_const_expr) {
	char *contents = elf_get_text_arg(S, 0);
	elf_load_const_expr(S, "no name", contents);
	return 1;
}

ELF_FUNCTION(core_lib_load_json) {
	char *name = elf_get_text_arg(S, 0);
	elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_ALWAYS);
	if (file) {
		unsigned int size = sys_size_file(file);
		char *heapbuf = malloc(size);
		sys_read_file(file, heapbuf, 0, size);
		sys_close_file(file);
		elf_load_json(S, name, heapbuf);
	} else {
		elf_push_nil(S);
	}
	return 1;
}





// todo:
static int formatvalue(char *buff, int size, elf_Value v) {
	int res = 0;
	switch (v.tag) {
		// todo: find the specific cases within stb and use them directly
		case elf_tag_Nil: {
			res=stbsp_snprintf(buff,size,"%s","nil");
		} break;
		case elf_tag_Int: {
			res=stbsp_snprintf(buff,size,"%lli",v.x_int);
		} break;
		case elf_tag_Num: {
			res=stbsp_snprintf(buff,size,"%f",v.x_num);
		} break;
		case elf_tag_String: {
			res=stbsp_snprintf(buff,size,"%s",v.x_str->text);
		} break;
		// case elf_tag_Handle: return fprintf(file,"h%llX",v.x_int);
		// case elf_tag_Closure: return fprintf(file,"F()");
		// case elf_tag_Function: return fprintf(file,"C()");
		// case elf_tag_Table: {} break;
		default: {
			res=stbsp_snprintf(buff,size,"%s","(?)");
		} break;
	}
	return res;
}


// todo: handle escape sequences
ELF_FUNCTION(core_lib_format) {
	int argindex = 0;
	char *format = elf_get_text_arg(S, argindex ++);
	// todo: do the same strat were we have a small temporary buffer in the
	// elf state accessible to everyone! elf_get_tempbuf()
	char *tempbuf = thread_alloc(4096);
	char *write = tempbuf;
	while (*format) {
		while (*format && *format != '%') {
			*write ++ = *format ++;
		}
		if (*format == '%') {
			if (argindex >= elf_get_num_args(S)) {
				elf_error(S, NO_BYTE, "not enough arguments to format string!");
			}
			format += 1;
			elf_Value value = elf_get_arg(S, argindex ++);
			int wrote = formatvalue(write, 4096-(write-tempbuf), value);
			write += wrote;
		}
	}
	*write ++ = 0;
	elf_push_string(S, tempbuf);
	return 1;
}


/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_State *R) {
	elf_Table *tab = elf_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_table_merge(tab,elf_get_table(R,i));
	}
	return 1;
}


//	int core_lib_abort(elf_State *R) {
//		if(1) abort();
//		return 0;
//	}


/* debugging */
int core_lib_flags(elf_State *R) {
	int flags = R->flags;
	R->flags |= elf_get_intarg(R,0);
	elf_push_int(R,flags);
	return 1;
}


int core_lib_debugger(elf_State *R) {
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_text_arg(R,0);
	}
	elf_debugger(message);
	return 0;
}

#if 0
static char *in_sym_dir(char *dir, char *sym) {
	do {
		if (*dir ++ != *sym ++) {
			return *sym == '.' ? dir : sym;
		}
	} while (*dir);
	return sym;
}


/* the dot is added so that symbols like elf.math.floor
are not mistaken with table accesses when shortened,
math.floor != .math.floor */
int core_lib_include(elf_State *R) {
	elf_check_num_args(R,".include",1,"(the directory to include to add to the global directory)");
	char *dir = elf_get_text_arg(R,0);
	int plen = text_length(dir);
	/* accumulate all symbols here first to
	avoid faulting under repeating patterns:
	elf.ray.elf.ray could include the symbol
	many more times when the new key is added
	as we traverse the array. the new key is
	encountered and we keep repeating the
	process... this would override the previous
	value and result in erroneous behavior.
	todo: */

	elf_Table *globals = R->globals;
	elf_Table_Entry entry;
	FOR_RANGE(i,0,globals->ntotal) {
		entry=globals->slots[i];
		if (entry.key.tag == elf_tag_String) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_table_set_raw(globals,VALUE_STRING(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}
#endif



int core_lib_fpf(elf_State *S) {
	elf_Handle file = elf_get_sysarg(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_push_int(S,wrote);
	return 1;
}

#if 0
int core_lib_lpf(elf_State *S) {
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		if (i != 0) fprintf(stdout,"\n");
		for (int j = 0; j < pf_indent; ++ j) {
			fprintf(stdout, "  ");
		}
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}
#endif

int core_lib_pf(elf_State *S) {
	#if 0
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	#endif
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}



static void bprinttabs(String_Builder *sb, int num) {
	while (num --) bprintf(sb, "\t");
}

// todo: cyclic references will break this
// todo: performance!
static int unparse(elf_State *inter, String_Builder *sb, elf_Value thing, int level) {
	int noerror = true;
	switch (thing.tag) {
		case elf_tag_Nil:    bprintf(sb, "nil"                       ); break;
		case elf_tag_Int:    bprintf(sb, "%lli"  , thing.x_int       ); break;
		case elf_tag_Num:    bprintf(sb, "%f"    , thing.x_num       ); break;
		case elf_tag_String: bprintf(sb, "\"%s\"", thing.x_str->text ); break;
		case elf_tag_Table: {
			elf_Table *table = thing.x_tab;

			int nwrote = 0;

			bprintf(sb, "{\n");

			// todo:
			// figure this out, or pass in flags to determine whether to omit the hash part or the array part
			if (table->nslots) {
				elf_IndexInt i;
				for (i = 0; i < table->ntotal; ++ i) {
					elf_Table_Entry entry = table->slots[i];
					elf_IndexInt index = entry.idx;
					elf_Value key = entry.key;

					if ((key.tag != elf_tag_Num)
					&&  (key.tag != elf_tag_Int)
					&&  (key.tag != elf_tag_String))
					{
						continue;
					}

					elf_Value value = table->array[index];
					if ((value.tag != elf_tag_Num)
					&&  (value.tag != elf_tag_Int)
					&&  (value.tag != elf_tag_Table)
					&&  (value.tag != elf_tag_String))
					{
						continue;
					}

					if (nwrote ++) bprintf(sb, ",\n");
					bprinttabs(sb, level + 1);

					unparse(inter, sb, key, 1);
					bprintf(sb, " = ");
					unparse(inter, sb, value, level + 1);
				}
			} else {
				FOR_ARRAY(i, table->array) {
					elf_Value value = table->array[i];
					if ((value.tag != elf_tag_Num)
					&&  (value.tag != elf_tag_Int)
					&&  (value.tag != elf_tag_Table)
					&&  (value.tag != elf_tag_String))
					{
						continue;
					}
					if (nwrote ++) bprintf(sb, ",\n");
					bprinttabs(sb, level + 1);
					unparse(inter, sb, value, level + 1);
				}
			}
			bprintf(sb,"\n");
			bprinttabs(sb, level);
			bprintf(sb,"}");
		} break;
		default: noerror = false;
	}
	return noerror;
}


ELF_FUNCTION(core_lib_unload) {
	elf_Handle file = elf_get_sysarg(S, 0);
	elf_Value thing = elf_get_arg(S, 1);
	String_Builder sb = {};
	int noerr = unparse(S, &sb, thing, 0);
	if (noerr) {
		sys_write_file(file, sb.buf, 0, sb.min);
	}
	free(sb.buf);
	elf_push_int(S, noerr);
	return 1;
}
