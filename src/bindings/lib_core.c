//
// See Copyright Notice In elf.h
//

// todo: we need something that hashes the pointer
// with more weight, and the pointer hash should weight
// the middle bits more, since the low bits are usually
// empty because of alignment, and the high bits remain
// similar because allocations tend to be smaller
// So most of the sporadic distribution should come from
// the pointer, but it shouldn't be too much because most
// objects tend to have few fields.
ELF_FUNCTION(l_core_registryhash64) {
	elf_Integer ptr = S->stack[args + 1].x_int;
	// elf_String *str = S->stack[args + 2].x_str;
	// hash_t hsh = str->hash + 0x9e3779b97f4a7c15ULL + (ptr << 6) + (ptr >> 2);
	char buf[64] = {};
	const char *str = elf_tostr(S, args + 2);
	memcpy(buf, &ptr, sizeof(ptr));
	memcpy(buf, str, strlen(str) + 1);
	// ASSERT(strlen(str) + 1 < 64 - 8);
	elf_Integer hsh = hash_text(buf);
	elf_pushint(S, hsh);
	return 1;
}


ELF_FUNCTION(l_core_get_meta) {
	elf_pushmetatab(S, args + 1);
	return 1;
}

ELF_FUNCTION(l_core_set_meta) {
	elf_setmetatab(S, args + 1, args + 2);
	elf_pushmetatab(S, args + 1);
	return 1;
}

ELF_FUNCTION(l_core_tagof) {
	elf_pushstr(S, tag2s[elf_gettag(S, args + 1)]);
	return 1;
}

ELF_FUNCTION(l_core_iton) {
	elf_Value v = elf_get_arg(S, 0);
	if (!isnumeric(v)) {
		elf_error(S, -1, "expected numeric value");
	}
	elf_pushnum(S, vitonum(v));
	return 1;
}

ELF_FUNCTION(l_core_ntoi) {
	elf_Value v = elf_get_arg(S, 0);
	if (!isnumeric(v)) {
		elf_error(S, -1, "expected numeric value");
	}
	elf_pushint(S, vntoint(v));
	return 1;
}


ELF_FUNCTION(l_core_load_file) {
	elf_pushstr(S, "noname");
	elf_readfile(S, args + 1, -1);
	elf_loadcode(S, false);
	elf_pushnil(S);
	return elf_call(S, 1, nrets);
}


ELF_FUNCTION(l_core_load_expr) {
	elf_pushstr(S, "noname");
	elf_readfile(S, args + 1, -1);
	elf_loadcode(S, true);
	elf_pushnil(S);
	return elf_call(S, 1, nrets);
}


ELF_FUNCTION(l_core_const_expr) {
	const char *contents = elf_tostr(S, args + 1);
	elf_load_const_expr(S, "no name", contents);
	return 1;
}

// todo: also accept a file handle directly!
ELF_FUNCTION(l_core_load_json) {
	const char *name = elf_tostr(S, args + 1);

	elf_Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN);
	if (!file) {
		elf_pushnil(S);
		goto esc;
	}

	unsigned int size = sys_size_file(file);

	char *heapbuf = malloc(size);

	sys_read_file(file, heapbuf, size);

	sys_close_file(file);

	elf_load_json(S, name, heapbuf);

	free(heapbuf);

	esc:
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

			index_t i,j,n;
			for (i=0;i<arrlen(tab->array);++i) {
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
//
// @doc: takes a format string similar to that of a c printf
// function, the type does not have to be specified
//
ELF_FUNCTION(l_core_format) {
	int argindex = 1;

	const char *format = elf_tostr(S, args + argindex);

	String_Builder sb = {};
	while (*format) {

		while (*format && *format != '%') {
			// todo: preallocate a small buffer to avoid
			// one call per char
			bwritechar(&sb, *format ++);
		}

		if (*format == '%') {
			if (argindex >= (nargs - 1)) {
				elf_error(S, NO_BYTE, "not enough arguments to format string!");
			}
			format += 1;

			// todo:
			elf_Value value = elf_get_arg(S, argindex ++);
			value_bprintf(&sb, value, 0);
		}
	}

	elf_pushstr(S, sb.buf);
	free(sb.buf);
	return 1;
}

ELF_FUNCTION(l_core_fpf) {
	elf_Handle file = elf_get_sysarg(S, 0);

	String_Builder sb = {};
	for (int i = 1; i < (nargs - 1); i ++) {
		value_bprintf(&sb, elf_get_arg(S, i), 0);
	}
	elf_pushint(S, sb.min);

	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);
	return 1;
}

ELF_FUNCTION(l_core_pf) {
	String_Builder sb = {};
	for (int i = 0; i < (nargs - 1); i ++) {
		value_bprintf(&sb, elf_get_arg(S,i),0);
	}
	bprintf(&sb, "\n");

	elf_Handle file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);

	elf_pushint(S, sb.min);
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
				index_t i;
				for (i = 0; i < table->ntotal; ++ i) {
					elf_Table_Entry entry = table->slots[i];
					index_t index = entry.idx;
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


ELF_FUNCTION(l_core_unload) {
	elf_Handle file = elf_get_sysarg(S, 0);
	elf_Value thing = elf_get_arg(S, 1);
	String_Builder sb = {};
	int noerr = unparse(S, &sb, thing, 0);
	if (noerr) {
		sys_write_file(file, sb.buf, sb.min);
	}
	free(sb.buf);
	elf_pushint(S, noerr);
	return 1;
}

const static elf_Binding lib_core[] = {
	{"get_meta", l_core_get_meta},
	{"set_meta", l_core_set_meta},
	{"tagof", l_core_tagof},
	{"iton", l_core_iton},
	{"ntoi", l_core_ntoi},
	{"load_file", l_core_load_file},
	{"load_expr", l_core_load_expr},
	{"const_expr", l_core_const_expr},
	{"load_json", l_core_load_json},
	{"format", l_core_format},
	{"fpf", l_core_fpf},
	{"pf", l_core_pf},
	{"unload", l_core_unload},
	{"registryhash64", l_core_registryhash64},
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
			elf_raw_table_set(globals,VALUE_STRING(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}
#endif
