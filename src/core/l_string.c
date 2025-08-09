//
// See Copyright Notice In elf.h
//


// todo: proper matcher
#include "string_matcher.c"

ELF_FUNCTION(l_str_length) {
	elf_pushint(S, f_checkstr(S, 0)->length);
	return 1;
}

ELF_FUNCTION(l_str_get_hash) {
	elf_String *str = f_checkstr(S, -1);
	elf_pushint(S, str->hash);
	return 1;
}

ELF_FUNCTION(l_str_get_index) {
	char *text = f_checktext(S, -1);
	int index = f_checkint(S, 0);
	elf_pushint(S, text[index]);
	return 1;
}

ELF_FUNCTION(l_str_slice) {
	elf_String *str = f_checkstr(S, -1);
	// todo: out of bounds check
	int from = f_checkint(S, 0);
	int to = f_checkint(S, 1);
	elf_pushstrl(S, str->text + from, to);
	return 1;
}



// todo: this should be like an elf thing, elf_formatting.c
static int value_bprintf(String_Builder *sb, elf_Value v, bool flags);

ELF_FUNCTION(l_str_join) {
	String_Builder sb = {};
	for (int i = -1; i < (nargs - 1); ++ i) {
		value_bprintf(&sb, loadvalue(S, i), 0);
	}
	elf_pushstrl(S, sb.buf, sb.min);
	free(sb.buf);
	return 1;
}


// todo: what if multiple inputs!
// @doc whether the string matches the passed in pattern
ELF_FUNCTION(l_str_match) {
	char *s = f_checktext(S, -1);
	char *p = f_checktext(S,  0);
	char *match = string_match(s, p);
	elf_pushint(S, match != 0);
	return 1;
}

// todo: doesn't actually work
// @doc finds all the matches and returns a list of all the strings
ELF_FUNCTION(l_str_find) {
	char *s = f_checktext(S, -1);
	char *p = f_checktext(S,  0);

	// return a list
	elf_pushtab(S);

	String_Builder sb = {};

	char *cur = s;
	while (*cur) {

		char *tail = string_match(cur, p);

		if (tail) {
			while (cur < tail) {
				bwritechar(&sb, *cur ++);
			}

			elf_pushstrl(S, sb.buf, sb.min);
			elf_arrayadd(S);

			sb.min = 0;

		} else cur ++;
	}

	free(sb.buf);

	return 1;
}

// @doc returns an list of lines from this string
ELF_FUNCTION(l_str_split_by_lines) {
	char *s = f_checktext(S, -1);

	elf_pushtab(S);

	String_Builder sb = {};

	char *cur = s;
	while (*cur) {

		while (*cur != 0 && *cur != '\n' && *cur != '\r') {
			bwritechar(&sb, *cur ++);
		}
		if (*cur == '\n' || *cur == '\r') {
			// skip additional char if \r\n, windows style line ending?
			cur += 1 + (cur[0] == '\r' && cur[1] == '\n');
		}

		elf_pushstrl(S, sb.buf, sb.min);
		elf_arrayadd(S);

		sb.min = 0;
	}

	free(sb.buf);

	return 1;
}


ELF_FUNCTION(l_str_split_by_char) {

	char *s = f_checktext(S, -1);
	int chr = f_checkint(S, 0);

	elf_pushtab(S);

	String_Builder sb = {};

	char *cur = s;
	while (*cur) {

		while (*cur != 0 && *cur != chr) {
			bwritechar(&sb, *cur ++);
		}

		if (*cur == chr) {
			cur ++;
		}

		elf_pushstrl(S, sb.buf, sb.min);
		elf_arrayadd(S);

		sb.min = 0;
	}

	free(sb.buf);

	return 1;
}


ELF_FUNCTION(l_str_lowercase) {
	elf_String *str = f_checkstr(S, -1);
	char *temp = malloc(str->length + 1);
	for (int i = 0; i < str->length; ++ i) {
		temp[i] = chr_to_lowercase(str->text[i]);
	}
	elf_pushstrl(S, temp, str->length);
	free(temp);
	return 1;
}


ELF_FUNCTION(l_str_uppercase) {
	elf_String *str = f_checkstr(S, -1);
	char *temp = malloc(str->length + 1);
	for (int i = 0; i < str->length; ++ i) {
		temp[i] = chr_to_uppercase(str->text[i]);
	}
	elf_pushstrl(S, temp, str->length);
	free(temp);
	return 1;
}


elf_Binding string_metafuncs[] = {
	{ "length"          , l_str_length         },
	{ "match"           , l_str_match          },
	{ "uppercase"       , l_str_uppercase      },
	{ "lowercase"       , l_str_lowercase      },
	{ "join"            , l_str_join           },
	{ "get_hash"        , l_str_get_hash       },
	{ "split_by_lines"  , l_str_split_by_lines },
	{ "split_by_char"   , l_str_split_by_char  },
	{ "idx"             , l_str_get_index      },
	{ "find"            , l_str_find           },
	{ ELF_OVERLOAD_ADD  , l_str_join           },
};