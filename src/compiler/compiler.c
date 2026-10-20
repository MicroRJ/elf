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

#include <dy.h>

#include "elf.h"
#include "elf_core.h"
#include "base.h"
#include "atom.h"
#include "compiler.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "ir.h"
#include "bc_emitter.h"
#include "ir_emitter.h"

#include "logging.c"
#include "source_diagnostics.c"
#include "atom.c"
#include "checker.h"


static void compiler_init_atoms(Compiler *compiler)
{
#define INTERN_KEYWORD_ATOM(NAME, TEXT) atom_from_data_id(compiler->atoms, TEXT, XFUSE(TOK_, NAME));
	KEYWORD_DEFINITIONS(INTERN_KEYWORD_ATOM)
#undef INTERN_KEYWORD_ATOM

#define INTERN_MACRO_ATOM(NAME, TEXT) atom_from_data_id(compiler->atoms, "#" TEXT, XFUSE(TOK_, NAME));
	MACRO_DEFINITIONS(INTERN_MACRO_ATOM)
#undef INTERN_MACRO_ATOM

#define INTERN_TYPE_NAME_ATOM(NAME, TEXT, RULE) atom_from_data_id(compiler->atoms, TEXT, XFUSE(ATOM_TYPE_NAME_, NAME));
	TYPE_NAME_DEFINITIONS(INTERN_TYPE_NAME_ATOM)
#undef INTERN_TYPE_NAME_ATOM
}


Compiler *compiler_create(Arena *arena)
{
	Compiler *compiler = 0;
	if (arena)
	{
		compiler = arena_push_zero(arena, sizeof(*compiler), _Alignof(Compiler));
	}
	if (compiler)
	{
		compiler->arena = arena;
		compiler->atoms = arena_push_zero(arena, sizeof(*compiler->atoms), _Alignof(Atom_Table));
		if (compiler->atoms)
		{
			atom_table_init(compiler->atoms, arena);
			if (compiler->atoms->buckets)
			{
				compiler_init_atoms(compiler);
			}
			else
			{
				compiler = 0;
			}
		}
		else
		{
			compiler = 0;
		}
	}
	return compiler;
}

static elf_StrSlice compiler_format_message(Arena *arena, const char *format, va_list args)
{
	va_list count_args;
	va_copy(count_args, args);
	int message_size = vsnprintf(0, 0, format, count_args);
	va_end(count_args);
	if (message_size < 0) {
		return (elf_StrSlice) {0};
	}
	char *message = arena_push(arena, (u64)message_size + 1, 1);
	if (!message) {
		return (elf_StrSlice) {0};
	}
	vsnprintf(message, (u64)message_size + 1, format, args);
	return (elf_StrSlice) {message, (u64)message_size};
}

b32 compiler_reportv(Com_Source *source, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(source);
	Compiler *compiler = source->compiler;
	ASSERT(compiler);
	ASSERT(code != ERROR_NONE);
	elf_StrSlice message = compiler_format_message(compiler->arena, format, args);
	if (!message.data) {
		return false;
	}

	Com_Message *compiler_message = arena_push_zero(compiler->arena, sizeof(*compiler_message), _Alignof(Com_Message));
	if (!compiler_message) {
		return false;
	}
	compiler_message->code = code;
	compiler_message->severity = severity;
	compiler_message->phase = phase;
	compiler_message->site = site;
	compiler_message->text = message;
	if (source->report.last)
	{
		source->report.last->next = compiler_message;
	}
	else
	{
		source->report.first = compiler_message;
	}
	source->report.last = compiler_message;
	source->report.message_count += 1;

	if (severity == ERROR_ERROR || severity == ERROR_FATAL)
	{
		source->report.error_count += 1;
		compiler->error_count += 1;
	}
	else if (severity == ERROR_WARNING)
	{
		source->report.warning_count += 1;
		compiler->warning_count += 1;
	}

	return true;
}

