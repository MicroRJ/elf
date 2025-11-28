//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>

#include "elf.h"

#include "subsystem.h"

#include "internal_types.h"
#include "internal_helpers.h"

#include "elf_compiler.h"
#include "c_token.h"
#include "c_tree.h"
#include "c_parse.h"


#include "logging.c"
#include "c_lexer.c"
#include "c_tree.c"
#include "c_parser.c"
#include "c_make.c"


#include "internal_shorternames.h"

// todo: find a better name for this!
int elf_load_json(elf_State *S, const char *name, const char *contents) {
	Parser *parser = elf_new_parser(S, name, contents);
	int result = parse_json_object(parser);
	elf_end_parser(parser);
	return result;
}

// todo: this is meant to be super light-weight, but it is not!
// todo: so why is this using the stack stuff... why couldn't it
// return a value?
int elf_pushconstexpr(elf_State *S, const char *name, const char *text) {
	if (text) {
		Parser *parser = elf_new_parser(S, name, text);
		int result = parse_constexpr(parser);
		elf_end_parser(parser);
		return result;
	}
	else {
		pushnil(S);
		return false;
	}
}

int elf_makefile(elf_State *S, char const *name) {
	ASSERT(name);

	Sys file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

	if (ELF_HISINVALID(file)) {
		elf_lerror("'%s': failed to load file, cannot make", name);
		return -1;
	}

	// todo: add size to allocated memory in GC!
	unsigned int size = sys_size_file(file);

	//
	// todo: alloc aligned, ensure text starts aligned too!
	//
	Proto_File *protofile = calloc(1, sizeof(*protofile) + size + 1);

	copy_text(protofile->name, sizeof(protofile->name), name);

	sys_read_file(file, protofile->text, size);
	sys_close_file(file);

	// todo: we're using calloc already!
	// null terminate
	protofile->text[size] = 0;

	heap_array_add(S->files, protofile);

	Parser *parser = elf_new_parser(S, name, protofile->text);
	parse_file(parser);


	// create prototypes for every function
	int nfuncs = heap_array_length(parser->functions);

	int protoindex = heap_array_grow(S->protos, nfuncs);
	int mainproto = protoindex;

	Proto *protos = & S->protos[protoindex];
	FOR_ARRAY(i, parser->functions) {
		parser->functions[i]->tree_funexpr.proto = protoindex ++;
	}

	protofile->bytepos = S->bytecur;
	FOR_ARRAY(i, parser->functions) {
		protos[i] = make_proto(parser, parser->functions[i]);
	}
	protofile->byteend = S->bytecur;


	elf_end_parser(parser);
	return mainproto;
}
