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
#include "core.h"
#include "helpers.h"
#include "atom.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "ir.h"
#include "bytecode_gen.h"
#include "lower.h"

#include "logging.c"
#include "atom.c"
#include "lexer.c"
#include "ast.c"
#include "parse.c"
#include "parse_json.c"

#include "ir.c"
#include "lower.c"
#include "bytecode_gen.c"

Compiler *compiler_create(elf_State *state, elf_Arena *arena, const char *name, elf_StrSlice source)
{
	Compiler *compiler = elf_arena_push_zero(arena, sizeof(*compiler));
	compiler->state = state;
	compiler->arena = arena;
	compiler->source_name = name;
	compiler->source = source;
	return compiler;
}

void compiler_reportv(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase,
	SourceSite site, const char *format, va_list args)
{
	ASSERT(compiler);
	if (compiler->diagnostic_count == compiler->diagnostic_capacity) {
		u32 capacity = compiler->diagnostic_capacity ? compiler->diagnostic_capacity * 2 : 16;
		elf_Diagnostic *diagnostics = realloc(compiler->diagnostics, sizeof(*diagnostics) * capacity);
		ASSERT(diagnostics);
		zero_memory(diagnostics + compiler->diagnostic_capacity,
			sizeof(*diagnostics) * (capacity - compiler->diagnostic_capacity));
		compiler->diagnostics = diagnostics;
		compiler->diagnostic_capacity = capacity;
	}

	va_list count_args;
	va_copy(count_args, args);
	int message_size = vsnprintf(0, 0, format, count_args);
	va_end(count_args);
	ASSERT(message_size >= 0);
	char *message = malloc((u64)message_size + 1);
	ASSERT(message);
	vsnprintf(message, (u64)message_size + 1, format, args);

	const char *source_name = compiler->source_name ? compiler->source_name : "<unknown>";
	elf_Diagnostic *diagnostic = compiler->diagnostics + compiler->diagnostic_count++;
	*diagnostic = (elf_Diagnostic) {
		.severity = severity,
		.phase = phase,
		.source_name = {(char *)source_name, strlen(source_name)},
		.message = {message, strlen(message)},
		.line = site.line_index,
		.column = source_slice_column(site),
	};

	if (severity == ELF_DIAGNOSTIC_ERROR) compiler->error_count += 1;
	else if (severity == ELF_DIAGNOSTIC_WARNING) compiler->warning_count += 1;

	LogLevel level = severity == ELF_DIAGNOSTIC_ERROR ? LOG_LEVEL_ERROR :
		severity == ELF_DIAGNOSTIC_WARNING ? LOG_LEVEL_WARNING : LOG_LEVEL_INFO;
	const char *severity_name = severity == ELF_DIAGNOSTIC_ERROR ? "error" :
		severity == ELF_DIAGNOSTIC_WARNING ? "warning" : "note";
	if (site.line_index) {
		log_linef(level, "%s [%u:%llu] %s: %s", source_name, site.line_index,
			source_slice_column(site), severity_name, message);
	}
	else {
		log_linef(level, "%s [?] %s: %s", source_name, severity_name, message);
	}
	if (source_slice_is_valid(site)) print_source_slice_marker(site, compiler->source);
}

void compiler_report(Compiler *compiler, elf_DiagnosticSeverity severity, elf_DiagnosticPhase phase,
	SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	compiler_reportv(compiler, severity, phase, site, format, args);
	va_end(args);
}

static char *copy_compile_report_text(elf_StrSlice text)
{
	char *copy = malloc(text.size + 1);
	ASSERT(copy);
	copy_memory(copy, text.data, text.size);
	copy[text.size] = 0;
	return copy;
}

