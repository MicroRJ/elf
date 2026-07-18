//
// See Copyright Notice In elf.h
//








// todo: (readonly ?= true)
ELF_FUNCTION(l_core_get_obj_pointer) {
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_OBJECT);
	push_value(S, value_from_integer((i64)value_as_object(value)));
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static u32 mark_readonly(elf_State *S, elf_Object * reference);

static u32 mark_table_readonly(elf_State *state, elf_Table *table)
{
	u32 reference_count = 0;

	for (u32 i = 0; i < table->nentries; ++ i)
	{
		elf_Value key = entry_key_value(table->entries[i]);
		if (value_is_object(key))
		{
			reference_count += mark_readonly(state, value_as_object(key));
		}
	}

	for (u32 i = 0; i < elf_array_len(table); ++ i)
	{
		elf_Value value = elf_array_get(state, table, i);
		if (value_is_object(value))
		{
			reference_count += mark_readonly(state, value_as_object(value));
		}
	}
	return reference_count;
}

static u32 mark_readonly(elf_State *S, elf_Object * reference)
{
	ASSERT(reference);

	u32 reference_count = 0;
	if (~reference->status & ELF_OBJECT_READONLY)
	{
		reference->status |= ELF_OBJECT_READONLY;

		reference_count = 1;

		if (reference->type == ELF_OBJECT_TABLE)
		{
			reference_count += mark_table_readonly(S, (elf_Table *) reference);
		}
	}
	return reference_count;
}

// todo: (readonly ?= true)
ELF_FUNCTION(l_core_mark_readonly)
{
	elf_Value value = load_value(S, 1);
	check_value_type_rule(S, value, TRULE_OBJECT);
	elf_Object * ref = value_as_object(value);
	u32 reference_count = mark_readonly(S, ref);
	push_value(S, value_from_integer(reference_count));
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ELF_FUNCTION(l_core_get_mem_counter)
{
	push_value(S, value_from_integer(S->gc_live_bytes));
	return 1;
}

ELF_FUNCTION(l_core_get_obj_counter)
{
	push_value(S, value_from_integer(S->gc_reference_count));
	return 1;
}



// todo: this might be temporary!
ELF_FUNCTION(l_core_assert) {
	elf_Value cond_value = load_value(S, 1);
	check_value_type_rule(S, cond_value, TRULE_NUMERIC);
	i64 cond = value_to_integer(cond_value);

	elf_Value error_value = load_value(S, 2);
	check_value_type(S, error_value, ELF_VALUE_TYPE_ATOM);
	const char *errmsg = atom_data(value_as_atom(error_value));
	if (!cond) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "assertion triggered: %s", errmsg);
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
	if (!caller(S).variadic) {
		push_value(S, value_from_integer(0));
		return 1;
	}

	i64 n = caller(S).nargs - caller(S).arity;
	if (n < 0) n = 0;
	push_value(S, value_from_integer(n));
	return 1;
}


ELF_FUNCTION(l_core_varg) {
	if (!caller(S).variadic) {
		push_value(S, value_nil());
		return 1;
	}

	elf_Value index_value = load_value(S, 1);
	check_value_type_rule(S, index_value, TRULE_NUMERIC);
	i64 i = value_to_integer(index_value);
	i64 n = caller(S).nargs;
	i64 a = caller(S).arity;

	i64 nvargs = n - a;
	if (nvargs < 0) nvargs = 0;

	if (i >= nvargs) {
		push_value(S, value_nil());
	} else {
		elf_Value value = caller(S).framebase[a + i];
		push_value(S, value);
	}
	return 1;
}


ELF_FUNCTION(l_core_nrets) {
	i64 n = caller(S).nrets;
	push_value(S, value_from_integer(n));
	return 1;
}


ELF_FUNCTION(l_core_nargs) {
	i64 n = caller(S).nargs;
	push_value(S, value_from_integer(n));
	return 1;
}


ELF_FUNCTION(l_core_arg) {
	elf_Value index_value = load_value(S, 1);
	check_value_type_rule(S, index_value, TRULE_NUMERIC);
	i64 i = value_to_integer(index_value);
	i64 n = caller(S).nargs;
	if (i >= n) {
		push_value(S, value_nil());
	} else {
		elf_Value value = caller(S).framebase[i];
		push_value(S, value);
	}
	return 1;
}









ELF_FUNCTION(l_core_get_meta) {
	elf_Value value = load_value(S, 1);
	elf_Table *meta = elf_get_type_metatable(S, value);
	if (meta) {
		push_table(S, meta);
	}
	else {
		elf_push_nil(S);
	}
	return 1;
}

