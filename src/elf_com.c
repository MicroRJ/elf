//
// See Copyright Notice In elf.h
//

#define _CRT_SECURE_NO_WARNINGS
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <dy.h>
#include <dy_fs.h>

#include "elf.h"
#include "elf_runtime.h"
#include "base.h"
#include "bytecode.h"
#include "compiler.h"
#include "token.h"
#include "atom.c"
#include "ast.h"
#include "entity.h"
#include "ir.h"

static b32 compiler_report(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...);
static b32 compiler_reportv(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args);

#include "logging.c"
#include "source_diagnostics.c"
#include "lexer.c"
#include "parser.c"
#include "entity.c"
#include "checker.c"
#include "ir_emitter.c"
#include "bc_emitter.c"


static void compiler_log_report(Compiler *compiler);
static Compiler *compiler_create(Arena *arena);
static Com_Status compiler_inject_symbols(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymTable *table);


static Compiler *compiler_create(Arena *arena)
{
	if (!arena) {
		return 0;
	}

	Compiler *compiler = arena_push_zero(arena, sizeof(*compiler), _Alignof(Compiler));
	if (!compiler) {
		return 0;
	}
	compiler->arena = arena;

	compiler->atoms = arena_push_zero(arena, sizeof(*compiler->atoms), _Alignof(Atom_Table));
	if (!compiler->atoms) {
		return 0;
	}
	if (!atom_table_init(compiler->atoms, arena)) {
		return 0;
	}

	compiler->globals = scope_create(arena, SCOPE_GLOBAL, 0, 0, 0);
	if (!compiler->globals) {
		return 0;
	}
	return compiler;
}