void compiler_finish_report(Compiler *compiler, elf_CompileReport *report)
{
	if (report) {
		*report = (elf_CompileReport) {};
	}
	if (report && compiler->diagnostic_count) {
		elf_Diagnostic *diagnostics = calloc(compiler->diagnostic_count, sizeof(*diagnostics));
		ASSERT(diagnostics);
		for (u32 i = 0; i < compiler->diagnostic_count; ++i) {
			diagnostics[i] = compiler->diagnostics[i];
			diagnostics[i].source_name.data = copy_compile_report_text(diagnostics[i].source_name);
			diagnostics[i].message.data = copy_compile_report_text(diagnostics[i].message);
		}
		report->diagnostics = diagnostics;
	}
	if (report) {
		report->diagnostic_count = compiler->diagnostic_count;
		report->error_count = compiler->error_count;
		report->warning_count = compiler->warning_count;
	}
	for (u32 i = 0; i < compiler->diagnostic_count; ++i) {
		free(compiler->diagnostics[i].message.data);
	}
	free(compiler->diagnostics);
	compiler->diagnostics = 0;
	compiler->diagnostic_count = 0;
	compiler->diagnostic_capacity = 0;
}

static void restore_source_stack(elf_State *state, elf_Value *checkpoint)
{
	ASSERT(checkpoint >= state->frame->framebase && checkpoint <= state->stack_ptr);
	value_zero_many(checkpoint, state->stack_ptr - checkpoint);
	state->stack_ptr = checkpoint;
}

int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report)
{
	elf_Value *stack_checkpoint = state->stack_ptr;
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena, name, source);
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(compiler);
	int result = parse_json_value(parser);
	if (!result) restore_source_stack(state, stack_checkpoint);
	else ASSERT(state->stack_ptr == stack_checkpoint + 1);

	compiler_finish_report(compiler, report);
	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return result;
}

int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source, elf_CompileReport *report)
{
	elf_Value *stack_checkpoint = state->stack_ptr;
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena, name, source);
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	Parser *parser = elf_create_parser(compiler);
	int result = parse_constexpr(parser);
	if (!result) restore_source_stack(state, stack_checkpoint);
	else ASSERT(state->stack_ptr == stack_checkpoint + 1);

	compiler_finish_report(compiler, report);
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

BcFunctionRef elf_compile_source(elf_State *state, char const *name, elf_StrSlice source, elf_CompileReport *report)
{
	ASSERT(name);
	ASSERT(source.data);

	u64 arena_checkpoint = state->arena.in_use;
	BcFunctionRef file_entry = {};
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena, name, source);
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	elf_arena_align(&state->arena, 8);
	elf_Module *module = elf_arena_push_zero(&state->arena, sizeof(*module));
	elf_StrSlice owned_source = persist_compiled_source(state, source);
	elf_String *source_name = elf_string_from_data(state, name);
	module->source_name = source_name;
	module->source_data = owned_source.data;
	module->source_size = (u32)owned_source.size;
	compiler->source = owned_source;

	Parser *parser = elf_create_parser(compiler);
	Ast ast_file;
	PROF_BLOCK("compiler.parse")
	{
		ast_file = elf_parse_file(parser);
	}
	if (parser_has_failed(parser) || ast_is_error(ast_file))
	{
		if (compiler->error_count == 0) {
			compiler_report(compiler, ELF_DIAGNOSTIC_ERROR, ELF_DIAGNOSTIC_PHASE_PARSER,
				(SourceSite) {}, "failed to parse source");
		}
		state->arena.in_use = arena_checkpoint;
		goto done;
	}

	LowerContext *ctx = elf_create_lower_context(compiler);
	IrModule ir_module;
	PROF_BLOCK("compiler.lower")
	{
		ir_module = elf_lower_ast_file(ctx, ast_file);
	}
	if (compiler->error_count) {
		state->arena.in_use = arena_checkpoint;
		goto done;
	}

	file_entry = generate_module(compiler, module, ir_module);
	if (!bc_function_ref_is_valid(file_entry) || compiler->error_count) {
		file_entry = (BcFunctionRef) {};
		state->arena.in_use = arena_checkpoint;
		goto done;
	}
	module->next = state->modules;
	state->modules = module;

done:
	compiler_finish_report(compiler, report);
	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return file_entry;
}
