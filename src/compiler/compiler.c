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


Compiler *compiler_create(elf_Arena *arena)
{
	Compiler *compiler = 0;
	if (arena)
	{
		compiler = dy_arena_push_zero(arena, sizeof(*compiler), _Alignof(Compiler));
	}
	if (compiler)
	{
		compiler->arena = arena;
		compiler->atoms = dy_arena_push_zero(arena, sizeof(*compiler->atoms), _Alignof(Atom_Table));
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

static elf_StrSlice compiler_format_message(elf_Arena *arena, const char *format, va_list args)
{
	va_list count_args;
	va_copy(count_args, args);
	int message_size = vsnprintf(0, 0, format, count_args);
	va_end(count_args);
	if (message_size < 0) {
		return (elf_StrSlice) {0};
	}
	char *message = elf_arena_push(arena, (u64)message_size + 1);
	if (!message) {
		return (elf_StrSlice) {0};
	}
	vsnprintf(message, (u64)message_size + 1, format, args);
	return (elf_StrSlice) {message, (u64)message_size};
}

b32 compiler_reportv(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(compiler);
	ASSERT(error != COM_ERROR_NONE);
	elf_StrSlice message = compiler_format_message(compiler->arena, format, args);
	if (!message.data) {
		return false;
	}

	Com_Message *compiler_message = dy_arena_push_zero(compiler->arena, sizeof(*compiler_message), _Alignof(Com_Message));
	if (!compiler_message) {
		return false;
	}
	compiler_message->error = error;
	compiler_message->severity = severity;
	compiler_message->phase = phase;
	compiler_message->site = site;
	compiler_message->text = message;
	if (compiler->report.last)
	{
		compiler->report.last->next = compiler_message;
	}
	else
	{
		compiler->report.first = compiler_message;
	}
	compiler->report.last = compiler_message;
	compiler->report.message_count += 1;

	if (severity == COMPILER_ERROR)
	{
		compiler->report.error_count += 1;
	}
	else if (severity == COMPILER_WARNING)
	{
		compiler->report.warning_count += 1;
	}

	return true;
}

void compiler_log_report(Compiler *compiler)
{
	ASSERT(compiler);
	for (Com_Message *message = compiler->report.first; message; message = message->next)
	{
		LogLevel level = message->severity == COMPILER_ERROR ? LOG_LEVEL_ERROR : message->severity == COMPILER_WARNING ? LOG_LEVEL_WARNING : LOG_LEVEL_INFO;
		const char *severity_name = message->severity == COMPILER_ERROR ? "error" : message->severity == COMPILER_WARNING ? "warning" : "note";
		SourceSite site = message->site;
		Com_Source *source = site.source < compiler->source_count ? compiler->sources + site.source : 0;
		const char *source_name = source ? atom_data(source->name) : "<unknown>";
		if (site.line_index)
		{
			log_linef(level, "%s [%u:%llu] %s: %s", source_name, site.line_index, source_slice_column(site), severity_name, message->text.data);
		}
		else
		{
			log_linef(level, "%s [?] %s: %s", source_name, severity_name, message->text.data);
		}
		if (source && source_slice_is_valid(site))
		{
			print_source_slice_marker(site, source->source);
		}
	}
}

b32 compiler_report(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 result = compiler_reportv(compiler, error, severity, phase, site, format, args);
	va_end(args);
	return result;
}

EntityTable *entity_table_create(Arena *arena)
{
	EntityTable *table = dy_arena_push_zero(arena, sizeof(*table), _Alignof(EntityTable));
	if (table) {
		table->arena = arena;
	}
	return table;
}

Entity *entity_table_find(EntityTable *table, Atom *name)
{
	for (u32 i = 0; i < table->count; ++i)
	{
		Entity *entity = table->entities[i];
		if (entity->name == name) {
			return entity;
		}
	}
	return 0;
}

Entity *entity_table_insert(EntityTable *table, Entity *entity)
{
	Entity *result = entity_table_find(table, entity->name);
	if (!result)
	{
		if (table->count == table->capacity)
		{
			u32 capacity = table->capacity ? table->capacity * 2 : 16;
			Entity **entities = dy_arena_push(table->arena, sizeof(*entities) * capacity, _Alignof(Entity *));
			if (!entities) {
				return 0;
			}
			if (table->count) {
				dy_memory_copy(entities, table->entities, sizeof(*entities) * table->count);
			}
			table->entities = entities;
			table->capacity = capacity;
		}
		table->entities[table->count++] = entity;
		result = entity;
	}
	return result;
}

Entity *entity_alloc(Arena *arena, EntityKind kind, Atom *name)
{
	Entity *entity = dy_arena_push_zero(arena, sizeof(*entity), _Alignof(Entity));
	if (entity)
	{
		entity->kind = kind;
		entity->name = name;
	}
	return entity;
}

static Com_SourceId compiler_alloc_source(Compiler *compiler, Atom *name, Atom *path, BcSourceBuffer data)
{
	if (compiler->source_count == compiler->source_capacity)
	{
		u32 capacity = compiler->source_capacity ? compiler->source_capacity * 2 : 8;
		Com_Source *sources = dy_arena_push_zero(compiler->arena, capacity * sizeof(*sources), _Alignof(Com_Source));
		if (!sources) {
			return COM_INVALID_SOURCE_ID;
		}
		if (compiler->source_count) {
			dy_memory_copy(sources, compiler->sources, compiler->source_count * sizeof(*sources));
		}
		compiler->sources = sources;
		compiler->source_capacity = capacity;
	}
	Com_SourceId source_id = compiler->source_count++;
	Com_Source *source = compiler->sources + source_id;
	source->name = name;
	source->path = path;
	source->source = data;
	return source_id;
}

// NOTE(RJ): assumes the path is canonical
static Com_SourceId compiler_load_source(Compiler *compiler, SourceSite site, Atom *path_atom)
{
	for (u32 i = 0; i < compiler->source_count; ++i)
	{
		Com_Source *source = compiler->sources + i;
		if (source->path == path_atom) {
			return i;
		}
	}

	const char *error = 0;

	dy_String path = {};
	path.data = (char *) atom_data(path_atom);
	path.size = atom_size(path_atom);

	elf_StrSlice contents = {0};

	dy_File_Info info = {0};
	if (!dy_get_file_info(path, &info).error && !info.is_directory)
	{
		if (info.size <= UINT32_MAX)
		{
			dy_File file = {0};
			if (!dy_access_file(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
			{
				contents.data = dy_arena_push(compiler->arena, info.size + 1, 16);
				if (contents.data)
				{
					dy_u64 read = 0;
					if (!dy_read_file(file, contents.data, info.size, &read).error && read == info.size)
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

				dy_close_file(file);
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

	Com_SourceId source_id = COM_INVALID_SOURCE_ID;
	if (error)
	{
		compiler_report(compiler, COM_ERROR_SOURCE_PATH, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "%s '%s'", error, atom_data(path_atom));
	}
	else
	{
		source_id = compiler_alloc_source(compiler, path_atom, path_atom, (BcSourceBuffer) {.data = contents.data, .size = (u32)contents.size});
		if (source_id == COM_INVALID_SOURCE_ID) {
			compiler_report(compiler, COM_ERROR_SOURCE_PATH, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "out of memory while registering source file '%s'", atom_data(path_atom));
		}
	}
	return source_id;
}

static Ast parse_source_recursively(Compiler *compiler, Parser *parser, Com_SourceId source_id)
{
	ASSERT(source_id < compiler->source_count);
	Com_Source *source = compiler->sources + source_id;
	if (source->state != COM_SOURCE_UNPARSED) {
		return source->ast;
	}

	Ast ast_file = parser_parse_file(parser, source_id);
	if (ast_is_error(ast_file)) {
		return ast_file;
	}
	ASSERT(ast_file->kind == AST_FILE);
	source->ast = ast_file;
	source->state = COM_SOURCE_PARSED;

	Ast_Array imports = ast_file->file.imports;
	for (u32 i = 0; i < imports.count; ++i)
	{
		Ast import_ast = imports.items[i];
		ASSERT(import_ast);
		ASSERT(import_ast->kind == AST_IMPORT);
		ASSERT(import_ast->import.path);

		Atom *import_path_requested = import_ast->import.path;
		Com_SourceId imported_source_id = compiler_load_source(compiler, import_ast->site, import_path_requested);
		if (imported_source_id == COM_INVALID_SOURCE_ID) {
			return ERROR_AST;
		}
		import_ast->import.source = imported_source_id;
		Ast imported_ast = parse_source_recursively(compiler, parser, imported_source_id);
		if (ast_is_error(imported_ast)) {
			return imported_ast;
		}
	}

	return ast_file;
}

BcModule *compiler_compile(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	BcModule *module = 0;
	if (arena && name.data && name.size <= UINT32_MAX && source.data && source.size <= UINT32_MAX)
	{
		Compiler *compiler = compiler_create(arena);
		if (compiler)
		{
			Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
			char *source_data = dy_arena_push_zero(arena, source.size + 1, 1);
			if (source_name && source_data)
			{
				copy_memory(source_data, source.data, source.size);
				Com_SourceId source_id = compiler_alloc_source(compiler, source_name, source_name, (BcSourceBuffer) {.data = source_data, .size = (u32)source.size});
				if (source_id != COM_INVALID_SOURCE_ID)
				{
					Parser *parser = parser_create(compiler);
					if (parser)
					{
						Ast ast = parse_source_recursively(compiler, parser, source_id);

						parser_destroy(parser);

						if (!ast_is_error(ast) && !compiler->report.error_count)
						{
							checker_run(compiler, ast);
							IrModule *ir_module = ir_emit_module(compiler, ast);
							if (ir_module && !compiler->report.error_count)
							{
								module = bc_emit_module(ir_module);
								if (module)
								{
									module->sources = dy_arena_push_zero(arena, sizeof(*module->sources), _Alignof(BcSource));
									if (module->sources)
									{
										module->source_count = 1;
										module->root_source = source_id;
										module->sources[0].name = (BcString) {.data = atom_data(source_name), .size = atom_size(source_name)};
										module->sources[0].path = module->sources[0].name;
										module->sources[0].source = compiler->sources[source_id].source;
									}
									else
									{
										module = 0;
									}
								}
							}
						}
					}
				}
			}
			compiler_log_report(compiler);
		}
	}
	return module;
}

Ast compiler_parse_constant_expr(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source)
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

static Ast compiler_parse_json_source(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source, CompilerParseType type)
{
	Ast ast = ERROR_AST;
	if (arena && name.data && name.size <= UINT32_MAX && source.data && source.size <= UINT32_MAX)
	{
		Compiler *compiler = compiler_create(arena);
		if (compiler)
		{
			Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
			char *source_data = dy_arena_push_zero(arena, source.size + 1, 1);
			if (source_name && source_data)
			{
				copy_memory(source_data, source.data, source.size);
				Com_SourceId source_id = compiler_alloc_source(compiler, source_name, source_name, (BcSourceBuffer) {.data = source_data, .size = (u32)source.size});
				if (source_id != COM_INVALID_SOURCE_ID)
				{
					Parser *parser = parser_create(compiler);
					if (parser)
					{
						ast = type == COMPILER_PARSE_JSON ? parser_parse_json(parser, source_id) : parser_parse_jsonl(parser, source_id);
						parser_destroy(parser);
					}
				}
			}
			if (compiler->report.error_count) {
				ast = ERROR_AST;
			}
		}
	}
	return ast;
}

Ast compiler_parse_json(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	return compiler_parse_json_source(arena, name, source, COMPILER_PARSE_JSON);
}

Ast compiler_parse_jsonl(elf_Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	return compiler_parse_json_source(arena, name, source, COMPILER_PARSE_JSONL);
}




#if 0

static Com_SourceId compiler_register_source(Compiler *compiler, Atom *name, Atom *path, elf_StrSlice source_text, SourceSite site)
{
	ASSERT(name);
	ASSERT(path);
	Com_SourceId source_id = compiler_load_source(compiler, path);
	if (source_id != COM_INVALID_SOURCE_ID) {
		return source_id;
	}

	b32 source_is_oversized = source_text.size > UINT32_MAX;
	b32 source_is_invalid = source_text.size && !source_text.data;
	b32 source_has_nul = !source_is_oversized && !source_is_invalid && source_text.size && memchr(source_text.data, 0, source_text.size);
	if (source_is_oversized || source_is_invalid || source_has_nul)
	{
		if (compiler->source_count == 0)
		{
			source_id = compiler_append_source(compiler, name, path, (BcSourceBuffer) {.data = source_text.data});
			site.source = source_id;
		}
		if (source_is_oversized) {
			compiler_report(compiler, COM_ERROR_INVALID_SOURCE, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source exceeds the 32-bit size limit");
		}
		else if (source_is_invalid) {
			compiler_report(compiler, COM_ERROR_INVALID_SOURCE, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source data is null");
		}
		else {
			compiler_report(compiler, COM_ERROR_INVALID_SOURCE, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source contains a NUL byte");
		}
		return COM_INVALID_SOURCE_ID;
	}

	BcSourceBuffer source = compiler_copy_source(compiler, source_text);
	return compiler_append_source(compiler, name, path, source);
}

// TODO(RJ): dy-core :path addition!
static b32 compiler_path_is_absolute(Atom *path)
{
	const char *data = atom_data(path);
	u32 size = atom_size(path);
	return (size && (data[0] == '/' || data[0] == '\\')) || (size >= 2 && data[1] == ':');
}

static Atom *compiler_resolve_source_path(Compiler *compiler, Atom *importer_path, Atom *requested_path)
{
	Atom *result = 0;
	elf_Scratch scratch = elf_begin_scratch_excluding(compiler->arena);
	dy_String requested = dy_string_from_data(atom_data(requested_path), atom_size(requested_path));
	dy_String candidate = requested;
	if (importer_path && !compiler_path_is_absolute(requested_path))
	{
		const char *importer_data = atom_data(importer_path);
		u32 importer_size = atom_size(importer_path);
		u32 directory_size = 0;
		for (u32 i = importer_size; i > 0; --i)
		{
			if (importer_data[i - 1] == '/' || importer_data[i - 1] == '\\') {
				directory_size = i > 1 ? i - 1 : 1;
				break;
			}
		}

		if (directory_size)
		{
			u64 capacity = (u64)directory_size + requested.size + 2;
			char *storage = elf_arena_push(scratch.arena, capacity);
			dy_Path_Builder builder;
			if (storage && dy_path_builder_init(&builder, storage, capacity, dy_string_from_data(importer_data, directory_size)) && dy_path_push(&builder, requested)) {
				candidate = dy_string_from_data(builder.data, builder.size);
			}
			else {
				candidate = (dy_String) {0};
			}
		}
	}

	if (candidate.data && candidate.size)
	{
		dy_String canonical = {0};
		if (!dy_get_absolute_path(scratch.arena, candidate, &canonical).error && canonical.size <= UINT32_MAX) {
			result = atom_from_data_size(compiler->atoms, canonical.data, (u32)canonical.size);
		}
	}
	elf_end_scratch(scratch);
	return result;
}

static Com_SourceId compiler_load_source_file(Compiler *compiler, Atom *importer_path, Atom *requested_path, SourceSite site)
{
	Com_SourceId result = COM_INVALID_SOURCE_ID;
	Atom *path = compiler_resolve_source_path(compiler, importer_path, requested_path);
	if (!path)
	{
		if (compiler->source_count == 0) {
			site.source = compiler_append_source(compiler, requested_path, requested_path, (BcSourceBuffer) {});
		}
		compiler_report(compiler, COM_ERROR_SOURCE_PATH, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "failed to resolve source path '%s'", atom_data(requested_path));
	}
	else
	{
		result = compiler_load_source(compiler, path);
		if (result == COM_INVALID_SOURCE_ID)
		{
			elf_Scratch scratch = elf_begin_scratch_excluding(compiler->arena);
			dy_String path = dy_string_from_data(atom_data(path), atom_size(path));
			dy_File_Info info = {0};
			dy_File file = {0};
			elf_StrSlice source = {0};
			const char *error = 0;
			if (dy_get_file_info(path, &info).error || info.is_directory) {
				error = "could not find source file";
			}
			else if (info.size > UINT32_MAX) {
				error = "source file exceeds the 32-bit size limit";
			}
			else if (dy_access_file(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error) {
				error = "could not open source file";
			}
			else
			{
				source.data = elf_arena_push(scratch.arena, info.size + 1);
				if (!source.data) {
					error = "out of memory while loading source file";
				}
				else
				{
					dy_u64 read = 0;
					dy_Result read_result = dy_read_file(file, source.data, info.size, &read);
					if (read_result.error || read != info.size) {
						error = "could not read source file";
					}
					else {
						source.data[info.size] = 0;
						source.size = info.size;
					}
				}
			}

			if (dy_file_is_valid(file)) {
				dy_close_file(file);
			}
			if (error)
			{
				if (compiler->source_count == 0) {
					site.source = compiler_append_source(compiler, path, path, (BcSourceBuffer) {});
				}
				compiler_report(compiler, COM_ERROR_SOURCE_PATH, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "%s '%s'", error, atom_data(path));
			}
			else {
				result = compiler_register_source(compiler, path, path, source, site);
			}
			elf_end_scratch(scratch);
		}
	}
	return result;
}

Com_SourceId compiler_add_root_file(Compiler *compiler, const char *path)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count == 0);
	Com_SourceId source_id = COM_INVALID_SOURCE_ID;
	if (path)
	{
		Atom *requested_path = atom_from_data(compiler->atoms, path);
		if (requested_path) {
			source_id = compiler_load_source_file(compiler, 0, requested_path, (SourceSite) {});
		}
	}
	ASSERT(source_id == COM_INVALID_SOURCE_ID || source_id == 0);
	return source_id;
}

static b32 compiler_discover_source_imports(Compiler *compiler, Com_SourceId source, Ast file)
{
	ASSERT(source < compiler->source_count);
	ASSERT(file && file->kind == AST_FILE);
	Ast body = file->file.body;
	ASSERT(body && body->kind == AST_BLOCK_STAT);
	compiler->sources[source].imports = 0;
	compiler->sources[source].import_count = 0;
	compiler->sources[source].import_capacity = 0;
	Atom *importer_path = compiler->sources[source].path;

	b32 result = true;
	for (u32 i = 0; i < body->block.count; ++i)
	{
		Ast declaration = body->block.items[i];
		if (declaration->kind == AST_STATIC_DECL && declaration->static_decl.expr->kind == AST_IMPORT)
		{
			Ast import_ast = declaration->static_decl.expr;
			Com_Import *import = compiler_append_import(compiler, source, import_ast->import.path, import_ast->site);
			if (!import)
			{
				compiler_report(compiler, COM_ERROR_IMPORT, COMPILER_ERROR, COMPILER_PHASE_PARSER, import_ast->site, "failed to record import");
				result = false;
			}
			else
			{
				import->resolved_source = compiler_load_source_file(compiler, importer_path, import->path, import->site);
				if (import->resolved_source == COM_INVALID_SOURCE_ID) {
					result = false;
				}
			}
		}
	}
	return result;
}

b32 compiler_parse_sources(Compiler *compiler)
{
	Parser *parser = parser_create(compiler);
	if (!parser) {
		return false;
	}

	b32 result = true;
	for (Com_SourceId source_id = 0; result && source_id < compiler->source_count; ++source_id)
	{
		Ast ast = parser_parse_file(parser, source_id);
		compiler->sources[source_id].ast = ast;
		if (ast_is_error(ast))
		{
			if (compiler->report.error_count == 0) {
				compiler_report(compiler, COM_ERROR_PARSE_FAILED, COMPILER_ERROR, COMPILER_PHASE_PARSER, (SourceSite) {.source = source_id}, "failed to parse source");
			}
			result = false;
		}
		else
		{
			result = compiler_discover_source_imports(compiler, source_id, ast);
		}
	}
	parser_destroy(parser);
	return result;
}

static Com_StaticSymbol *compiler_static_symbol_from_scope(Com_StaticScope *scope, Atom *name)
{
	for (u32 i = 0; i < scope->symbol_count; ++i)
	{
		Com_StaticSymbol *symbol = scope->symbols + i;
		if (symbol->name == name) {
			return symbol;
		}
	}
	return 0;
}

Com_StaticSymbol *compiler_static_symbol_from_source(Compiler *compiler, Com_SourceId source, Atom *name)
{
	ASSERT(source < compiler->source_count);
	return compiler_static_symbol_from_scope(&compiler->sources[source].statics, name);
}

static Com_StaticSymbol *compiler_exported_symbol_from_source(Compiler *compiler, Com_SourceId source, Atom *name)
{
	Com_StaticSymbol *symbol = compiler_static_symbol_from_source(compiler, source, name);
	return symbol && symbol->exported ? symbol : 0;
}

static Com_Import *compiler_import_from_source(Compiler *compiler, Com_SourceId source, Atom *path)
{
	ASSERT(source < compiler->source_count);
	Com_Source *compiler_source = compiler->sources + source;
	for (u32 i = 0; i < compiler_source->import_count; ++i)
	{
		Com_Import *import = compiler_source->imports + i;
		if (import->path == path) {
			return import;
		}
	}
	return 0;
}

static u32 compiler_add_function(Compiler *compiler, Com_SourceId source, Ast ast)
{
	ASSERT(ast);
	b32 is_entry = ast->kind == AST_FILE;
	b32 variadic = true;
	u32 param_count = IMPLICIT_PARAM_COUNT;
	if (!is_entry)
	{
		ASSERT(ast->kind == AST_FUNCTION);
		variadic = ast->function.variadic != 0;
		param_count += ast->function.params.count;
	}
	else
	{
		ASSERT(ast->kind == AST_FILE);
	}

	ASSERT(compiler->function_count < compiler->function_capacity);
	u32 index = compiler->function_count++;
	compiler->functions[index] = (Com_Function) {
		.ast = ast,
		.source = source,
		.index = index,
		.param_count = param_count,
		.variadic = variadic,
	};
	return index;
}

static b32 compiler_evaluate_static_symbol(Compiler *compiler, Com_StaticSymbol *symbol);

Com_StaticLookup compiler_lookup_static_expr(Compiler *compiler, Com_SourceId source, Ast expr, Com_StaticValue *out)
{
	Com_StaticLookup result = COM_STATIC_LOOKUP_NONE;
	if (expr->kind == AST_IDENT)
	{
		Com_StaticSymbol *symbol = compiler_static_symbol_from_source(compiler, source, expr->atom);
		if (symbol)
		{
			symbol->referenced = true;
			if (compiler_evaluate_static_symbol(compiler, symbol))
			{
				*out = symbol->value;
				result = COM_STATIC_LOOKUP_VALUE;
			}
			else {
				result = COM_STATIC_LOOKUP_ERROR;
			}
		}
	}
	else if (expr->kind == AST_IMPORT)
	{
		Com_Import *import = compiler_import_from_source(compiler, source, expr->import.path);
		if (import && import->resolved_source < compiler->source_count)
		{
			out->kind = COM_STATIC_NAMESPACE;
			out->source = import->resolved_source;
			result = COM_STATIC_LOOKUP_VALUE;
		}
		else
		{
			compiler_report(compiler, COM_ERROR_IMPORT, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "import has no resolved source");
			result = COM_STATIC_LOOKUP_ERROR;
		}
	}
	else if (expr->kind == AST_FIELD)
	{
		Com_StaticValue receiver = {};
		Com_StaticLookup receiver_result = compiler_lookup_static_expr(compiler, source, expr->binary.x, &receiver);
		if (receiver_result == COM_STATIC_LOOKUP_ERROR) {
			result = COM_STATIC_LOOKUP_ERROR;
		}
		else if (receiver_result == COM_STATIC_LOOKUP_VALUE && receiver.kind == COM_STATIC_NAMESPACE)
		{
			Ast member = expr->binary.y;
			if (!member || member->kind != AST_STRING_LITERAL)
			{
				compiler_report(compiler, COM_ERROR_STATIC_LOOKUP, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "expected static namespace member name");
				result = COM_STATIC_LOOKUP_ERROR;
			}
			else
			{
				Com_StaticSymbol *symbol = compiler_exported_symbol_from_source(compiler, receiver.source, member->atom);
				if (!symbol)
				{
					compiler_report(compiler, COM_ERROR_STATIC_LOOKUP, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, member->site, "'%s' is not exported by the imported source", atom_data(member->atom));
					result = COM_STATIC_LOOKUP_ERROR;
				}
				else
				{
					symbol->referenced = true;
					if (compiler_evaluate_static_symbol(compiler, symbol))
					{
						*out = symbol->value;
						result = COM_STATIC_LOOKUP_VALUE;
					}
					else {
						result = COM_STATIC_LOOKUP_ERROR;
					}
				}
			}
		}
	}
	return result;
}

static b32 compiler_evaluate_static_expr(Compiler *compiler, Com_SourceId source, Ast expr, Com_StaticValue *out)
{
	b32 result = true;
	switch (expr->kind)
	{
		case AST_NIL_LITERAL:
		{
			out->kind = COM_STATIC_NIL;
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			out->kind = COM_STATIC_INTEGER;
			out->integer = expr->integer_value;
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			out->kind = COM_STATIC_NUMBER;
			out->number = expr->number_value;
		}
		break;

		case AST_STRING_LITERAL:
		{
			out->kind = COM_STATIC_STRING;
			out->string = expr->atom;
		}
		break;

		case AST_IDENT:
		case AST_IMPORT:
		case AST_FIELD:
		{
			Com_StaticLookup lookup = compiler_lookup_static_expr(compiler, source, expr, out);
			if (lookup == COM_STATIC_LOOKUP_NONE)
			{
				if (expr->kind == AST_IDENT) {
					compiler_report(compiler, COM_ERROR_STATIC_LOOKUP, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not a static symbol", atom_data(expr->atom));
				}
				else {
					compiler_report(compiler, COM_ERROR_INVALID_STATIC_EXPRESSION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not supported in a static expression", ast_type_name(expr->kind));
				}
				result = false;
			}
			else if (lookup == COM_STATIC_LOOKUP_ERROR) {
				result = false;
			}
		}
		break;

		default:
		{
			compiler_report(compiler, COM_ERROR_INVALID_STATIC_EXPRESSION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not supported in a static expression", ast_type_name(expr->kind));
			result = false;
		}
		break;
	}
	return result;
}

static b32 compiler_evaluate_static_symbol(Compiler *compiler, Com_StaticSymbol *symbol)
{
	ASSERT(symbol);
	b32 result = symbol->value.kind != COM_STATIC_INVALID;
	if (symbol->state == COM_STATIC_RESOLVING)
	{
		compiler_report(compiler, COM_ERROR_CYCLIC_STATIC_DEFINITION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, symbol->site, "cyclic static definition for '%s'", atom_data(symbol->name));
		result = false;
	}
	else if (symbol->state == COM_STATIC_UNRESOLVED)
	{
		symbol->state = COM_STATIC_RESOLVING;
		result = compiler_evaluate_static_expr(compiler, symbol->source, symbol->expr, &symbol->value);
		symbol->state = COM_STATIC_RESOLVED;
	}
	return result;
}

static b32 compiler_resolve_function_parameters(Compiler *compiler, Com_Function *function)
{
	ASSERT(function->ast && function->ast->kind == AST_FUNCTION);

	Ast ast = function->ast;
	u32 declared_param_count = ast->function.params.count;
	ASSERT(function->param_count == IMPLICIT_PARAM_COUNT + declared_param_count);
	if (declared_param_count)
	{
		function->params = elf_arena_push_zero(compiler->arena, sizeof(*function->params) * declared_param_count);
	}

	b32 result = true;
	for (u32 i = 0; i < declared_param_count; ++i)
	{
		Ast param = ast->function.params.items[i];
		if (!param || ast_is_error(param) || param->kind != AST_FUNCTION_PARAM || !param->param.name || param->param.name->kind != AST_IDENT)
		{
			compiler_report(compiler, COM_ERROR_INVALID_FUNCTION_PARAMETER, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "expected static function parameter");
			result = false;
			continue;
		}

		Com_FunctionParameter *resolved = function->params + i;
		resolved->name = param->param.name->atom;
		resolved->site = param->site;
		if (param->param.expr)
		{
			resolved->has_default = true;
			if (!compiler_evaluate_static_expr(compiler, function->source, param->param.expr, &resolved->default_value)) {
				result = false;
			}
			else if (resolved->default_value.kind == COM_STATIC_NAMESPACE)
			{
				compiler_report(compiler, COM_ERROR_INVALID_FUNCTION_PARAMETER, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, param->param.expr->site, "default for parameter '%s' cannot be an imported namespace", atom_data(resolved->name));
				resolved->default_value.kind = COM_STATIC_INVALID;
				result = false;
			}
		}
	}
	return result;
}

static Com_StaticScope compiler_extract_source_statics(Compiler *compiler, Com_SourceId source)
{
	Com_Source *compiler_source = compiler->sources + source;
	Ast file = compiler_source->ast;
	ASSERT(file && file->kind == AST_FILE);
	Ast body = file->file.body;
	ASSERT(body && body->kind == AST_BLOCK_STAT);

	Com_StaticScope result = {};
	if (body->block.count) {
		result.symbols = elf_arena_push_zero(compiler->arena, sizeof(*result.symbols) * body->block.count);
	}
	for (u32 i = 0; i < body->block.count; ++i)
	{
		Ast declaration = body->block.items[i];
		if (declaration->kind != AST_STATIC_DECL) {
			continue;
		}

		Ast name = declaration->static_decl.name;
		if (!name || ast_is_error(name) || name->kind != AST_IDENT)
		{
			compiler_report(compiler, COM_ERROR_INVALID_STATIC_DECLARATION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, declaration->site, "expected static declaration name");
			continue;
		}
		if (compiler_static_symbol_from_scope(&result, name->atom))
		{
			compiler_report(compiler, COM_ERROR_DUPLICATE_STATIC_DECLARATION, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, name->site, "'%s': is already declared", atom_data(name->atom));
			continue;
		}

		Com_StaticSymbol *symbol = result.symbols + result.symbol_count++;
		symbol->name = name->atom;
		symbol->site = name->site;
		symbol->expr = declaration->static_decl.expr;
		symbol->source = source;
		symbol->exported = declaration->static_decl.exported;
	}
	return result;
}

b32 compiler_resolve_statics(Compiler *compiler, Com_UnitType unit_type)
{
	compiler->functions = 0;
	compiler->function_count = 0;
	compiler->function_capacity = 0;
	u32 planned_function_count = unit_type == COM_UNIT_FUNCTION ? 1 : 0;

	for (Com_SourceId source = 0; source < compiler->source_count; ++source)
	{
		Com_Source *compiler_source = compiler->sources + source;
		compiler_source->statics = compiler_extract_source_statics(compiler, source);
		Ast file = compiler_source->ast;
		ASSERT(file && file->kind == AST_FILE);
		Ast body = file->file.body;
		ASSERT(body && body->kind == AST_BLOCK_STAT);
		for (u32 i = 0; i < body->block.count; ++i)
		{
			Ast declaration = body->block.items[i];
			if (declaration->kind != AST_STATIC_DECL)
			{
				if (unit_type == COM_UNIT_MODULE || source != 0) {
					compiler_report(compiler, COM_ERROR_EXECUTABLE_MODULE_STATEMENT, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, declaration->site, "static module cannot contain executable top-level statements");
				}
				continue;
			}
		}
		for (u32 i = 0; i < compiler_source->statics.symbol_count; ++i)
		{
			Com_StaticSymbol *symbol = compiler_source->statics.symbols + i;
			if (symbol->expr->kind == AST_FUNCTION) {
				planned_function_count += 1;
			}
		}
	}

	if (planned_function_count)
	{
		compiler->functions = elf_arena_push_zero(compiler->arena, sizeof(*compiler->functions) * planned_function_count);
	}
	compiler->function_capacity = planned_function_count;
	if (unit_type == COM_UNIT_FUNCTION)
	{
		compiler_add_function(compiler, 0, compiler->sources[0].ast);
	}

	for (Com_SourceId source = 0; source < compiler->source_count; ++source)
	{
		Com_StaticScope *scope = &compiler->sources[source].statics;
		for (u32 i = 0; i < scope->symbol_count; ++i)
		{
			Com_StaticSymbol *symbol = scope->symbols + i;
			if (symbol->expr->kind == AST_FUNCTION)
			{
				symbol->value.kind = COM_STATIC_FUNCTION;
				symbol->value.function = compiler_add_function(compiler, source, symbol->expr);
				symbol->state = COM_STATIC_RESOLVED;
			}
		}
	}
	ASSERT(compiler->function_count == compiler->function_capacity);
	for (Com_SourceId source = 0; source < compiler->source_count; ++source)
	{
		Com_StaticScope *scope = &compiler->sources[source].statics;
		for (u32 i = 0; i < scope->symbol_count; ++i) {
			compiler_evaluate_static_symbol(compiler, scope->symbols + i);
		}
	}
	for (u32 i = 0; i < compiler->function_count; ++i)
	{
		Com_Function *function = compiler->functions + i;
		if (function->ast->kind == AST_FUNCTION) {
			compiler_resolve_function_parameters(compiler, function);
		}
	}
	return compiler->report.error_count == 0;
}

void compiler_report_unreferenced_statics(Compiler *compiler, Com_SourceId source)
{
	ASSERT(source < compiler->source_count);
	Com_StaticScope *scope = &compiler->sources[source].statics;
	for (u32 i = 0; i < scope->symbol_count; ++i)
	{
		Com_StaticSymbol *symbol = scope->symbols + i;
		if (!symbol->referenced && !symbol->exported) {
			compiler_report(compiler, COM_ERROR_UNREFERENCED_ENTITY, COMPILER_WARNING, COMPILER_PHASE_LOWERING, symbol->site, "unreferenced entity");
		}
	}
}

// TODO(RJ): remove we already have utils that do this
static elf_StrSlice compiler_format_message(elf_Arena *arena, const char *format, va_list args)
{
	va_list count_args;
	va_copy(count_args, args);
	int message_size = vsnprintf(0, 0, format, count_args);
	va_end(count_args);
	if (message_size < 0) {
		return (elf_StrSlice) {0};
	}
	char *message = elf_arena_push(arena, (u64)message_size + 1);
	if (!message) {
		return (elf_StrSlice) {0};
	}
	vsnprintf(message, (u64)message_size + 1, format, args);
	return (elf_StrSlice) {message, (u64)message_size};
}

b32 compiler_reportv(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(compiler);
	ASSERT(error != COM_ERROR_NONE);
	elf_StrSlice message = compiler_format_message(compiler->arena, format, args);
	if (!message.data) {
		return false;
	}

	elf_arena_align(compiler->arena, _Alignof(Com_Message));
	Com_Message *compiler_message = elf_arena_push_zero(compiler->arena, sizeof(*compiler_message));
	if (!compiler_message) {
		return false;
	}
	compiler_message->error = error;
	compiler_message->severity = severity;
	compiler_message->phase = phase;
	compiler_message->site = site;
	compiler_message->text = message;
	if (compiler->report.last)
	{
		compiler->report.last->next = compiler_message;
	}
	else
	{
		compiler->report.first = compiler_message;
	}
	compiler->report.last = compiler_message;
	compiler->report.message_count += 1;

	if (severity == COMPILER_ERROR)
	{
		compiler->report.error_count += 1;
	}
	else if (severity == COMPILER_WARNING)
	{
		compiler->report.warning_count += 1;
	}

	return true;
}

b32 compiler_report(Compiler *compiler, Com_Error error, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 result = compiler_reportv(compiler, error, severity, phase, site, format, args);
	va_end(args);
	return result;
}

Ast compiler_parse_constant_expr(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count);
	Ast ast = ERROR_AST;
	if (!compiler->report.error_count)
	{
		Parser *parser = parser_create(compiler);
		if (parser)
		{
			ast = parser_parse_constant_expr(parser, 0);
			parser_destroy(parser);
		}
	}
	return ast;
}

Ast compiler_parse_json(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count);
	Ast ast = ERROR_AST;
	if (!compiler->report.error_count)
	{
		Parser *parser = parser_create(compiler);
		if (parser)
		{
			ast = parser_parse_json(parser, 0);
			parser_destroy(parser);
		}
	}
	return ast;
}

Ast compiler_parse_jsonl(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count);
	Ast ast = ERROR_AST;
	if (!compiler->report.error_count)
	{
		Parser *parser = parser_create(compiler);
		if (parser)
		{
			ast = parser_parse_jsonl(parser, 0);
			parser_destroy(parser);
		}
	}
	return ast;
}

static IrModule *ir_new_module(Compiler *compiler)
{
	elf_Arena *arena = compiler->arena;
	IrModule *module = elf_arena_push_zero(arena, sizeof(*module));
	module->compiler = compiler;
	module->arena = arena;
	module->function_capacity = 1024;
	module->functions = elf_arena_push_zero(arena, sizeof(*module->functions) * module->function_capacity);
	ASSERT(compiler->function_count <= module->function_capacity);
	for (u32 i = 0; i < compiler->function_count; ++i)
	{
		Com_Function *input = compiler->functions + i;
		Ast ast = input->ast;
		IrFunction *function = module->functions + i;
		function->site = ast->site;
		function->variadic = input->variadic;
		function->is_static = ast->kind == AST_FUNCTION;
		function->param_count = input->param_count;
		ASSERT(input->index == i);
	}
	module->function_count = compiler->function_count;
	return module;
}

static BcString compiler_copy_compiled_string(elf_Arena *arena, const char *data, u32 size)
{
	char *copy = elf_arena_push(arena, size + 1);
	if (size) {
		copy_memory(copy, data, size);
	}
	copy[size] = 0;
	return (BcString) {.data = copy, .size = size};
}

static BcString compiler_copy_compiled_atom(elf_Arena *arena, Atom *atom)
{
	return compiler_copy_compiled_string(arena, atom_data(atom), atom_size(atom));
}

static BcModule *compiler_package_module(Compiler *compiler, elf_Arena *output_arena, IrModule *ir_module, BcModule *input)
{
	ASSERT(input);
	elf_arena_align(output_arena, 8);
	BcModule *module = elf_arena_push_zero(output_arena, sizeof(*module));

	module->source_count = compiler->source_count;
	module->root_source = 0;
	if (module->source_count)
	{
		elf_arena_align(output_arena, 8);
		module->sources = elf_arena_push_zero(output_arena, sizeof(*module->sources) * module->source_count);
		for (u32 i = 0; i < module->source_count; ++i)
		{
			Com_Source *source = compiler->sources + i;
			BcSource *output_source = module->sources + i;
			output_source->name = compiler_copy_compiled_atom(output_arena, source->name);
			output_source->path = compiler_copy_compiled_atom(output_arena, source->path);
			BcString source_text = compiler_copy_compiled_string(output_arena, source->source.data, source->source.size);
			output_source->source = (BcSourceBuffer) {.data = source_text.data, .size = source_text.size};
		}
	}

	module->bytecode_count = input->bytecode_count;
	if (module->bytecode_count)
	{
		elf_arena_align(output_arena, 8);
		module->bytecode = elf_arena_push_copy(output_arena, sizeof(*module->bytecode) * module->bytecode_count, input->bytecode);
	}
	module->source_map_count = input->source_map_count;
	if (module->source_map_count)
	{
		module->source_map = dy_arena_push_copy(output_arena, sizeof(*module->source_map) * module->source_map_count, 8, input->source_map);
	}

	module->integer_constant_count = input->integer_constant_count;
	if (module->integer_constant_count)
	{
		elf_arena_align(output_arena, 8);
		module->integer_constants = elf_arena_push_copy(output_arena, sizeof(*module->integer_constants) * module->integer_constant_count, input->integer_constants);
	}

	module->number_constant_count = input->number_constant_count;
	if (module->number_constant_count)
	{
		elf_arena_align(output_arena, 8);
		module->number_constants = elf_arena_push_copy(output_arena, sizeof(*module->number_constants) * module->number_constant_count, input->number_constants);
	}

	module->string_count = input->atom_count;
	if (module->string_count)
	{
		elf_arena_align(output_arena, 8);
		module->strings = elf_arena_push_zero(output_arena, sizeof(*module->strings) * module->string_count);
		for (u32 i = 0; i < module->string_count; ++i) {
			module->strings[i] = compiler_copy_compiled_atom(output_arena, input->atoms[i]);
		}
	}

	module->global_count = input->global_count;
	if (module->global_count)
	{
		elf_arena_align(output_arena, 8);
		module->global_names = elf_arena_push_zero(output_arena, sizeof(*module->global_names) * module->global_count);
		for (u32 i = 0; i < module->global_count; ++i) {
			module->global_names[i] = input->global_names[i];
		}
	}

	module->global_relocation_count = input->global_relocation_count;
	if (module->global_relocation_count)
	{
		elf_arena_align(output_arena, 8);
		module->global_relocations = elf_arena_push_zero(output_arena, sizeof(*module->global_relocations) * module->global_relocation_count);
		for (u32 i = 0; i < module->global_relocation_count; ++i)
		{
			module->global_relocations[i].bytecode_index = input->global_relocations[i].bytecode_index;
			module->global_relocations[i].global_index = input->global_relocations[i].global_index;
		}
	}

	module->function_count = input->function_count;
	if (module->function_count)
	{
		elf_arena_align(output_arena, 8);
		module->functions = elf_arena_push_copy(output_arena, sizeof(*module->functions) * module->function_count, input->functions);
	}

	return module;
}

BcModule *compiler_compile(Compiler *compiler, elf_Arena *output_arena, Com_UnitType unit_type)
{
	ASSERT(compiler);
	ASSERT(output_arena);
	ASSERT(compiler->source_count);
	ASSERT(unit_type == COM_UNIT_FUNCTION || unit_type == COM_UNIT_MODULE);
	BcModule *module = 0;
	if (compiler->report.error_count) {
		return module;
	}

	b32 parsed = false;
	PROF_BLOCK("compiler.parse")
	{
		parsed = compiler_parse_sources(compiler);
	}
	if (!parsed || compiler->report.error_count)
	{
		return module;
	}

	PROF_BLOCK("compiler.resolve_statics")
	{
		parsed = compiler_resolve_statics(compiler, unit_type);
	}
	if (!parsed || compiler->report.error_count)
	{
		return module;
	}

	IrModule *ir_module = ir_new_module(compiler);
	IrEmitter *ir_function_emitter = ir_new_function_emitter(ir_module);
	PROF_BLOCK("compiler.lower")
	{
		for (u32 i = 0; i < compiler->function_count; ++i) {
			ir_emit_function(ir_function_emitter, compiler->functions + i);
		}
	}
	if (unit_type == COM_UNIT_FUNCTION) {
		compiler_report_unreferenced_statics(compiler, 0);
	}
	if (compiler->report.error_count) {
		return module;
	}
	ASSERT(ir_module->functions || ir_module->function_count == 0);
	BcModule *bc_module = bc_emit_module(ir_module);
	module = compiler_package_module(compiler, output_arena, ir_module, bc_module);
	return module;
}
#endif
