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
		sys_read_file(file, heapbuf, size);
		sys_close_file(file);
		elf_load_json(S, name, heapbuf);
	} else {
		elf_push_nil(S);
	}
	return 1;
}



/* merges one or several tables together into
a new table, which is then returned. */
ELF_FUNCTION(core_lib_merge_tables) {
	elf_Table *tab = elf_new_table(S);
	int i;
	for (i = 0; i < elf_get_num_args(S); i += 1) {
		elf_table_merge(tab,elf_get_table(S,i));
	}
	return 1;
}


//
// FORMATTING
//


static int value_bprintf(String_Builder *sb, elf_Value v, bool flags) {
	switch (v.tag) {
		case elf_tag_Nil:        return bprintf(sb,   "nil"               );
		case elf_tag_Int:        return bprintf(sb,  "%lli", v.x_int      );
		case elf_tag_Num:        return bprintf(sb,    "%f", v.x_num      );
		case elf_tag_Handle:     return bprintf(sb, "h%llX", v.x_int      );
		case elf_tag_String:     return bprintf(sb,    "%s", v.x_str->text);
		case elf_tag_Closure:    return bprintf(sb,   "F()");
		case elf_tag_Function:   return bprintf(sb,   "C()");

		case elf_tag_Table: {
			/* todo: this is slow! */
			int wrote = 0;
			elf_Table *tab = v.x_tab;
			wrote += bprintf(sb, "{");

			elf_IndexInt i,j,n;
			for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
				if (i != 0) wrote += bprintf(sb, ", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elf_Table_Entry it = tab->slots[j];
					if (it.key.tag==elf_tag_Nil) continue;
					if (it.idx!=i) continue;
					if (n ++ != 0) wrote += bprintf(sb, ", ");
					wrote += value_bprintf(sb,it.key,1);
				}
				if (n != 0) wrote += bprintf(sb, " = ");
				wrote += value_bprintf(sb,tab->array[i],1);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elf_Table_Entry it = tab->slots[i];
			// 	if (it.key.tag == elf_tag_Nil) continue;
			// 	if (n ++ != 0) wrote += bprintf(sb,", ");
			// 	wrote += value_bprintf(sb,it.key,1);
			// 	wrote += bprintf(sb," = ");
			// 	wrote += value_bprintf(sb,tab->array[it.i],1);
			// }
			// FOR_ARRAY(t->v) {
			// 	if (i != 0) wrote += bprintf(sb,", ");
			// 	wrote += value_bprintf(sb,t->v[i],1);
			// }
			wrote += bprintf(sb,"}");
			return wrote;
		} break;
		default: return bprintf(sb,"(?)");
	}
}

// todo: handle escape sequences
ELF_FUNCTION(core_lib_format) {
	int argindex = 0;
	char *format = elf_get_text_arg(S, argindex ++);
	String_Builder sb = {};
	while (*format) {
		while (*format && *format != '%') {
			// todo: preallocate a small buffer to avoid
			// one call per char
			bwritechar(&sb, *format ++);
		}
		if (*format == '%') {
			if (argindex >= elf_get_num_args(S)) {
				elf_error(S, NO_BYTE, "not enough arguments to format string!");
			}
			format += 1;
			elf_Value value = elf_get_arg(S, argindex ++);
			value_bprintf(&sb, value, 0);
		}
	}
	elf_push_string(S, sb.buf);
	free(sb.buf);
	return 1;
}

ELF_FUNCTION(core_lib_fpf) {
	elf_Handle file = elf_get_sysarg(S, 0);

	String_Builder sb = {};
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		value_bprintf(&sb, elf_get_arg(S, i), 0);
	}
	elf_push_int(S, sb.min);

	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);
	return 1;
}

ELF_FUNCTION(core_lib_pf) {
	String_Builder sb = {};
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		value_bprintf(&sb, elf_get_arg(S,i),0);
	}
	bprintf(&sb, "\n");

	elf_Handle file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);

	elf_push_int(S, sb.min);
	return 1;
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
		sys_write_file(file, sb.buf, sb.min);
	}
	free(sb.buf);
	elf_push_int(S, noerr);
	return 1;
}

const static elf_Binding lib_core[] = {
	{"get_meta", core_lib_get_meta},
	{"set_meta", core_lib_set_meta},
	{"tagof", core_lib_tagof},
	{"iton", core_lib_iton},
	{"ntoi", core_lib_ntoi},
	{"load_file", core_lib_load_file},
	{"load_expr", core_lib_load_expr},
	{"const_expr", core_lib_const_expr},
	{"load_json", core_lib_load_json},
	{"merge_tables", core_lib_merge_tables},
	{"format", core_lib_format},
	{"fpf", core_lib_fpf},
	{"pf", core_lib_pf},
	{"unload", core_lib_unload},
};


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