static Com_Status compiler_report_symbol(Com_Source *source, elf_SymField *field, elf_ErrorCode error, const char *message)
{
	const char *name = field->name.data ? field->name.data : "<invalid>";
	b32 reported = compiler_report(source, error, ERROR_ERROR, ERROR_PHASE_CHECKER, (SourceSite) {}, "environment symbol '%s' %s", name, message);
	return reported ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Com_Status compiler_inject_symbol_table(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymField *field, Atom *name)
{
	Entity *existing = scope_find_current(scope, name);
	if (existing)
	{
		if (existing->kind != ENTITY_GLOBAL || existing->en_global.value.type != COM_VALUE_SCOPE)
		{
			return compiler_report_symbol(source, field, ELF_ERROR_DUPLICATE_DECLARATION, "uses an existing value as a namespace");
		}
		return compiler_inject_symbols(compiler, existing->en_global.value.scope, source, field->table);
	}

	Entity *entity = scope_declare(scope, ENTITY_GLOBAL, name);
	if (!entity)
	{
		return COM_STATUS_OOM;
	}

	// TODO(RJ):
	ASSERT(entity->count == 1);

	Scope *members = scope_create(compiler->arena, SCOPE_NAMESPACE, scope, 0, 0);
	if (!members)
	{
		return COM_STATUS_OOM;
	}
	entity->en_global.value.type = COM_VALUE_SCOPE;
	entity->en_global.value.scope = members;
	return compiler_inject_symbols(compiler, members, source, field->table);
}

static Com_Status compiler_inject_symbol_value(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymField *field, Atom *name)
{
	if (scope_find_current(scope, name))
	{
		return compiler_report_symbol(source, field, ELF_ERROR_DUPLICATE_DECLARATION, "conflicts with an existing host symbol");
	}

	Entity *entity = scope_declare(scope, ENTITY_GLOBAL, name);
	if (!entity)
	{
		return COM_STATUS_OOM;
	}
	ASSERT(entity->count == 1);

	Com_Value *value = &entity->en_global.value;
	switch (field->type)
	{
		case ELF_SYMBOL_INTEGER:
		{
			value->type = COM_VALUE_INTEGER;
			value->integer = field->integer;
		}
		break;
		case ELF_SYMBOL_NUMBER:
		{
			value->type = COM_VALUE_NUMBER;
			value->number = field->number;
		}
		break;
		case ELF_SYMBOL_STRING:
		{
			if (!field->string.data || field->string.size > UINT32_MAX)
			{
				return compiler_report_symbol(source, field, ELF_ERROR_OUT_OF_RANGE, "has an invalid string value");
			}
			value->type = COM_VALUE_STRING;
			value->string = atom_from_data_size(compiler->atoms, field->string.data, (u32)field->string.size);
			if (!value->string)
			{
				return COM_STATUS_OOM;
			}
		}
		break;
		case ELF_SYMBOL_FUNCTION:
		{
			u32 native_function_index = UINT32_MAX;
			elf_ErrorCode error = elf_native_function_ensure(compiler->target, field->function, &native_function_index);
			if (error == ELF_ERROR_OUT_OF_MEMORY)
			{
				return COM_STATUS_OOM;
			}
			if (error != ELF_ERROR_NONE)
			{
				return compiler_report_symbol(source, field, error, "could not be installed in the target state");
			}
			value->type = COM_VALUE_NATIVE_FUNCTION;
			value->native_function_index = native_function_index;
		}
		break;
		default:
		{
			return compiler_report_symbol(source, field, ELF_ERROR_INTERNAL, "has an invalid value type");
		}
	}
	return COM_STATUS_OK;
}

static Com_Status compiler_inject_symbols(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymTable *table)
{
	ASSERT(compiler);
	ASSERT(compiler->target);
	ASSERT(scope);
	ASSERT(source);
	if (!table)
	{
		return COM_STATUS_OK;
	}

	for (elf_SymField *field = table->first_field; field; field = field->next)
	{
		if (!field->name.data || !field->name.size || field->name.size > UINT32_MAX)
		{
			return compiler_report_symbol(source, field, ELF_ERROR_INVALID_DECLARATION, "has an invalid name");
		}
		Atom *name = atom_from_data_size(compiler->atoms, field->name.data, (u32)field->name.size);
		if (!name)
		{
			return COM_STATUS_OOM;
		}

		Com_Status status = field->type == ELF_SYMBOL_TABLE ? compiler_inject_symbol_table(compiler, scope, source, field, name) : compiler_inject_symbol_value(compiler, scope, source, field, name);
		if (status != COM_STATUS_OK)
		{
			return status;
		}
	}
	return COM_STATUS_OK;
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

static b32 compiler_reportv(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(source);
	Compiler *compiler = source->compiler;
	ASSERT(compiler);
	ASSERT(code != ELF_ERROR_NONE);
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

static void compiler_log_report(Compiler *compiler)
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

static b32 compiler_report(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...)
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
		if (source->path == path_atom)
		{
			return source;
		}
	}

	dy_String path = dy_string_from_data(atom_data(path_atom), atom_size(path_atom));

	dy_File_Info info = {0};
	if (dy_file_info(path, &info).error || info.is_directory)
	{
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "could not find source file '%s'", atom_data(path_atom));
		return 0;
	}

	if (info.size > UINT32_MAX)
	{
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "source file exceeds the 32-bit size limit '%s'", atom_data(path_atom));
		return 0;
	}

	dy_File file = {0};
	if (dy_file_open(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "could not open source file '%s'", atom_data(path_atom));
		return 0;
	}

	u64 arena_checkpoint = arena_mark(compiler->arena);
	dy_String contents = {.data = arena_push(compiler->arena, info.size + 1, 16)};
	const char *load_error = 0;
	if (!contents.data)
	{
		load_error = "out of memory while loading source file";
	}
	else
	{
		dy_u64 read = 0;
		dy_Result read_result = dy_file_read(file, contents.data, info.size, &read);
		if (read_result.error || read != info.size)
		{
			load_error = "could not read source file";
		}
		else
		{
			contents.data[info.size] = 0;
			contents.size = info.size;
		}
	}
	dy_file_close(file);

	if (load_error)
	{
		arena_restore(compiler->arena, arena_checkpoint);
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "%s '%s'", load_error, atom_data(path_atom));
		return 0;
	}

	elf_Source *runtime_source = elf_source_create(compiler->target, path, contents);
	arena_restore(compiler->arena, arena_checkpoint);
	if (!runtime_source)
	{
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "out of memory while creating source file '%s'", atom_data(path_atom));
		return 0;
	}

	Com_Source *source = compiler_alloc_source(compiler, path_atom, path_atom, runtime_source, (dy_String) {0});
	if (!source)
	{
		compiler_report(requesting_source, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "out of memory while registering source file '%s'", atom_data(path_atom));
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

static elf_Prototype *compiler_compile_source(Compiler *compiler, Com_Source *source, elf_SymTable *symbols)
{
	Com_Status inject_status = compiler_inject_symbols(compiler, compiler->globals, source, compiler->target->symbols);
	if (inject_status == COM_STATUS_OK && symbols != compiler->target->symbols)
	{
		inject_status = compiler_inject_symbols(compiler, compiler->globals, source, symbols);
	}
	if (inject_status == COM_STATUS_OOM)
	{
		compiler_report(source, ELF_ERROR_OUT_OF_MEMORY, ERROR_FATAL, ERROR_PHASE_CHECKER, (SourceSite) {0}, "could not inject environment symbols");
	}
	if (inject_status != COM_STATUS_OK)
	{
		return 0;
	}

	Parser parser = {0};
	if (!parser_init(&parser))
	{
		return 0;
	}
	Ast ast = parse_source_recursively(compiler, &parser, source);
	parser_destroy(&parser);
	if (ast_is_error(ast) || compiler->error_count)
	{
		return 0;
	}

	if (check_source(source) != COM_STATUS_OK)
	{
		return 0;
	}

	IrModule *ir_module = ir_emit_module(source);
	if (!ir_module || compiler->error_count)
	{
		return 0;
	}
	return bc_emit_module(compiler->target, ir_module);
}

static elf_Prototype *compiler_compile(elf_State *target, elf_SymTable *symbols, Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	if (!target || !arena || !name.data || name.size > UINT32_MAX || !source.data || source.size > UINT32_MAX)
	{
		return 0;
	}

	Compiler *compiler = compiler_create(arena);
	if (!compiler)
	{
		return 0;
	}
	compiler->target = target;

	Atom *source_name = atom_from_data_size(compiler->atoms, name.data, (u32)name.size);
	dy_String runtime_name = {(char *)name.data, name.size};
	dy_String runtime_text = {(char *)source.data, source.size};
	elf_Source *runtime_source = elf_source_create(target, runtime_name, runtime_text);
	if (!source_name || !runtime_source)
	{
		return 0;
	}

	Com_Source *compiler_source = compiler_alloc_source(compiler, source_name, source_name, runtime_source, (dy_String) {0});
	if (!compiler_source)
	{
		return 0;
	}

	elf_Prototype *entry = compiler_compile_source(compiler, compiler_source, symbols);
	compiler_log_report(compiler);
	return entry;
}

#if !defined(ELF_COM_TEST_COPY)

static b32 push_compiled_prototype(elf_State *state, elf_Prototype *proto)
{
	if (!proto)
	{
		return false;
	}
	return true;
}

elf_ErrorCode elf_push_code_source(elf_State *state, elf_SymTable *symbols, elf_StrSlice name, elf_StrSlice source)
{
	if (!state || !name.data || !source.data)
	{
		return ELF_ERROR_INVALID_ARGUMENT;
	}

	Scratch scratch = scratch_begin();
	elf_Prototype *proto = compiler_compile(state, symbols, scratch.arena, name, source);
	scratch_end(scratch);

	if (proto) {
		push_value(state, proto_value(proto));
		return ELF_ERROR_NONE;
	}
	return ELF_ERROR_COMPILATION_FAILED;
}

static b32 compiler_load_source_file(Arena *arena, elf_StrSlice name, elf_StrSlice *source)
{
	*source = (elf_StrSlice) {0};
	dy_String path = {name.data, name.size};
	dy_File_Info info = {0};
	if (dy_file_info(path, &info).error || info.is_directory || info.size > UINT32_MAX)
	{
		return false;
	}

	dy_File file = {0};
	if (dy_file_open(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		return false;
	}

	char *data = arena_push(arena, info.size + 16, 1);
	if (!data)
	{
		dy_file_close(file);
		return false;
	}

	dy_u64 read = 0;
	dy_Result read_result = dy_file_read(file, data, info.size, &read);
	dy_file_close(file);
	if (read_result.error || read != info.size)
	{
		return false;
	}

	zero_memory(data + info.size, 16);
	*source = (elf_StrSlice) {data, info.size};
	return true;
}

elf_Bool elf_push_code_file(elf_State *state, elf_SymTable *symbols, elf_StrSlice name)
{
	if (!state)
	{
		return false;
	}
	if (!name.data)
	{
		elf_push_nil(state);
		return false;
	}

	Scratch scratch = scratch_begin();
	elf_StrSlice source = {0};
	b32 loaded = compiler_load_source_file(scratch.arena, name, &source);
	if (!loaded)
	{
		scratch_end(scratch);
		elf_push_nil(state);
		return false;
	}

	elf_ErrorCode error = elf_push_code_source(state, symbols, name, source);
	scratch_end(scratch);
	return error == ELF_ERROR_NONE;
}


#include "lib_com.c"

#endif

