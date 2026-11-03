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
#include "ir.h"
#include "entity.h"

static b32 report(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...);
static b32 reportv(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args);

#include "logging.c"
#include "source_diagnostics.c"
#include "lexer.c"
#include "parser.c"
#include "ir.c"
#include "bc_emitter.c"
#include "checker.c"

static Com_Status inject_symbols(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymTable *table);

static Compiler *new_compiler(Arena *arena)
{
	if (!arena) {
		return 0;
	}

	Compiler *com = arena_push_zero(arena, sizeof(*com), _Alignof(Compiler));
	if (!com) {
		return 0;
	}
	com->arena = arena;

	com->atoms = arena_push_zero(arena, sizeof(*com->atoms), _Alignof(Atom_Table));
	if (!com->atoms) {
		return 0;
	}
	if (!atom_table_init(com->atoms, arena)) {
		return 0;
	}

	com->globals = scope_create(arena, 0, 0, 0);
	if (!com->globals) {
		return 0;
	}

	static Com_Type type_nil = {TRULE_NIL};
	static Com_Type type_num = {TRULE_NUMBER};
	static Com_Type type_int = {TRULE_INTEGER};
	static Com_Type type_fun = {TRULE_FUNCTION};
	static Com_Type type_user = {TRULE_USER};
	static Com_Type type_closure = {TRULE_CLOSURE};
	static Com_Type type_tab = {TRULE_TABLE};
	static Com_Type type_str = {TRULE_STRING};
	static Com_Type type_err = {TRULE_ERROR};
	static Com_Type type_buf = {TRULE_BUFFER};
	static Com_Type type_arr = {TRULE_ARRAY};
	static Com_Type type_all = {TRULE_ALL};
	static Com_Type type_numeric = {TRULE_NUMERIC};
	static Com_Type type_nonnil = {TRULE_NONNIL};
	static Com_Type type_obj = {TRULE_OBJECT};
	static Com_Type type_callable = {TRULE_CALLABLE};

	struct {
		const char *name;
		Com_Type   *type;
	}
	static type_decls[] = {
		{ "nil",      &type_nil      },
		{ "num",      &type_num      },
		{ "int",      &type_int      },
		{ "fun",      &type_fun      },
		{ "user",     &type_user     },
		{ "closure",  &type_closure  },
		{ "tab",      &type_tab      },
		{ "str",      &type_str      },
		{ "err",      &type_err      },
		{ "buf",      &type_buf      },
		{ "arr",      &type_arr      },
		{ "all",      &type_all      },
		{ "numeric",  &type_numeric  },
		{ "nonnil",   &type_nonnil   },
		{ "obj",      &type_obj      },
		{ "callable", &type_callable },
	};

	for (u32 i = 0; i < ARRAY_COUNT(type_decls); ++ i) {
		Entity *en = scope_declare(com->globals, atom_from_data(com->atoms, type_decls[i].name), ENTITY_TAG_STATIC | ENTITY_TAG_CONSTANT);
		en->value.type = COM_VALUE_TYPE;
		en->value.com_type = type_decls[i].type;
	}


	return com;
}

static Com_Status report_symbol(Com_Source *source, elf_SymField *field, elf_ErrorCode error, const char *message)
{
	const char *name = field->name.data ? field->name.data : "<invalid>";
	b32 reported = report(source, error, ERROR_ERROR, ERROR_PHASE_CHECKER, (SourceSite) {}, "environment symbol '%s' %s", name, message);
	return reported ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Com_Status inject_symbol_table(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymField *field, Atom *name)
{
	Entity *existing = scope_find_current(scope, name);
	if (existing)
	{
		if (existing->declaration || existing->value.type != COM_VALUE_SCOPE)
		{
			return report_symbol(source, field, ELF_ERROR_DUPLICATE_DECLARATION, "uses an existing value as a namespace");
		}
		return inject_symbols(compiler, existing->value.scope, source, field->table);
	}

	Entity *entity = scope_declare(scope, name, ENTITY_TAG_STATIC | ENTITY_TAG_CONSTANT);
	if (!entity)
	{
		return COM_STATUS_OOM;
	}

	ASSERT(entity->count == 1);

	Scope *members = scope_create(compiler->arena, scope, 0, 0);
	if (!members)
	{
		return COM_STATUS_OOM;
	}
	entity->value.type = COM_VALUE_SCOPE;
	entity->value.scope = members;
	return inject_symbols(compiler, members, source, field->table);
}

static Com_Status inject_symbol_value(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymField *field, Atom *name)
{
	if (scope_find_current(scope, name))
	{
		return report_symbol(source, field, ELF_ERROR_DUPLICATE_DECLARATION, "conflicts with an existing host symbol");
	}

	Entity *entity = scope_declare(scope, name, ENTITY_TAG_STATIC | ENTITY_TAG_CONSTANT);
	if (!entity)
	{
		return COM_STATUS_OOM;
	}
	ASSERT(entity->count == 1);

	Com_Value *value = &entity->value;
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
				return report_symbol(source, field, ELF_ERROR_OUT_OF_RANGE, "has an invalid string value");
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
				return report_symbol(source, field, error, "could not be installed in the target state");
			}
			value->type = COM_VALUE_NATIVE_FUNCTION;
			value->native_function_index = native_function_index;
		}
		break;
		default:
		{
			return report_symbol(source, field, ELF_ERROR_INTERNAL, "has an invalid value type");
		}
	}
	return COM_STATUS_OK;
}

