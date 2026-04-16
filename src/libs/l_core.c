//
// See Copyright Notice In elf.h
//






ELF_FUNCTION(l_core_new_buffer) {
	Buf buf = new_buffer(S, 0, 0);
	pushbuf(S, buf);
	return 1;
}









// todo: (readonly ?= true)
ELF_FUNCTION(l_core_get_obj_pointer) {
	pushint(S, (Int) load_reference(S, 1));
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static u32 mark_readonly(elf_State *S, GCRef reference);

static u32 mark_table_readonly(elf_State *state, Table *table)
{
	u32 counter = 0;

	for (u32 i = 0; i < table->nentries; ++ i)
	{
		if (value_is_reference(table->entries[i].key))
		{
			counter += mark_readonly(state, reference_from_value(table->entries[i].key));
		}
	}

	for (u32 i = 0; i < heap_array_length(table->array); ++ i)
	{
		if (value_is_reference(table->array[i]))
		{
			counter += mark_readonly(state, reference_from_value(table->array[i]));
		}
	}
	return counter;
}

static u32 mark_readonly(elf_State *S, GCRef reference)
{
	ASSERT(reference);

	u32 counter = 0;
	if (~reference->status & NODE_READONLY)
	{
		reference->status |= NODE_READONLY;

		counter = 1;

		if (reference->type == GC_TABLE)
		{
			counter += mark_table_readonly(S, (Table *) reference);
		}
	}
	return counter;
}

// todo: (readonly ?= true)
ELF_FUNCTION(l_core_mark_readonly)
{
	loadrulecheck(S, 1, TRULE_OBJECT);

	GCRef ref = load_reference(S, 1);
	u32 counter = mark_readonly(S, ref);
	pushint(S, counter);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ELF_FUNCTION(l_core_get_mem_counter)
{
	pushint(S, S->collector_state.memory_counter);
	return 1;
}

ELF_FUNCTION(l_core_get_obj_counter)
{
	pushint(S, S->collector_state.counter);
	return 1;
}



// todo: this might be temporary!
ELF_FUNCTION(l_core_assert) {
	Int cond = loadint(S, 1);
	const char *errmsg = loadtext(S, 2);
	if (!cond) {
		reporterrorf(S, -1, "assertion triggered: %s", errmsg);
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


// todo: doing this with functions, requires other code to compromise,
// because we get the information from the caller's call frame
ELF_FUNCTION(l_core_nvargs) {
	int n = caller(S).nargs - caller(S).arity;
	if (n < 0) n = 0;
	pushint(S, n);
	return 1;
}


ELF_FUNCTION(l_core_varg) {
	int i = loadint(S, 1);
	int n = caller(S).nargs;
	int a = caller(S).arity;

	int nvargs = n - a;
	if (nvargs < 0) nvargs = 0;

	if (i >= nvargs) {
		push_nil(S);
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
	int i = loadint(S, 1);
	int n = caller(S).nargs;
	if (i >= n) {
		push_nil(S);
	} else {
		V v = caller(S).framebase[i];
		pushvalueunsafe(S, v);
	}
	return 1;
}









ELF_FUNCTION(l_core_get_meta) {
	Tab t = loadtable(S, 1);
	Tab m = getmeta(t);
	pushtab(S, m);
	return 1;
}


// result is the object we passed in
ELF_FUNCTION(l_core_set_meta) {
	Tab t = loadtable(S, 1);
	Tab m = loadtable(S, 2);
	setmeta(t, m);
	pushtab(S, t);
	return 1;
}

ELF_FUNCTION(l_core_is_string) {
	pushint(S, is_string_type(loadtype(S, 1)));
	return 1;
}

ELF_FUNCTION(l_core_is_numeric) {
	pushint(S, is_numeric_type(loadtype(S, 1)));
	return 1;
}

ELF_FUNCTION(l_core_tagof) {
	pushtext(S, tag2s[loadtype(S, 1)]);
	return 1;
}



ELF_FUNCTION(l_core_iton) {
	V v = loadvalue(S, 1);
	if (!is_numeric(v)) {
		reporterrorf(S, -1, "expected numeric value, instead got %s", tag2s[tag_of(v)]);
	}
	pushnum(S, int_to_num(v));
	return 1;
}



ELF_FUNCTION(l_core_ntoi) {
	V v = loadvalue(S, 1);
	if (!is_numeric(v)) {
		reporterrorf(S, -1, "expected numeric value, instead got %s", tag2s[tag_of(v)]);
	}
	pushint(S, num_to_int(v));
	return 1;
}



// todo: support handles!
ELF_FUNCTION(l_core_load_file) {
	int ok = elf_pushcodefile(S, loadtext(S, 1), 0);
	if (!ok) {
		return 0;
	}
	loadpush(S, 0);
	// we need to move the arguments down!
	for (int i = 2; i < nargs; ++ i) {
		loadpush(S, i);
	}
	nrets = elf_do_tail_call(S, nargs - 1, nrets);
	return nrets;
}




// todo: support handles!
ELF_FUNCTION(l_core_load_expr) {
	//	elf_pushcodefile(S, loadtext(S, 1), true);
	//	pushthis(S);
	//	return elf_call(S, 1, nrets);
	// todo:!
	__debugbreak();
	return 0;
}



ELF_FUNCTION(l_core_const_expr) {
	const char *contents = loadtext(S, 1);
	elf_pushconstexpr(S, "no name", contents);
	return 1;
}



// todo: also accept a file handle directly!
ELF_FUNCTION(l_core_load_json) {
	const char *name = loadtext(S, 1);

	Handle file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN);
	if (!file) {
		elf_push_nil(S);
		goto esc;
	}
	unsigned int size = elf_platform_get_file_size(file);
	char *heapbuf = malloc(size);
	elf_platform_read_file(file, heapbuf, size);
	elf_platform_close_file(file);

	elf_load_json(S, name, heapbuf);

	free(heapbuf);

	esc:
	return 1;
}



// todo: this should instead return a string!?
ELF_FUNCTION(l_core_unparse) {
	Sys file = loadsys(S, 1);
	V value = loadvalue(S, 2);

	Stringer sb = {};
	int ok = unparse(S, &sb, value, 0);

	if (ok) {
		sys_write_file(file, sb.buf, sb.min);
	}

	free(sb.buf);

	pushint(S, ok);
	return 1;
}

//
// FORMATTING
//
//
//	todo: terrible at that
//


static int valuetostr(Stringer *sb, V v, bool flags);


// todo: this is slow! if we pre-rank entries for large tables
// it should be much better!
static int tabletostr(Stringer *sb, Tab tab, bool flags) {
	sb_writetext(sb, "{");

	Index i,j,n;
	for (i = 0; i < _table_arraylen(tab); ++ i) {
		if (i != 0) sb_writetext(sb, ", ");

		for (j = 0, n = 0; j < tab->ntotal; ++j) {
			IndexValue en = tab->entries[j];

			if (isdead(en.key)) continue;
			if (en.idx != i) continue;

			if (n ++ != 0) sb_writetext(sb, ", ");
			valuetostr(sb, en.key, 1);
		}
		if (n != 0) sb_writetext(sb, " = ");
		valuetostr(sb, tab->array[i], 1);
	}
	sb_writetext(sb, "}");
	return true;
}

static int valuetostr(Stringer *sb, V v, bool flags) {
	switch (v.tag) {
		case ELF_VALUE_TYPE_NIL:        return sb_writetextf(sb, "nil"                  );
		case ELF_VALUE_TYPE_INTEGER:    return sb_writetextf(sb, "%lli"  , v.x_int      );
		case ELF_VALUE_TYPE_NUMBER:     return sb_writetextf(sb, "%f"    , v.x_num      );
		case ELF_VALUE_TYPE_HANDLE:     return sb_writetextf(sb, "h%llX" , v.x_int      );
		case ELF_VALUE_TYPE_STRING:     return sb_writetextf(sb, "%s"    , v.x_str->text);
		case ELF_VALUE_TYPE_CLOSURE:    return sb_writetextf(sb, "C()");
		case ELF_VALUE_TYPE_CFUNCTION:   return sb_writetextf(sb, "F()");
		case ELF_VALUE_TYPE_BUFFER:     return   sb_writebuf(sb, as_buffer(v));
		case ELF_VALUE_TYPE_TABLE:      return    tabletostr(sb, table_from_value(v), flags);
		default: return sb_writetextf(sb,"(?)");
	}
}

//
// todo: handle escape sequences
// todo: make it actually handle format specifiers!
//
// @doc: takes a format string similar to that of a c printf
// function, the type does not have to be specified
//
ELF_FUNCTION(l_core_format) {

	int index = 1;
	const char *format = loadtext(S, index ++);

	Stringer sb = {};
	while (*format) {

		while (*format && *format != '%') {
			// todo: preallocate a small buffer to avoid
			// one call per char
			sb_writechar(&sb, *format ++);
		}

		if (*format == '%') {
			if (index >= nargs) {
				reporterror(S, NO_BYTE, "not enough arguments to format string!");
			}
			format += 1;

			// todo:
			V value = loadvalue(S, index ++);
			valuetostr(&sb, value, 0);
		}
	}

	pushtext2(S, sb.buf, sb.min);
	free(sb.buf);
	return 1;
}

ELF_FUNCTION(l_core_print) {
	Stringer sb = {};
	for (int i = 1; i < nargs; i ++) {
		valuetostr(&sb, loadvalue(S,i),0);
	}

	Sys file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, sb.buf, sb.min);
	free(sb.buf);

	pushint(S, sb.min);
	return 1;
}

ELF_FUNCTION(l_core_printl_csv) {
	Stringer sb = {};
	for (int i = 1; i < nargs; i ++) {
		if (i != 1) sb_writetext(&sb, ", ");
		valuetostr(&sb, loadvalue(S,i),0);
	}
	sb_writechar(&sb, '\n');

	Sys file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, sb.buf, sb.min);
	free(sb.buf);

	pushint(S, sb.min);
	return 1;
}

ELF_FUNCTION(l_core_printl) {
	Stringer sb = {};
	for (int i = 1; i < nargs; i ++) {
		valuetostr(&sb, loadvalue(S,i),0);
	}
	sb_writetextf(&sb, "\n");

	Sys file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, sb.buf, sb.min);
	free(sb.buf);

	pushint(S, sb.min);
	return 1;
}

//
// todo: can we deprecate this? who cares about this anymore?
// we can use buffers... and the user can just write to file.
//
ELF_FUNCTION(l_core_fprintl) {
	Handle file = loadsys(S, 1);

	Stringer sb = {};
	for (int i = 2; i < nargs; i ++) {
		valuetostr(&sb, loadvalue(S, i), 0);
	}
	pushint(S, sb.min);

	sys_write_file(file, sb.buf, sb.min);

	free(sb.buf);
	return 1;
}




const static elf_Binding l_core[] = {
	{"get_meta",        l_core_get_meta        },
	{"set_meta",        l_core_set_meta        },
	{"mark_readonly",   l_core_mark_readonly   },

	{"assert",          l_core_assert          },
	{"new_buffer",      l_core_new_buffer      },

	{"get_mem_counter", l_core_get_mem_counter },
	{"get_obj_counter", l_core_get_obj_counter },
	{"get_obj_pointer", l_core_get_obj_pointer },

	{"nvargs",          l_core_nvargs          },
	{"varg",            l_core_varg            },
	{"nrets",           l_core_nrets           },
	{"nargs",           l_core_nargs           },
	{"arg",             l_core_arg             },

	{"tagof",           l_core_tagof           },

	{"is_str",          l_core_is_string       },
	{"is_numeric",      l_core_is_numeric      },

	{"iton",            l_core_iton            },
	{"ntoi",            l_core_ntoi            },

	{"load_file",       l_core_load_file  },
	{"load_expr",       l_core_load_expr  },
	{"const_expr",      l_core_const_expr },
	{"load_json",       l_core_load_json  },
	{"unparse",         l_core_unparse    },

	{"format",          l_core_format     },
	{"printl",          l_core_printl     },
	{"printl_csv",      l_core_printl_csv },
	{"print",           l_core_print      },

	// todo: deprecated?
	{"fprint",     l_core_fprintl              },
	{"fpf"   ,     l_core_fprintl              },
	{"pf"    ,     l_core_printl           },
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
	char *dir = loadtext(R, 1);
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
	IndexValue entry;
	FOR_RANGE(i,0,globals->ntotal) {
		entry=globals->slots[i];
		if (entry.key.tag == ELF_VALUE_TYPE_STRING) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = new_string_from_data(R,sym);
			tableset(globals,VALUE_STRING(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}
#endif
