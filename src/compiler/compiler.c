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
#include "system.h"
#include "core.h"
#include "helpers.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parse.h"
#include "ir.h"
#include "bytecode_gen.h"
#include "lower.h"

#include "logging.c"
#include "lexer.c"
#include "ast.c"
#include "parse.c"
#include "parse_json.c"
#include "bytecode_emit.c"

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
	Scratch scratch = get_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(state, scratch.arena, name, source);
	int result = parse_json_value(parser);

	state->gc_mode = saved_gc_mode;
	end_scratch(scratch);
	return result;
}

int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source)
{
	Scratch scratch = get_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(state, scratch.arena, name, source);
	int result = parse_constexpr(parser);

	state->gc_mode = saved_gc_mode;
	end_scratch(scratch);
	return result;
}

static u32 append_bytecode(elf_State *state, Bytecode *bytecode, u32 count)
{
	ASSERT(state->bytecode_count + count <= state->bytecode_capacity);
	u32 offset = state->bytecode_count;
	copy_memory(state->bytecode + offset, bytecode, sizeof(*bytecode) * count);
	state->bytecode_count += count;
	return offset;
}

static SourceMapEntry *copy_source_map(Arena *arena, SourceMapEntry *entries, u32 count, u32 bytecode_offset)
{
	if (count == 0)
	{
		return 0;
	}

	SourceMapEntry *source_map = arena_push(arena, sizeof(*source_map) * count);
	for (u32 i = 0; i < count; ++ i)
	{
		SourceMapEntry entry = entries[i];
		entry.byte_start += bytecode_offset;
		entry.byte_end   += bytecode_offset;
		source_map[i] = entry;
	}
	return source_map;
}

static BytecodeFunction *reserve_bytecode_functions(elf_State *state, u32 count)
{
	ASSERT(state->bytecode_function_count + count <= state->bytecode_function_capacity);

	u32 index = state->bytecode_function_count;
	state->bytecode_function_count += count;
	return state->bytecode_functions + index;
}

BytecodeFunction elf_compile_source(elf_State *state, char const *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);

	Arena *arena = &state->arena;
	Scratch scratch = get_scratch();
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	elf_Atom *source_name = elf_atom_from_data(state, name);

	Parser *parser = elf_create_parser(state, scratch.arena, name, source);
	AstRef ast_file = elf_parse_file(parser);

	LowerContext *ctx = elf_create_lower_context(state, scratch.arena);
	ctx->source_name = source_name;
	elf_lower_ast_file(ctx, ast_file);

	u32 bytecode_function_base = state->bytecode_function_count;
	BytecodeFunction *bytecode_functions = reserve_bytecode_functions(state, ctx->num_functions);

	BytecodeGen *gen = allocate_bytecode_function_generator(state, scratch.arena, bytecode_function_base);

	for (u32 i = 0; i < ctx->num_functions; ++ i)
	{
		FunctionIR function = ctx->functions[i];
		generate_bytecode_function(gen, function);

		u32 bytecode_offset = append_bytecode(state, gen->bytecode_buffer.bytecode, gen->bytecode_buffer.position);
		SourceMapEntry *source_map = copy_source_map(arena, gen->source_map_buffer.entries, gen->source_map_buffer.count, bytecode_offset);

		BytecodeFunction *bytecode_function = bytecode_functions + i;
		bytecode_function->variadic         = function.variadic;
		bytecode_function->arity            = function.arity;
		bytecode_function->offset           = bytecode_offset;
		bytecode_function->length           = gen->bytecode_buffer.position;
		bytecode_function->captures         = function.capture_count;
		bytecode_function->stack_size       = gen->memory_usage.slot;
		bytecode_function->source_map       = source_map;
		bytecode_function->source_map_count = gen->source_map_buffer.count;
		bytecode_function->source_data      = source.data;
		bytecode_function->source_size      = (u32)source.size;
		bytecode_function->source_name      = source_name;
	}

	BytecodeFunction file_entry = bytecode_functions[0];

	state->gc_mode = saved_gc_mode;
	end_scratch(scratch);
	return file_entry;
}
