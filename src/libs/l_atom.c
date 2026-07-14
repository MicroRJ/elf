//
// See Copyright Notice In elf.h
//
ELF_FUNCTION(l_atom_size) {
	elf_Value value = load_value(S, 0);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	push_value(S, value_from_integer(value_as_atom(value)->size));
	return 1;
}




ELF_FUNCTION(l_atom_get_hash) {
	elf_Value value = load_value(S, 0);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	push_value(S, value_from_integer(value_as_atom(value)->hash));
	return 1;
}



ELF_FUNCTION(l_atom_idx) {
	elf_Value text_value = load_value(S, 0);
	check_value_type(S, text_value, ELF_VALUE_TYPE_ATOM);
	const char *text = elf_atom_data(value_as_atom(text_value));

	elf_Value index_value = load_value(S, 1);
	check_value_type_rule(S, index_value, TRULE_NUMERIC);
	int index = value_to_integer(index_value);
	push_value(S, value_from_integer(text[index]));
	return 1;
}



ELF_FUNCTION(l_atom_slice) {
	elf_Value value = load_value(S, 0);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *str = value_as_atom(value);
	// todo: out of bounds check
	elf_Value lo_value = load_value(S, 2);
	check_value_type_rule(S, lo_value, TRULE_NUMERIC);
	int lo = value_to_integer(lo_value);

	elf_Value hi_value = load_value(S, 3);
	check_value_type_rule(S, hi_value, TRULE_NUMERIC);
	int hi = value_to_integer(hi_value);
	push_value(S, value_from_atom(elf_atom_from_data_size(S, str->data + lo, hi)));
	return 1;
}


ELF_FUNCTION(l_atom_join) {
	Scratch scratch = get_scratch();

	char *start = arena_push(scratch.arena, 0);
	for (int i = 0; i < nargs; ++ i) {
		print_value(scratch.arena, load_value(S, i));
	}
	char *end = arena_push_zero(scratch.arena, 1);

	elf_Atom *atom = elf_atom_from_data_size(S, start, end - start);
	push_value(S, value_from_atom(atom));
	end_scratch(scratch);
	return 1;
}



// todo: what if multiple inputs!
// @doc whether the atom matches the passed in pattern
ELF_FUNCTION(l_atom_match) {
	elf_Value text_value = load_value(S, 0);
	elf_Value pattern_value = load_value(S, 1);
	check_value_type(S, text_value, ELF_VALUE_TYPE_ATOM);
	check_value_type(S, pattern_value, ELF_VALUE_TYPE_ATOM);
	const char *s = elf_atom_data(value_as_atom(text_value));
	const char *p = elf_atom_data(value_as_atom(pattern_value));
	const char *match = matcher_match(s, p);
	push_value(S, value_from_integer(match != 0));
	return 1;
}



// todo: doesn't actually work
// @doc finds all the matches and returns a list of all the atoms
ELF_FUNCTION(l_atom_find) {
	return 0;
}



// @doc returns an list of lines from this atom
ELF_FUNCTION(l_atom_split_by_lines) {
	elf_Value text_value = load_value(S, 0);
	check_value_type(S, text_value, ELF_VALUE_TYPE_ATOM);
	const char *s = elf_atom_data(value_as_atom(text_value));

	elf_push_new_table(S);

	Scratch scratch = get_scratch();

	char *cur = (char *) s;
	while (*cur) {
		u64 start_pos = scratch.arena->in_use;
		char *start = arena_push(scratch.arena, 0);

		while (*cur != 0 && *cur != '\n' && *cur != '\r') {
			arena_push_char(scratch.arena, *cur ++);
		}
		if (*cur == '\n' || *cur == '\r') {
			// skip additional char if \r\n, windows style line ending?
			cur += 1 + (cur[0] == '\r' && cur[1] == '\n');
		}

		char *end = arena_push_zero(scratch.arena, 1);
		push_value(S, value_from_atom(elf_atom_from_data_size(S, start, (u32)(end - start))));
		elf_arrayadd(S);

		scratch.arena->in_use = start_pos;
	}

	end_scratch(scratch);

	return 1;
}



ELF_FUNCTION(l_atom_split_by_char) {

	elf_Value text_value = load_value(S, 0);
	check_value_type(S, text_value, ELF_VALUE_TYPE_ATOM);
	const char *s = elf_atom_data(value_as_atom(text_value));

	elf_Value chr_value = load_value(S, 1);
	check_value_type_rule(S, chr_value, TRULE_NUMERIC);
	int chr = value_to_integer(chr_value);

	elf_Table * splits = elf_push_new_table(S);

	Scratch scratch = get_scratch();

	while (*s) {
		u64 start_pos = scratch.arena->in_use;
		char *start = arena_push(scratch.arena, 0);

		while (*s != 0 && *s != chr) {
			arena_push_char(scratch.arena, *s ++);
		}

		if (*s == chr) {
			s ++;
		}

		char *end = arena_push_zero(scratch.arena, 1);
		elf_Atom * split = elf_atom_from_data_size(S, start, (u32)(end - start));
		elf_Value value;
		value = value_from_atom(split);

		elf_array_add(S, splits, value);

		scratch.arena->in_use = start_pos;
	}

	end_scratch(scratch);

	return 1;
}



bool is_lowercase_chr(char x) {
	return x >= 'a' && x <= 'z';
}


bool is_uppercase_chr(char x) {
	return x >= 'A' && x <= 'Z';
}

char chr_to_lowercase(char x) {
	if (is_uppercase_chr(x)) {
	 	return x - 'A' + 'a';
	}
	return x;
}


char chr_to_uppercase(char x) {
	if (is_lowercase_chr(x)) {
	 	return x - 'a' + 'A';
	}
	return x;
}

ELF_FUNCTION(l_atom_lowercase) {
	elf_Value value = load_value(S, 0);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *str = value_as_atom(value);
	char *temp = malloc(str->size + 1);
	for (int i = 0; i < str->size; ++ i) {
		temp[i] = chr_to_lowercase(str->data[i]);
	}
	push_value(S, value_from_atom(elf_atom_from_data_size(S, temp, str->size)));
	free(temp);
	return 1;
}



ELF_FUNCTION(l_atom_uppercase) {
	elf_Value value = load_value(S, 0);
	check_value_type(S, value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *str = value_as_atom(value);
	char *temp = malloc(str->size + 1);
	for (int i = 0; i < str->size; ++ i) {
		temp[i] = chr_to_uppercase(str->data[i]);
	}
	push_value(S, value_from_atom(elf_atom_from_data_size(S, temp, str->size)));
	free(temp);
	return 1;
}



static const elf_Binding l_atom[] = {
	{ "length"          , l_atom_size           },
	{ "size"            , l_atom_size           },
	{ "match"           , l_atom_match          },
	{ "uppercase"       , l_atom_uppercase      },
	{ "lowercase"       , l_atom_lowercase      },
	{ "join"            , l_atom_join           },
	{ "get_hash"        , l_atom_get_hash       },
	{ "split_by_lines"  , l_atom_split_by_lines },
	{ "split_by_char"   , l_atom_split_by_char  },
	{ "idx"             , l_atom_idx            },
	{ "find"            , l_atom_find           },
};

static elf_Table *elf_lib_atom(elf_State *state)
{
	return new_binding_table(state, l_atom, ARRAY_COUNT(l_atom));
}
