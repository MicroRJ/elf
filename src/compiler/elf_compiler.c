//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>

#include "elf.h"
#include "r_core.h"
#include "subsystem.h"
#include "r_auxilary.h"

#include "bytecode_metadata.h"

#include "elf_compiler.h"
#include "c_token.h"
#include "c_tree.h"
#include "c_parse.h"


#include "c_lexer.c"
#include "c_tree.c"
#include "c_parse.c"
#include "c_generate.c"


elf_Table *elf_parse_json(elf_State *S, char *name, char *contents) {
	elf_Parser parser = {};
	c_parser_init(S, &parser, name, contents);

	elf_Table *table = c_parse_json(&parser);
	return table;
}


int elf_parse_const(elf_State *S, char *name, char *contents) {
	elf_Parser parser = {};
	c_parser_init(S, &parser, name, contents);

	return c_parse_const(&parser);
}

// todo: remove the as_expr thing?
elf_Proto elf_compile(elf_State *S, elf_String *name, elf_String *contents, bool as_expr) {
	ASSERT(contents);
	ASSERT(name);
	// todo: uninit the parser!
	elf_Parser parser_ = {};
	elf_Parser *parser = &parser_;
	c_parser_init(S, parser, name->text, contents->text);

	// Todo: this pattern is common, just create one "begin" / "end"
	// set of functions

	treeID func = new_tree(parser, parser->tok.line, TREE_FUNCTION, NT_FUN);
	parser->enc = func;

	add_this_param(parser, parser->tok.line);

	ARRAY_ADD(parser->functions, func);

	if (as_expr) {

		treeID v = parse_expr(parser, 0);
		v = tree_ret(parser, parser->tok.line, v);
		block_add(parser, v);

	} else{

		// todo: parse block function?
		while (parse_stat(parser));
		FOR_ARRAY(i, parser->block.defers) {
			ARRAY_ADD(parser->block.body, parser->block.defers[i]);
		}

	}
	func->expr_fun.body = tree_block(parser, parser->tok.line, parser->block.body);

	// create prototypes for every function
	int nfunctions = ARRAY_LENGTH(parser->functions);
	int index = ARRAY_GROW(S->protos, nfunctions);
	elf_Proto *protos = & S->protos[index];

	// assign prototypes to each function
	int start = S->nbytes;
	FOR_ARRAY(i, parser->functions) {
		parser->functions[i]->expr_fun.proto = index ++;
	}

	// then compile each
	FOR_ARRAY(i, parser->functions) {
		protos[i] = elf_compile_function(parser, parser->functions[i]);
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
		ARRAY_ADD(S->files, file);
	}

	// todo: how do we track this, should each proto
	// point to the file they are from?...
	elf_array_add_raw(S->globals, VALUE_STRING(contents));
	elf_array_add_raw(S->globals, VALUE_STRING(name));

	return protos[0];
}