void compiler_log_report(Compiler *compiler)
{
	ASSERT(compiler);
	for (Com_Source *source = compiler->first_source; source; source = source->next)
	{
		for (Com_Message *message = source->report.first; message; message = message->next)
		{
			LogLevel level = LOG_LEVEL_INFO;
			const char *severity_name = "note";
			if (message->severity == ERROR_FATAL)
			{
				level = LOG_LEVEL_FATAL;
				severity_name = "fatal";
			}
			else if (message->severity == ERROR_ERROR)
			{
				level = LOG_LEVEL_ERROR;
				severity_name = "error";
			}
			else if (message->severity == ERROR_WARNING)
			{
				level = LOG_LEVEL_WARNING;
				severity_name = "warning";
			}
			SourceSite site = message->site;
			const char *source_name = atom_data(source->name);
			if (site.line_index)
			{
				log_format(LOG_LEVEL_INFO, "%s [%u:%llu] ", source_name, site.line_index, source_slice_column(site));
				log_format(level, "%s", severity_name);
				log_linef(LOG_LEVEL_INFO, ": %s", message->text.data);
			}
			else
			{
				log_format(LOG_LEVEL_INFO, "%s [?] ", source_name);
				log_format(level, "%s", severity_name);
				log_linef(LOG_LEVEL_INFO, ": %s", message->text.data);
			}
			if (source_slice_is_valid(site))
			{
				print_source_slice_marker(site, source->source);
			}
		}
	}
}

b32 compiler_report(Com_Source *source, ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 result = compiler_reportv(source, code, severity, phase, site, format, args);
	va_end(args);
	return result;
}

static Com_Source *compiler_alloc_source(Compiler *compiler, Atom *name, Atom *path, elf_Source *runtime_source, dy_String data)
{
	Com_Source *source = arena_push_zero(compiler->arena, sizeof(*source), _Alignof(Com_Source));
	if (!source) {
		return 0;
	}
	source->compiler = compiler;
	source->name = name;
	source->path = path;
	source->runtime_source = runtime_source;
	if (runtime_source)
	{
		ASSERT(!data.data && !data.size);
		ASSERT(runtime_source->text.size <= UINT32_MAX);
		data = runtime_source->text;
	}
	source->source = data;
	if (compiler->last_source) {
		compiler->last_source->next = source;
	} else {
		compiler->first_source = source;
	}
	compiler->last_source = source;
	return source;
}

