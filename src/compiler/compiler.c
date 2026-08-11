//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "elf.h"
#include "base.h"
#include "platform.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "ir.h"
#include "bytecode_gen.h"
#include "lower.h"

#include "logging.c"
#include "lexer.c"
#include "ast.c"
#include "parse.c"
#include "parse_json.c"

#include "ir.c"
#include "lower.c"
#include "bytecode_gen.c"

void elf_init_compiler_atoms(elf_State *state)
{
#define INTERN_KEYWORD_ATOM(NAME, TEXT) elf_atom_from_data_id(state, TEXT, XFUSE(TOK_, NAME));
	KEYWORDDEF(INTERN_KEYWORD_ATOM)
#undef INTERN_KEYWORD_ATOM

#define INTERN_MACRO_ATOM(NAME, TEXT) elf_atom_from_data_id(state, "#" TEXT, XFUSE(TOK_, NAME));
	MACRODEF(INTERN_MACRO_ATOM)
#undef INTERN_MACRO_ATOM
}

int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_Scratch scratch = elf_begin_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(state, scratch.arena, name, source);
	int result = parse_json_value(parser);

	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return result;
}

int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_Scratch scratch = elf_begin_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(state, scratch.arena, name, source);
	int result = parse_constexpr(parser);

	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return result;
}

static elf_StrSlice persist_compiled_source(elf_State *state, elf_StrSlice source)
{
	if (source.size > 0xffffffffu) abort();
	u64 storage_size = (source.size + 16 + 7) & ~(u64)7;
	char *data = elf_arena_push_zero(&state->arena, storage_size);
	copy_memory(data, source.data, source.size);
	return (elf_StrSlice) {data, source.size};
}

BcFunctionRef elf_compile_source(elf_State *state, char const *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);

	elf_Scratch scratch = elf_begin_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	elf_arena_align(&state->arena, 8);
	elf_Module *module = elf_arena_push_zero(&state->arena, sizeof(*module));
	elf_StrSlice owned_source = persist_compiled_source(state, source);
	elf_String *source_name = elf_atom_from_data(state, name);
	module->source_name = source_name;
	module->source_data = owned_source.data;
	module->source_size = (u32)owned_source.size;

	Parser *parser = elf_create_parser(state, scratch.arena, name, owned_source);
	Ast ast_file;
	PROF_BLOCK("compiler.parse")
	{
		ast_file = elf_parse_file(parser);
	}

	LowerContext *ctx = elf_create_lower_context(state, scratch.arena);
	ctx->source_name = source_name;
	IrModule ir_module;
	PROF_BLOCK("compiler.lower")
	{
		ir_module = elf_lower_ast_file(ctx, ast_file);
	}

	BcFunctionRef file_entry = generate_module(state, scratch.arena, module, ir_module);
	module->next = state->modules;
	state->modules = module;

	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return file_entry;
}