ELF_FUNCTION(l_core_is_atom) {
	push_value(S, value_from_integer(elf_value_type_is_atom(value_type(load_value(S, 1)))));
	return 1;
}

ELF_FUNCTION(l_core_is_numeric) {
	push_value(S, value_from_integer(elf_value_type_is_numeric(value_type(load_value(S, 1)))));
	return 1;
}

ELF_FUNCTION(l_core_tagof) {
	push_value(S, value_from_atom(elf_atom_from_data(S, value_type_name(value_type(load_value(S, 1))))));
	return 1;
}



ELF_FUNCTION(l_core_iton) {
	elf_Value value = load_value(S, 1);
	if (!value_is_numeric(value)) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "expected numeric value, instead got %s", value_type_name(value_type(value)));
	}
	push_value(S, value_from_number(value_to_number(value)));
	return 1;
}



ELF_FUNCTION(l_core_ntoi) {
	elf_Value value = load_value(S, 1);
	if (!value_is_numeric(value)) {
		report_runtime_error(S, RUNTIME_ERROR_GENERIC, -1, "expected numeric value, instead got %s", value_type_name(value_type(value)));
	}
	push_value(S, value_from_integer(value_to_integer(value)));
	return 1;
}



// todo: support handles!
ELF_FUNCTION(l_core_load_file) {
	elf_Value name_value = load_value(S, 1);
	check_value_type(S, name_value, ELF_VALUE_TYPE_ATOM);
	i64 ok = elf_push_code_file(S, atom_data(value_as_atom(name_value)));
	if (!ok) {
		return 0;
	}
	push_value(S, load_value(S, 0));
	// we need to move the arguments down!
	for (i64 i = 2; i < nargs; ++ i) {
		push_value(S, load_value(S, i));
	}
	nrets = elf_do_tail_call(S, nargs - 1, nrets);
	return nrets;
}




// todo: support handles!
ELF_FUNCTION(l_core_load_expr) {
	//	elf_push_code_source(S, ..., source);
	//	push_value(S, load_value(S, 0));
	//	return elf_call(S, 1, nrets);
	// todo:!
	__debugbreak();
	return 0;
}



ELF_FUNCTION(l_core_const_expr) {
	elf_Value contents_value = load_value(S, 1);
	check_value_type(S, contents_value, ELF_VALUE_TYPE_ATOM);
	const char *contents = atom_data(value_as_atom(contents_value));
	elf_StrSlice source = {(char *)contents, strlen(contents)};
	elf_push_constant_expr(S, "no name", source);
	return 1;
}



// todo: also accept a file handle directly!
ELF_FUNCTION(l_core_load_json) {
	elf_Value name_value = load_value(S, 1);
	check_value_type(S, name_value, ELF_VALUE_TYPE_ATOM);
	const char *name = atom_data(value_as_atom(name_value));

	elf_Handle file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (ELF_IS_HANDLE_INVALID(file)) {
		elf_push_nil(S);
		goto esc;
	}
	u64 size = elf_platform_get_file_size(file);
	char *heapbuf = malloc(size + 1);
	elf_platform_read_file(file, heapbuf, size);
	heapbuf[size] = 0;
	elf_platform_close_file(file);

	elf_StrSlice source = {heapbuf, size};
	elf_push_json(S, name, source);

	free(heapbuf);

	esc:
	return 1;
}



// todo: this should instead return a atom!?
ELF_FUNCTION(l_core_unparse) {
	elf_Value file_value = load_value(S, 1);
	check_value_type(S, file_value, ELF_VALUE_TYPE_HANDLE);
	elf_Handle file = value_as_handle(file_value);
	elf_Value value = load_value(S, 2);

	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	b32 ok = serialize_value(S, scratch.arena, value, 0);
	char *end = arena_push_zero(scratch.arena, 1);

	if (ok) {
		sys_write_file(file, start, (i32)(end - start));
	}

	end_scratch(scratch);

	push_value(S, value_from_integer(ok));
	return 1;
}

//
// todo: handle escape sequences
// todo: make it actually handle format specifiers!
//
// @doc: takes a format atom similar to that of a c printf
// function, the type does not have to be specified
//
ELF_FUNCTION(l_core_format) {

	i64 index = 1;
	elf_Value format_value = load_value(S, index ++);
	check_value_type(S, format_value, ELF_VALUE_TYPE_ATOM);
	const char *format = atom_data(value_as_atom(format_value));

	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	while (*format) {

		while (*format && *format != '%') {
			arena_push_char(scratch.arena, *format ++);
		}

		if (*format == '%') {
			if (index >= nargs) {
				report_runtime_error(S, RUNTIME_ERROR_GENERIC, NO_BYTE, "not enough arguments to format atom!");
			}
			format += 1;

			// todo:
			elf_Value value = load_value(S, index ++);
			print_value(scratch.arena, value);
		}
	}

	char *end = arena_push_zero(scratch.arena, 1);
	push_value(S, value_from_atom(elf_atom_from_data_size(S, start, (u32)(end - start))));
	end_scratch(scratch);
	return 1;
}