// NOTE(RJ): assumes the path is canonical
static Com_Source *compiler_load_source(Compiler *compiler, Com_Source *requesting_source, SourceSite site, Atom *path_atom)
{
	ASSERT(compiler->target);
	ASSERT(requesting_source);
	for (Com_Source *source = compiler->first_source; source; source = source->next)
	{
		if (source->path == path_atom) {
			return source;
		}
	}

	const char *error = 0;

	dy_String path = {};
	path.data = (char *) atom_data(path_atom);
	path.size = atom_size(path_atom);

	u64 arena_checkpoint = arena_mark(compiler->arena);
	dy_String contents = {0};

	dy_File_Info info = {0};
	if (!dy_file_info(path, &info).error && !info.is_directory)
	{
		if (info.size <= UINT32_MAX)
		{
			dy_File file = {0};
			if (!dy_file_open(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
			{
				contents.data = arena_push(compiler->arena, info.size + 1, 16);
				if (contents.data)
				{
					dy_u64 read = 0;
					if (!dy_file_read(file, contents.data, info.size, &read).error && read == info.size)
					{
						contents.data[info.size] = 0;
						contents.size = info.size;
					}
					else {
						error = "could not read source file";
					}
				}
				else {
					error = "out of memory while loading source file";
				}

				dy_file_close(file);
			}
			else {
				error = "could not open source file";
			}
		}
		else {
			error = "source file exceeds the 32-bit size limit";
		}
	}
	else {
		error = "could not find source file";
	}

	elf_Source *runtime_source = 0;
	if (!error)
	{
		runtime_source = elf_source_create(compiler->target, path, contents);
	}
	arena_restore(compiler->arena, arena_checkpoint);

	Com_Source *source = 0;
	if (error)
	{
		compiler_report(requesting_source, ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "%s '%s'", error, atom_data(path_atom));
	}
	else
	{
		source = compiler_alloc_source(compiler, path_atom, path_atom, runtime_source, (dy_String) {});
		if (!source) {
			compiler_report(requesting_source, ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "out of memory while registering source file '%s'", atom_data(path_atom));
		}
	}
	return source;
}

static Ast parse_source_recursively(Compiler *compiler, Parser *parser, Com_Source *source)
{
	ASSERT(source);
	if (source->state != COM_SOURCE_UNPARSED) {
		return source->ast;
	}

	Ast ast = parser_parse_source(parser, source);
	if (ast_is_error(ast)) {
		return ast;
	}
	ASSERT(ast->kind == AST_FUNCTION);
	ASSERT(!ast->function.parent);
	source->ast = ast;
	source->state = COM_SOURCE_PARSED;

	for (u32 i = 0; i < source->import_count; ++i)
	{
		Ast import_ast = source->imports[i];
		ASSERT(import_ast);
		ASSERT(import_ast->kind == AST_IMPORT);
		ASSERT(import_ast->import.path);

		Atom *import_path_requested = import_ast->import.path;
		Com_Source *imported_source = compiler_load_source(compiler, source, import_ast->site, import_path_requested);
		if (!imported_source) {
			return ERROR_AST;
		}
		import_ast->import.target = imported_source;
		Ast imported_ast = parse_source_recursively(compiler, parser, imported_source);
		if (ast_is_error(imported_ast)) {
			return imported_ast;
		}
	}

	return ast;
}

elf_Prototype *compiler_compile(elf_State *target, Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	elf_Prototype *entry = 0;
	if (target && arena && name.data && name.size <= UINT32_MAX && source.data && source.size <= UINT32_MAX)
	{
		Compiler *compiler = compiler_create(arena);
		if (compiler)
		{
			compiler->target = target;
			Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
			dy_String runtime_name = {(char *)name.data, name.size};
			dy_String runtime_text = {(char *)source.data, source.size};
			elf_Source *runtime_source = elf_source_create(target, runtime_name, runtime_text);
			if (source_name && runtime_source)
			{
				Com_Source *compiler_source = compiler_alloc_source(compiler, source_name, source_name, runtime_source, (dy_String) {});
				if (compiler_source)
				{
					Parser parser = {};
					if (parser_init(&parser))
					{
						Ast ast = parse_source_recursively(compiler, &parser, compiler_source);

						parser_destroy(&parser);

						if (!ast_is_error(ast) && !compiler->error_count)
						{
							Com_Status check_status = check_source(compiler_source);
							if (check_status == COM_STATUS_OK)
							{
								IrModule *ir_module = ir_emit_module(compiler_source);
								if (ir_module && !compiler->error_count) {
									entry = bc_emit_module(compiler->target, ir_module);
								}
							}
						}
					}
				}
			}
			compiler_log_report(compiler);
		}
	}
	return entry;
}

Ast compiler_parse_constant_expr(Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	(void)arena;
	(void)name;
	(void)source;
	return ERROR_AST;
}

typedef enum
{
	COMPILER_PARSE_JSON,
	COMPILER_PARSE_JSONL,
}
CompilerParseType;

static Ast compiler_parse_json_source(Arena *arena, elf_StrSlice name, elf_StrSlice source, CompilerParseType type)
{
	Ast ast = ERROR_AST;
	if (arena && name.data && name.size <= UINT32_MAX && source.data && source.size <= UINT32_MAX)
	{
		Compiler *compiler = compiler_create(arena);
		if (compiler)
		{
			Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
			char *source_data = arena_push_zero(arena, source.size + 1, 1);
			if (source_name && source_data)
			{
				copy_memory(source_data, source.data, source.size);
				Com_Source *compiler_source = compiler_alloc_source(compiler, source_name, source_name, 0, (dy_String) {.data = source_data, .size = source.size});
				if (compiler_source)
				{
					Parser parser = {};
					if (parser_init(&parser))
					{
						ast = type == COMPILER_PARSE_JSON ? parser_parse_json(&parser, compiler_source) : parser_parse_jsonl(&parser, compiler_source);
						parser_destroy(&parser);
					}
				}
			}
			if (compiler->error_count) {
				ast = ERROR_AST;
			}
		}
	}
	return ast;
}

Ast compiler_parse_json(Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	return compiler_parse_json_source(arena, name, source, COMPILER_PARSE_JSON);
}

Ast compiler_parse_jsonl(Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	return compiler_parse_json_source(arena, name, source, COMPILER_PARSE_JSONL);
}

