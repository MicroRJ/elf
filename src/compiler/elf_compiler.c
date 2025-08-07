//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>

#include "elf.h"

#include "elf_internal.h"

#include "subsystem.h"
#include "r_auxilary.h"

#include "bytecode_metadata.h"

#include "elf_compiler.h"
#include "c_token.h"
#include "c_tree.h"
#include "c_parse.h"


#include "c_lexer.c"
#include "c_tree.c"
#include "elf_parser.c"
#include "c_generate.c"


// todo: find a better name for this!
int elf_load_json(elf_State *S, const char *name, const char *contents) {
	elf_Parser *parser = elf_new_parser(S, name, contents);
	int result = parse_json_object(parser);
	free(parser);
	return result;
}

int elf_load_const_expr(elf_State *S, const char *name, const char *contents) {
	elf_Parser *parser = elf_new_parser(S, name, contents);
	int result = parse_constexpr(parser);
	free(parser);
	return result;
}

// todo: remove the as_expr thing?
elf_Proto elf_compile(elf_State *S, elf_String *name, elf_String *contents, bool as_expr) {
	ASSERT(contents);
	ASSERT(name);

	// todo: uninit the parser!
	elf_Parser *parser = elf_new_parser(S, name->text, contents->text);

	// Todo: this pattern is common, just create one "begin" / "end"
	// set of functions

	treeID func = new_tree(parser, parser->tok.line, TREE_FUNCTION, NT_FUN);
	parser->enc = func;

	add_this_param(parser, parser->tok.line);

	arradd(parser->functions, func);

	if (as_expr) {

		treeID v = parse_expr(parser, 0);
		v = tree_ret(parser, parser->tok.line, v);
		block_add(parser, v);

	} else{

		// todo: parse block function?
		while (parse_stat(parser));
		FOR_ARRAY(i, parser->block.defers) {
			arradd(parser->block.body, parser->block.defers[i]);
		}

	}
	func->tree_funexpr.body = tree_block(parser, parser->tok.line, parser->block.body, 0, 0);

	// create prototypes for every function
	int nfunctions = arrlen(parser->functions);
	int index = ARRAY_GROW(S->protos, nfunctions);
	elf_Proto *protos = & S->protos[index];

	// assign prototypes to each function
	int start = S->nbytes;
	FOR_ARRAY(i, parser->functions) {
		parser->functions[i]->tree_funexpr.proto = index ++;
	}

	// then compile each
	FOR_ARRAY(i, parser->functions) {
		protos[i] = genfunction(parser, parser->functions[i]);
		// elf_debug_log("PROTO: [%i, %i) (%i)"
		// , 	protos[i].bytes
		// , 	protos[i].bytes+protos[i].nbytes
		// ,	protos[i].nbytes);
	}
	int end = S->nbytes;

	{
		//
		// todo: because we show source code when
		// the program crashes at runtime the
		// contents string is kept alive, can we
		// do better ?
		//
		elf_File file = {
			.pos = start,
			.end = end,
			.proto = protos[0],
			.contents = contents,
			.name = name,
		};
		arradd(S->files, file);
	}

	// todo: how do we track this, should each proto
	// point to the file they are from?...
	elf_raw_array_add(S->globals, VALUE_STRING(contents));
	elf_raw_array_add(S->globals, VALUE_STRING(name));

	free(parser);

	return protos[0];
}