ELF_FUNCTION(l_core_print) {
	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	for (i64 i = 1; i < nargs; i ++) {
		print_value(scratch.arena, load_value(S,i));
	}
	char *end = arena_push_zero(scratch.arena, 1);
	u32 size = (u32)(end - start);

	elf_Handle file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, start, size);

	end_scratch(scratch);
	push_value(S, value_from_integer(size));
	return 1;
}

ELF_FUNCTION(l_core_printl_csv) {
	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	for (i64 i = 1; i < nargs; i ++) {
		if (i != 1) arena_push_text(scratch.arena, ", ");
		print_value(scratch.arena, load_value(S,i));
	}
	arena_push_char(scratch.arena, '\n');
	char *end = arena_push_zero(scratch.arena, 1);
	u32 size = (u32)(end - start);

	elf_Handle file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, start, size);

	end_scratch(scratch);
	push_value(S, value_from_integer(size));
	return 1;
}

ELF_FUNCTION(l_core_printl) {
	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	for (i64 i = 1; i < nargs; i ++) {
		print_value(scratch.arena, load_value(S,i));
	}
	arena_push_char(scratch.arena, '\n');
	char *end = arena_push_zero(scratch.arena, 1);
	u32 size = (u32)(end - start);

	elf_Handle file = sys_get_std_file(SYS_STD_OUTPUT);
	sys_write_file(file, start, size);

	end_scratch(scratch);
	push_value(S, value_from_integer(size));
	return 1;
}

ELF_FUNCTION(l_core_fprintl) {
	elf_Value file_value = load_value(S, 1);
	check_value_type(S, file_value, ELF_VALUE_TYPE_HANDLE);
	elf_Handle file = value_as_handle(file_value);

	Scratch scratch = get_scratch();
	char *start = arena_push(scratch.arena, 0);
	for (i64 i = 2; i < nargs; i ++) {
		print_value(scratch.arena, load_value(S, i));
	}
	char *end = arena_push_zero(scratch.arena, 1);
	u32 size = (u32)(end - start);
	push_value(S, value_from_integer(size));

	sys_write_file(file, start, size);

	end_scratch(scratch);
	return 1;
}




const static elf_Binding l_core[] = {
	{"get_meta",        l_core_get_meta        },
	{"mark_readonly",   l_core_mark_readonly   },

	{"assert",          l_core_assert          },

	{"get_mem_counter", l_core_get_mem_counter },
	{"get_obj_counter", l_core_get_obj_counter },
	{"get_obj_pointer", l_core_get_obj_pointer },

	{"nvargs",          l_core_nvargs          },
	{"varg",            l_core_varg            },
	{"nrets",           l_core_nrets           },
	{"nargs",           l_core_nargs           },
	{"arg",             l_core_arg             },

	{"tagof",           l_core_tagof           },

	{"value_is_atom",          l_core_is_atom       },
	{"value_is_numeric",      l_core_is_numeric      },

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

static elf_Table *elf_lib_core(elf_State *state)
{
	return new_binding_table(state, l_core, ARRAY_COUNT(l_core));
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
i64 core_lib_include(elf_State *R) {
	elf_check_num_args(R,".include",1,"(the directory to include to add to the global directory)");
	elf_Value dir_value = load_value(R, 1);
	check_value_type(R, dir_value, ELF_VALUE_TYPE_ATOM);
	char *dir = value_as_atom(dir_value)->data;
	i64 plen = (i64)strlen(dir);
	/* accumulate all symbols here first to
	avoid faulting under repeating patterns:
	elf.ray.elf.ray could include the symbol
	many more times when the new key is added
	as we traverse the elf_Value *. the new key is
	encountered and we keep repeating the
	process... this would override the previous
	elf_Value and result in erroneous behavior.
	todo: */

	elf_Table *globals = R->globals;
	Entry entry;
	FOR_RANGE(i,0,globals->nentries) {
		entry=globals->entries[i];
		elf_Value key = entry_key_value(entry);
		if (value_is_atom(key)) {
			char *sym = in_sym_dir(dir, key.x_atom->data);
			if (*sym != '.') continue;
			elf_String *ref = elf_atom_from_data(R,sym);
			elf_table_set(R, globals, VALUE_ATOM(ref), elf_array_get(R, globals, entry_index(entry)));
		}
	}
	return 0;
}
#endif
