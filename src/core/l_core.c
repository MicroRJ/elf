//
// See Copyright Notice In elf.h
//



ELF_FUNCTION(l_core_assert) {
	Int cond = lint(S, 1);
	const char *emsg = lstrdata(S, 2);
	if (!cond) {
		elf_errorf(S, -1, "assertion triggered: %s", emsg);
	}
	return 0;
}



/* ======= Arguments API: ====================================

	Get Parent Call Frame Information

 	- elf.arg(i):       Returns the i-th argument of the caller.
 	- elf.varg(i):      Returns the i-th variadic argument (extra argument beyond declared params).
 	- elf.nargs():      Returns the total number of arguments passed to the caller.
 	- elf.nvargs():     Returns the number of variadic (extra) arguments passed.
 	- elf.nrets():      Returns the number of expected results for the caller's function call.

 	** Out-of-bounds argument accesses return nil.
============================================================== */


#define caller(S) ((S)->frame_stack[(S)->frame_index - 1])


ELF_FUNCTION(l_core_nvargs) {
	int n = caller(S).nargs - caller(S).arity;
	if (n < 0) n = 0;
	pushint(S, n);
	return 1;
}


ELF_FUNCTION(l_core_varg) {
	int i = lint(S, 1);
	int n = caller(S).nargs;
	int a = caller(S).arity;

	int nvargs = n - a;
	if (nvargs < 0) nvargs = 0;

	if (i >= nvargs) {
		pushnil(S);
	} else {
		V v = caller(S).framebase[a + i];
		pushvalueunsafe(S, v);
	}
	return 1;
}


ELF_FUNCTION(l_core_nrets) {
	int n = caller(S).nrets;
	pushint(S, n);
	return 1;
}


ELF_FUNCTION(l_core_nargs) {
	int n = caller(S).nargs;
	pushint(S, n);
	return 1;
}


ELF_FUNCTION(l_core_arg) {
	int i = lint(S, 1);
	int n = caller(S).nargs;
	if (i >= n) {
		pushnil(S);
	} else {
		V v = caller(S).framebase[i];
		pushvalueunsafe(S, v);
	}
	return 1;
}









ELF_FUNCTION(l_core_get_meta) {
	TRef t = ltable(S, 1);
	TRef m = getmeta(t);
	pushtab(S, m);
	return 1;
}


// result is the object we passed in
ELF_FUNCTION(l_core_set_meta) {
	TRef t = ltable(S, 1);
	TRef m = ltable(S, 2);
	setmeta(t, m);
	pushtab(S, t);
	return 1;
}


ELF_FUNCTION(l_core_tagof) {
	elf_pushstr(S, tag2s[elf_gettag(S, args + 1)]);
	return 1;
}


ELF_FUNCTION(l_core_iton) {
	elf_Value v = loadvalue(S, 0);
	if (!visnumeric(v)) {
		elf_error(S, -1, "expected numeric value");
	}
	elf_pushnum(S, vitonum(v));
	return 1;
}


ELF_FUNCTION(l_core_ntoi) {
	elf_Value v = loadvalue(S, 0);
	if (!visnumeric(v)) {
		elf_error(S, -1, "expected numeric value");
	}
	elf_pushint(S, vntoint(v));
	return 1;
}


// todo: support handles!
ELF_FUNCTION(l_core_load_file) {
	elf_loadcodefile(S, lstrdata(S, 1));
	pushthis(S);
	return elf_call(S, 1, nrets);
}


// todo: support handles!
ELF_FUNCTION(l_core_load_expr) {
	//	elf_loadcodefile(S, lstrdata(S, 1), true);
	//	pushthis(S);
	//	return elf_call(S, 1, nrets);
	// todo:!
	__debugbreak();
	return 0;
}


