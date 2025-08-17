//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>

#include "elf.h"

#include "subsystem.h"

#include "internal_types.h"
#include "internal_api.h"
#include "internal_helpers.h"

#include "elf_compiler.h"
#include "c_token.h"
#include "c_tree.h"
#include "c_parse.h"


#include "c_lexer.c"
#include "c_tree.c"
#include "c_parser.c"
#include "c_make.c"
#include "logging.c"


#include "internal_shorternames.h"

// todo: find a better name for this!
int elf_load_json(elf_State *S, const char *name, const char *contents) {
	elf_Parser *parser = elf_new_parser(S, name, contents);
	int result = parse_json_object(parser);
	elf_end_parser(parser);
	return result;
}


// todo: this is meant to be super light-weight, but it is not!
// also have a version that takes multiple strings, and outputs multiple
// expressions...
int elf_load_const_expr_from_text(elf_State *S, const char *name, const char *text) {
	elf_Parser *parser = elf_new_parser(S, name, text);
	int result = parse_constexpr(parser);
	elf_end_parser(parser);
	return result;
}



int elf_makefile(elf_State *S, char const *name) {
	ASSERT(name);

	Handle hfile = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

	if (ELF_HISINVALID(hfile)) {
		elf_lerror("'%s': failed to load file, cannot make", name);
		return -1;
	}

	// todo: add size to allocated memory in GC!
	unsigned int size = sys_size_file(hfile);

	Proto_File *prof = calloc(1, sizeof(*prof) + size + 1);

	copy_text(prof->name, sizeof(prof->name), name);

	sys_read_file(hfile, prof->text, size);
	sys_close_file(hfile);

	// null terminate
	prof->text[size] = 0;

	darr_add(S->files, prof);

	elf_Parser *parser = elf_new_parser(S, name, prof->text);
	parse_file(parser);


	// create prototypes for every function
	int nfuncs = darr_l(parser->functions);

	int protoindex = darr_grow(S->protos, nfuncs);
	int mainproto = protoindex;

	Proto *protos = & S->protos[protoindex];
	FOR_ARRAY(i, parser->functions) {
		parser->functions[i]->tree_funexpr.proto = protoindex ++;
	}

	prof->bytepos = S->bytecur;
	FOR_ARRAY(i, parser->functions) {
		protos[i] = make_proto(parser, parser->functions[i]);
	}
	prof->byteend = S->bytecur;


	elf_end_parser(parser);
	return mainproto;
}