// TODO(RJ): the only reason this takes a source is for error reporting, the we should have a compiler wide
// dummy source for 'sourceless' diagnostics.
static Com_Status inject_symbols(Compiler *compiler, Scope *scope, Com_Source *source, elf_SymTable *table)
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
			return report_symbol(source, field, ELF_ERROR_INVALID_DECLARATION, "has an invalid name");
		}
		Atom *name = atom_from_data_size(compiler->atoms, field->name.data, (u32)field->name.size);
		if (!name)
		{
			return COM_STATUS_OOM;
		}

		Com_Status status = field->type == ELF_SYMBOL_TABLE ? inject_symbol_table(compiler, scope, source, field, name) : inject_symbol_value(compiler, scope, source, field, name);
		if (status != COM_STATUS_OK)
		{
			return status;
		}
	}
	return COM_STATUS_OK;
}

static b32 reportv(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(source);
	Compiler *compiler = source->compiler;
	ASSERT(compiler);
	ASSERT(code != ELF_ERROR_NONE);
	dy_String message = arena_pushfv(compiler->arena, format, args);
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
	compiler_message->text = (elf_StrSlice) {message.data, message.size};
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

static void log_report(Compiler *compiler)
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

static b32 report(Com_Source *source, elf_ErrorCode code, ErrorSeverity severity, ErrorPhase phase, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 result = reportv(source, code, severity, phase, site, format, args);
	va_end(args);
	return result;
}

static Com_Source *alloc_source(Compiler *com, Atom *name, Atom *path, elf_Source *runtime_source, dy_String data)
{
	Com_Source *source = arena_push_zero(com->arena, sizeof(*source), _Alignof(Com_Source));
	if (!source) {
		return 0;
	}
	source->compiler = com;
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
	if (com->last_source)
	{
		com->last_source->next = source;
		if (com->last_source->runtime_source && runtime_source)
		{
			com->last_source->runtime_source->next_compilation_source = runtime_source;
		}
	}
	else
	{
		com->first_source = source;
	}
	com->last_source = source;
	return source;
}

static dy_String read_source_file(Arena *arena, dy_String path, dy_String *contents)
{
	ASSERT(arena);
	ASSERT(contents);
	*contents = (dy_String) {0};

	dy_File_Info info = {0};
	if (dy_file_info(path, &info).error || info.is_directory)
	{
		return DY_LIT("could not find source file");
	}
	if (info.size > UINT32_MAX)
	{
		return DY_LIT("source file exceeds the 32-bit size limit");
	}

	dy_File file = {0};
	if (dy_file_open(path, DY_FILE_OPEN_EXISTING, DY_FILE_READ | DY_FILE_SHARE_READ, &file).error)
	{
		return DY_LIT("could not open source file");
	}

	contents->data = arena_push(arena, info.size + 1, 1);
	if (!contents->data)
	{
		dy_file_close(file);
		return DY_LIT("out of memory while loading source file");
	}

	dy_u64 read = 0;
	dy_Result read_result = dy_file_read(file, contents->data, info.size, &read);
	dy_file_close(file);
	if (read_result.error || read != info.size)
	{
		*contents = (dy_String) {0};
		return DY_LIT("could not read source file");
	}

	contents->data[info.size] = 0;
	contents->size = info.size;
	return (dy_String) {0};
}

// NOTE(RJ): assumes the path is canonical
static Com_Source *load_source(Compiler *com, Com_Source *req, SourceSite site, Atom *path_atom)
{
	ASSERT(com->target);
	ASSERT(req);

	for (Com_Source *source = com->first_source; source; source = source->next)
	{
		if (source->path == path_atom)
		{
			return source;
		}
	}

	dy_String path = dy_string_from_data(atom_data(path_atom), atom_size(path_atom));
	u64 arena_checkpoint = arena_mark(com->arena);
	dy_String contents = {0};
	dy_String load_error = read_source_file(com->arena, path, &contents);
	if (load_error.data)
	{
		arena_restore(com->arena, arena_checkpoint);
		report(req, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "%.*s '%s'", (i32)load_error.size, load_error.data, atom_data(path_atom));
		return 0;
	}

	elf_Source *runtime_source = elf_source_create(com->target, path, contents);
	arena_restore(com->arena, arena_checkpoint);
	if (!runtime_source)
	{
		report(req, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "out of memory while creating source file '%s'", atom_data(path_atom));
		return 0;
	}

	Com_Source *source = alloc_source(com, path_atom, path_atom, runtime_source, (dy_String) {0});
	if (!source)
	{
		report(req, ELF_ERROR_SOURCE_PATH, ERROR_ERROR, ERROR_PHASE_PARSER, site, "out of memory while registering source file '%s'", atom_data(path_atom));
	}
	return source;
}

static Com_Status parse_source_recursively(Compiler *compiler, Parser *parser, Com_Source *source)
{
	ASSERT(source);
	if (source->state != COM_SOURCE_UNPARSED) {
		return COM_STATUS_OK;
	}

	Ast ast = parser_parse_source(parser, source);
	if (ast_is_error(ast)) {
		return COM_STATUS_REPORTED;
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
		Com_Source *imported_source = load_source(compiler, source, import_ast->site, import_path_requested);
		if (!imported_source) {
			return COM_STATUS_REPORTED;
		}
		import_ast->import.target = imported_source;
		Com_Status status = parse_source_recursively(compiler, parser, imported_source);
		if (status != COM_STATUS_OK) {
			return status;
		}
	}

	return COM_STATUS_OK;
}

static elf_Prototype *compile(elf_State *target, elf_SymTable *symbols, Arena *arena, elf_StrSlice name, elf_StrSlice source)
{
	if (!target || !arena || !name.data || name.size > UINT32_MAX || !source.data || source.size > UINT32_MAX)
	{
		return 0;
	}

	Compiler *com = new_compiler(arena);
	if (!com)
	{
		return 0;
	}
	com->target = target;

	Atom *source_name = atom_from_data_size(com->atoms, name.data, (u32)name.size);
	dy_String runtime_name = {(char *)name.data, name.size};
	dy_String runtime_text = {(char *)source.data, source.size};
	elf_Source *runtime_source = elf_source_create(target, runtime_name, runtime_text);
	if (!source_name || !runtime_source)
	{
		return 0;
	}

	Com_Source *com_source = alloc_source(com, source_name, source_name, runtime_source, (dy_String) {0});
	if (!com_source) {
		return 0;
	}

	Com_Status inject_status = inject_symbols(com, com->globals, com_source, com->target->symbols);
	if (inject_status == COM_STATUS_OK && symbols != com->target->symbols) {
		inject_status = inject_symbols(com, com->globals, com_source, symbols);
	}

	if (inject_status != COM_STATUS_OK) {
		report(com_source, ELF_ERROR_OUT_OF_MEMORY, ERROR_FATAL, ERROR_PHASE_CHECKER, (SourceSite) {0}, "could not inject environment symbols");
		return 0;
	}

	// TODO(RJ): why the fuck do we need to preinitialize the parser, just make this one
	// atomic parse operation ...
	Parser parser = {0};
	if (!parser_init(&parser)) {
		return 0;
	}

	if (parse_source_recursively(com, &parser, com_source) != COM_STATUS_OK) {
		log_report(com);
		return 0;
	}

	// NOTE(RJ): only destroys the scratch arena ...
	parser_destroy(&parser);

	IrModule ir_module;
	if (ir_module_init(&ir_module, com->arena) != COM_STATUS_OK) {
		log_report(com);
		return 0;
	}

	elf_Value *root_checkpoint = target->stack_top;
	if (check(&ir_module, com) != COM_STATUS_OK) {
		target->stack_top = root_checkpoint;
		log_report(com);
		return 0;
	}

	elf_Prototype *entry = bc_emit_module(com->target, &ir_module);
	target->stack_top = root_checkpoint;
	log_report(com);
	return entry;
}

#if !defined(ELF_COM_TEST_COPY)

elf_ErrorCode elf_push_code_source(elf_State *state, elf_SymTable *symbols, elf_StrSlice name, elf_StrSlice source)
{
	if (!state || !name.data || !source.data)
	{
		return ELF_ERROR_INVALID_ARGUMENT;
	}

	Scratch scratch = scratch_begin();
	elf_Prototype *proto = compile(state, symbols, scratch.arena, name, source);
	scratch_end(scratch);

	if (proto) {
		push_value(state, proto_value(proto));
		return ELF_ERROR_NONE;
	}
	return ELF_ERROR_COMPILATION_FAILED;
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
	dy_String path = dy_string_from_data(name.data, name.size);
	dy_String source = {0};
	dy_String load_error = read_source_file(scratch.arena, path, &source);
	if (load_error.data)
	{
		scratch_end(scratch);
		elf_push_nil(state);
		return false;
	}

	elf_ErrorCode error = elf_push_code_source(state, symbols, name, (elf_StrSlice) {source.data, source.size});
	scratch_end(scratch);
	return error == ELF_ERROR_NONE;
}


#include "lib_com.c"

#endif

