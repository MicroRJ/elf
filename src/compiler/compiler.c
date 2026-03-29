//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>

#include "elf.h"

#include "subsystem.h"

#include "internal_types.h"
#include "internal_helpers.h"

#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parse.h"
#include "generate.h"


#include "logging.c"
#include "c_lexer.c"
#include "arena.c"
#include "ast.c"
#include "parse.c"
#include "emit.c"
#include "generate_old.c"
#include "generate.c"


#include "internal_shorternames.h"

// todo: find a better name for this!
int elf_load_json(elf_State *S, const char *name, const char *contents)
{
#if 0
	elf_Arena arena = elf_create_arena(0);
	Parser *parser = elf_create_parser(S, &arena, name, contents);
	int result = parse_json_object(parser);
	elf_destroy_parser(parser);
	ELF_DestroyArena(&arena);
	return result;
#endif
	return 0;
}

// todo: this is meant to be super light-weight, but it is not!
// todo: so why is this using the stack stuff... why couldn't it
// return a value?
int elf_pushconstexpr(elf_State *S, const char *name, const char *text) {
	if (text) {
		elf_Arena arena = elf_create_arena(0);
		Parser *parser = elf_create_parser(S, &arena, name, text);
		int result = parse_constexpr(parser);
		elf_destroy_parser(parser);
		ELF_DestroyArena(&arena);
		return result;
	}
	else {
		pushnil(S);
		return false;
	}
}


typedef struct
{
	char *data;
	u32   size;
}
SourceFile;

static SourceFile elf_read_source_file(elf_Arena *arena, const char *name)
{
	SourceFile source_file = {};
	Sys file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);
	if (file) {
		u32 size = elf_platform_get_file_size(file);
		char *data = elf_arena_push(arena, size + 16);
		zero_memory(data + size, 16);
		elf_platform_read_file(file, data, size);
		elf_platform_close_file(file);
		source_file.data = data;
		source_file.size = size;
	}
	return source_file;
}

int elf_makefile(elf_State *state, char const *name)
{
	ASSERT(name);

	elf_Arena *scratch_arena = state->scratch_arena;
	elf_Arena *arena = state->arena;
	SourceFile file = elf_read_source_file(arena, name);
	Parser *parser = elf_create_parser(state, scratch_arena, name, file.data);
	AstRef ast_file = elf_parse_file(parser);
	Printer pr = {};
	print_ast(&pr, ast_file);
	BytecodeGen *gen = elf_create_bytecode_generator(state, scratch_arena);
	BytecodeFunction bytecode_function = elf_generate_ast_file(gen, ast_file);


#if 0
	Sys file = elf_platform_access_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

	if (ELF_HISINVALID(file)) {
		elf_lerror("'%s': failed to load file, cannot make", name);
		return -1;
	}

	// todo: add size to allocated memory in GC!
	unsigned int size = elf_platform_get_file_size(file);

	//
	// todo: alloc aligned, ensure text starts aligned too!
	//
	BytecodeFile *protofile = calloc(1, sizeof(*protofile) + size + 1);

	copy_text(protofile->name, sizeof(protofile->name), name);

	elf_platform_read_file(file, protofile->text, size);
	elf_platform_close_file(file);

	// todo: we're using calloc already!
	// null terminate
	protofile->text[size] = 0;

	heap_array_add(S->files, protofile);

	elf_Arena arena = elf_create_arena(0);
	Parser *parser = elf_create_parser(S, &arena, name, protofile->text);
	AstRef parse_file = elf_parse_file(parser);

	Printer pr = {};
	print_ast(&pr, parse_file);


	// create prototypes for every function
	int nfuncs = heap_array_length(parser->functions);

	int protoindex = heap_array_grow(S->protos, nfuncs);
	int mainproto = protoindex;

	BytecodeFunction *protos = & S->protos[protoindex];
	FOR_ARRAY(i, parser->functions) {
		parser->functions[i]->ast_function.proto = protoindex ++;
	}

	protofile->bytepos = S->bytecur;
	//	FOR_ARRAY(i, parser->functions) {
	//		protos[i] = k_do_proto(parser, parser->functions[i]);
	//	}
	protofile->byteend = S->bytecur;


	elf_destroy_parser(parser);
	ELF_DestroyArena(&arena);
	return mainproto;
#endif

	return -1;
}