ELF_FUNCTION(l_core_const_expr) {
	const char *contents = lstrdata(S, 1);
	elf_load_const_expr_from_text(S, "no name", contents);
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
		case ELF_TNIL:        return bprintf(sb,   "nil"               );
		case ELF_TINTEGER:        return bprintf(sb,  "%lli", v.x_int      );
		case ELF_TNUMBER:        return bprintf(sb,    "%f", v.x_num      );
		case ELF_THANDLE:     return bprintf(sb, "h%llX", v.x_int      );
		case ELF_TSTRING:     return bprintf(sb,    "%s", v.x_str->text);
		case ELF_TCLOSURE:    return bprintf(sb,   "F()");
		case ELF_TFUNCTION:   return bprintf(sb,   "C()");

		case ELF_TTABLE: {
			/* todo: this is slow! */
			int wrote = 0;
			elf_Table *tab = v.x_tab;
			wrote += bprintf(sb, "{");

			index_t i,j,n;
			for (i=0;i<darr_l(tab->array);++i) {
				if (i != 0) wrote += bprintf(sb, ", ");
				for (j=0,n=0;j<tab->ntotal;++j) {
					elf_Table_Entry it = tab->slots[j];
					if (it.key.tag==ELF_TNIL) continue;
					if (it.idx!=i) continue;
					if (n ++ != 0) wrote += bprintf(sb, ", ");
					wrote += value_bprintf(sb,it.key,1);
				}
				if (n != 0) wrote += bprintf(sb, " = ");
				wrote += value_bprintf(sb,tab->array[i],1);
			}
			// for (i=0,n=0;i<tab->nslots;++i) {
			// 	elf_Table_Entry it = tab->slots[i];
			// 	if (it.key.tag == ELF_TNIL) continue;
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

	int index = 1;
	const char *format = lstrdata(S, index ++);

	String_Builder sb = {};
	while (*format) {

		while (*format && *format != '%') {
			// todo: preallocate a small buffer to avoid
			// one call per char
			bwritechar(&sb, *format ++);
		}

		if (*format == '%') {
			if (index >= nargs) {
				elf_error(S, NO_BYTE, "not enough arguments to format string!");
			}
			format += 1;

			// todo:
			V value = lvalue(S, index ++);
			value_bprintf(&sb, value, 0);
		}
	}

	elf_pushstr(S, sb.buf);
	free(sb.buf);
	return 1;
}

ELF_FUNCTION(l_core_fpf) {
	elf_Handle file = f_checkhand(S, 0);

	String_Builder sb = {};
	for (int i = 1; i < (nargs - 1); i ++) {
		value_bprintf(&sb, loadvalue(S, i), 0);
	}
	elf_pushint(S, sb.min);

	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);
	return 1;
}

ELF_FUNCTION(l_core_printl) {
	String_Builder sb = {};
	for (int i = 0; i < (nargs - 1); i ++) {
		value_bprintf(&sb, loadvalue(S,i),0);
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
		case ELF_TNIL:    bprintf(sb, "nil"                       ); break;
		case ELF_TINTEGER:    bprintf(sb, "%lli"  , thing.x_int       ); break;
		case ELF_TNUMBER:    bprintf(sb, "%f"    , thing.x_num       ); break;
		case ELF_TSTRING: bprintf(sb, "\"%s\"", thing.x_str->text ); break;
		case ELF_TTABLE: {
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

					if ((key.tag != ELF_TNUMBER)
					&&  (key.tag != ELF_TINTEGER)
					&&  (key.tag != ELF_TSTRING))
					{
						continue;
					}

					elf_Value value = table->array[index];
					if ((value.tag != ELF_TNUMBER)
					&&  (value.tag != ELF_TINTEGER)
					&&  (value.tag != ELF_TTABLE)
					&&  (value.tag != ELF_TSTRING))
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
					if ((value.tag != ELF_TNUMBER)
					&&  (value.tag != ELF_TINTEGER)
					&&  (value.tag != ELF_TTABLE)
					&&  (value.tag != ELF_TSTRING))
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
	elf_Handle file = f_checkhand(S, 0);
	elf_Value thing = loadvalue(S, 1);
	String_Builder sb = {};
	int noerr = unparse(S, &sb, thing, 0);
	if (noerr) {
		sys_write_file(file, sb.buf, sb.min);
	}
	free(sb.buf);
	elf_pushint(S, noerr);
	return 1;
}

const static elf_Binding l_core[] = {
	{"get_meta", l_core_get_meta},
	{"set_meta", l_core_set_meta},
	{"assert", l_core_assert},
	{"nvargs", l_core_nvargs },
	{"varg", l_core_varg },
	{"nrets", l_core_nrets },
	{"nargs", l_core_nargs },
	{"arg", l_core_arg },
	{"tagof", l_core_tagof},
	{"iton", l_core_iton},
	{"ntoi", l_core_ntoi},
	{"load_file", l_core_load_file},
	{"load_expr", l_core_load_expr},
	{"const_expr", l_core_const_expr},
	{"load_json", l_core_load_json},
	{"format", l_core_format},
	{"fprint", l_core_fpf},
	{"printl", l_core_printl},
	{"unload", l_core_unload},

	// todo: deprecated?
	{"fpf", l_core_fpf},
	{"pf", l_core_printl},
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
	char *dir = f_checktext(R,0);
	int plen = text_l(dir);
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
		if (entry.key.tag == ELF_TSTRING) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_raw_table_set(globals,VALUE_STRING(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}
#endif
