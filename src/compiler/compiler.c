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
#include "atom.c"
#include "lexer.c"
#include "ast.c"
#include "parser.c"
#include "parse_json.c"

#include "ir.c"
#include "ir_emitter.c"
#include "bc_emitter.c"

static Com_SourceId compiler_append_source(Compiler *compiler, Atom *name, Atom *canonical_path, SourceBuffer source)
{
	if (compiler->source_count == compiler->source_capacity)
	{
		u32 capacity = compiler->source_capacity ? compiler->source_capacity * 2 : 4;
		Com_Source *sources = elf_arena_push_zero(compiler->arena, sizeof(*sources) * capacity);
		if (compiler->source_count) {
			copy_memory(sources, compiler->sources, sizeof(*sources) * compiler->source_count);
		}
		compiler->sources = sources;
		compiler->source_capacity = capacity;
	}

	Com_SourceId source_id = compiler->source_count++;
	Com_Source *compiler_source = compiler->sources + source_id;
	compiler_source->name = name;
	compiler_source->canonical_path = canonical_path;
	compiler_source->source = source;
	return source_id;
}

static void init_atoms(Compiler *compiler)
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
	Compiler *compiler = elf_arena_push_zero(arena, sizeof(*compiler));
	compiler->arena = arena;
	compiler->atoms = elf_arena_push_zero(arena, sizeof(*compiler->atoms));
	atom_table_init(compiler->atoms, arena);
	init_atoms(compiler);
	compiler->log_messages = true;
	return compiler;
}

void compiler_set_log_messages(Compiler *compiler, b32 enabled)
{
	compiler->log_messages = enabled;
}

b32 compiler_record_import(Compiler *compiler, Com_SourceId source_id, Ast import_ast)
{
	ASSERT(source_id < compiler->source_count);
	ASSERT(import_ast && import_ast->kind == AST_IMPORT);
	Com_Source *source = compiler->sources + source_id;
	if (source->import_count == source->import_capacity)
	{
		u32 capacity = source->import_capacity ? source->import_capacity * 2 : 4;
		Ast *imports = elf_arena_push_zero(compiler->arena, sizeof(*imports) * capacity);
		if (!imports) {
			return false;
		}
		if (source->import_count) {
			copy_memory(imports, source->imports, sizeof(*imports) * source->import_count);
		}
		source->imports = imports;
		source->import_capacity = capacity;
	}
	source->imports[source->import_count++] = import_ast;
	return true;
}

static Com_SourceId compiler_find_source(Compiler *compiler, Atom *canonical_path)
{
	Com_SourceId result = COM_SOURCE_INVALID;
	for (Com_SourceId source_id = 0; source_id < compiler->source_count; ++source_id)
	{
		if (compiler->sources[source_id].canonical_path == canonical_path) {
			result = source_id;
			break;
		}
	}
	return result;
}

static SourceBuffer compiler_copy_source(Compiler *compiler, elf_StrSlice source)
{
	ASSERT(source.size <= 0xffffffffu);
	char *data = elf_arena_push(compiler->arena, source.size + 1);
	if (source.size) {
		copy_memory(data, source.data, source.size);
	}
	data[source.size] = 0;
	return (SourceBuffer) {.data = data, .size = (u32)source.size};
}

static Com_SourceId compiler_register_source(Compiler *compiler, Atom *name, Atom *canonical_path, elf_StrSlice source_text, SourceSite site)
{
	ASSERT(name);
	ASSERT(canonical_path);
	Com_SourceId source_id = compiler_find_source(compiler, canonical_path);
	if (source_id != COM_SOURCE_INVALID) {
		return source_id;
	}

	b32 source_is_oversized = source_text.size > UINT32_MAX;
	b32 source_is_invalid = source_text.size && !source_text.data;
	b32 source_has_nul = !source_is_oversized && !source_is_invalid && source_text.size && memchr(source_text.data, 0, source_text.size);
	if (source_is_oversized || source_is_invalid || source_has_nul)
	{
		if (compiler->source_count == 0)
		{
			source_id = compiler_append_source(compiler, name, canonical_path, (SourceBuffer) {.data = source_text.data});
			site.source = source_id;
		}
		if (source_is_oversized) {
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source exceeds the 32-bit size limit");
		}
		else if (source_is_invalid) {
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source data is null");
		}
		else {
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_LEXER, site, "source contains a NUL byte");
		}
		return COM_SOURCE_INVALID;
	}

	SourceBuffer source = compiler_copy_source(compiler, source_text);
	return compiler_append_source(compiler, name, canonical_path, source);
}

Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source)
{
	ASSERT(compiler->source_count == 0);
	Atom *source_name = atom_from_data(compiler->atoms, name ? name : "<unknown>");
	Com_SourceId source_id = compiler_register_source(compiler, source_name, source_name, source, (SourceSite) {});
	ASSERT(source_id == COM_SOURCE_INVALID || source_id == 0);
	return source_id;
}

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
	Com_SourceId result = COM_SOURCE_INVALID;
	Atom *canonical_path = compiler_resolve_source_path(compiler, importer_path, requested_path);
	if (!canonical_path)
	{
		if (compiler->source_count == 0) {
			site.source = compiler_append_source(compiler, requested_path, requested_path, (SourceBuffer) {});
		}
		compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "failed to resolve source path '%s'", atom_data(requested_path));
	}
	else
	{
		result = compiler_find_source(compiler, canonical_path);
		if (result == COM_SOURCE_INVALID)
		{
			elf_Scratch scratch = elf_begin_scratch_excluding(compiler->arena);
			dy_String path = dy_string_from_data(atom_data(canonical_path), atom_size(canonical_path));
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
					site.source = compiler_append_source(compiler, canonical_path, canonical_path, (SourceBuffer) {});
				}
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, site, "%s '%s'", error, atom_data(canonical_path));
			}
			else {
				result = compiler_register_source(compiler, canonical_path, canonical_path, source, site);
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
	Com_SourceId source_id = COM_SOURCE_INVALID;
	if (path)
	{
		Atom *requested_path = atom_from_data(compiler->atoms, path);
		if (requested_path) {
			source_id = compiler_load_source_file(compiler, 0, requested_path, (SourceSite) {});
		}
	}
	ASSERT(source_id == COM_SOURCE_INVALID || source_id == 0);
	return source_id;
}

static b32 compiler_resolve_import(Compiler *compiler, Com_SourceId importer, Ast import_ast)
{
	ASSERT(importer < compiler->source_count);
	ASSERT(import_ast && import_ast->kind == AST_IMPORT);
	b32 result = false;
	Atom *importer_path = compiler->sources[importer].canonical_path;
	Com_SourceId source_id = compiler_load_source_file(compiler, importer_path, import_ast->import.path, import_ast->site);
	if (source_id != COM_SOURCE_INVALID)
	{
		import_ast->import.source = source_id;
		result = true;
	}
	return result;
}

b32 compiler_parse_sources(Compiler *compiler)
{
	b32 result = true;
	for (Com_SourceId source_id = 0; result && source_id < compiler->source_count; ++source_id)
	{
		// TODO(RJ): the parser should take a scratch arena for scratch allocations, and arena
		// for results
		Parser *parser = parser_create(compiler, source_id);
		if (!parser) {
			return false;
		}
		Ast ast = elf_parse_file(parser);
		compiler->sources[source_id].ast = ast;
		if (ast_is_error(ast))
		{
			if (compiler->report.error_count == 0) {
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, (SourceSite) {.source = source_id}, "failed to parse source");
			}
			result = false;
		}
		else
		{
			Ast *imports = compiler->sources[source_id].imports;
			u32 import_count = compiler->sources[source_id].import_count;
			for (u32 i = 0; i < import_count; ++i)
			{
				if (!compiler_resolve_import(compiler, source_id, imports[i])) {
					result = false;
				}
			}
		}
	}
	return result;
}

Com_StaticSymbol *compiler_static_symbol_from_source(Compiler *compiler, Com_SourceId source, Atom *name)
{
	ASSERT(source < compiler->static_scope_count);
	Com_StaticScope *scope = compiler->static_scopes + source;
	for (u32 i = 0; i < scope->symbol_count; ++i)
	{
		Com_StaticSymbol *symbol = scope->symbols + i;
		if (symbol->name == name) {
			return symbol;
		}
	}
	return 0;
}

static u32 compiler_add_function(Compiler *compiler, Com_SourceId source, Ast ast, b32 is_entry, b32 is_static)
{
	if (compiler->function_count == compiler->function_capacity)
	{
		u32 capacity = compiler->function_capacity ? compiler->function_capacity * 2 : 16;
		Com_Function *functions = elf_arena_push_zero(compiler->arena, sizeof(*functions) * capacity);
		if (compiler->function_count) {
			copy_memory(functions, compiler->functions, sizeof(*functions) * compiler->function_count);
		}
		compiler->functions = functions;
		compiler->function_capacity = capacity;
	}

	u32 index = compiler->function_count++;
	compiler->functions[index] = (Com_Function) {
		.ast = ast,
		.source = source,
		.index = index,
		.is_entry = is_entry,
		.is_static = is_static,
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
		if (expr->import.source < compiler->static_scope_count)
		{
			out->kind = COM_STATIC_NAMESPACE;
			out->source = expr->import.source;
			result = COM_STATIC_LOOKUP_VALUE;
		}
		else
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "import has no resolved source");
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
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "expected static namespace member name");
				result = COM_STATIC_LOOKUP_ERROR;
			}
			else
			{
				Com_StaticSymbol *symbol = compiler_static_symbol_from_source(compiler, receiver.source, member->atom);
				if (!symbol)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, member->site, "'%s' is not exported by the imported source", atom_data(member->atom));
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
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not a static symbol", atom_data(expr->atom));
				}
				else {
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not supported in a static expression", ast_type_name(expr->kind));
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
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, expr->site, "'%s' is not supported in a static expression", ast_type_name(expr->kind));
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
		compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, symbol->site, "cyclic static definition for '%s'", atom_data(symbol->name));
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
	ASSERT(function->is_static);
	ASSERT(function->ast && function->ast->kind == AST_FUNCTION);

	Ast ast = function->ast;
	function->param_count = ast->function.nparams;
	if (function->param_count) {
		function->params = elf_arena_push_zero(compiler->arena, sizeof(*function->params) * function->param_count);
	}

	b32 result = true;
	for (u32 i = 0; i < function->param_count; ++i)
	{
		Ast param = ast->function.params[i];
		if (!param || ast_is_error(param) || param->kind != AST_FUNCTION_PARAM || !param->param.name || param->param.name->kind != AST_IDENT)
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "expected static function parameter");
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
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, param->param.expr->site, "default for parameter '%s' cannot be an imported namespace", atom_data(resolved->name));
				resolved->default_value.kind = COM_STATIC_INVALID;
				result = false;
			}
		}
	}
	return result;
}

b32 compiler_resolve_statics(Compiler *compiler, Com_UnitType unit_type)
{
	compiler->static_scope_count = compiler->source_count;
	compiler->static_scopes = elf_arena_push_zero(compiler->arena, sizeof(*compiler->static_scopes) * compiler->static_scope_count);
	compiler->functions = 0;
	compiler->function_count = 0;
	compiler->function_capacity = 0;
	if (unit_type == COM_UNIT_FUNCTION) {
		compiler_add_function(compiler, 0, compiler->sources[0].ast, true, false);
	}

	for (Com_SourceId source = 0; source < compiler->source_count; ++source)
	{
		Ast file = compiler->sources[source].ast;
		ASSERT(file && file->kind == AST_FILE);
		Ast body = file->file.body;
		ASSERT(body && body->kind == AST_BLOCK_STAT);
		Com_StaticScope *scope = compiler->static_scopes + source;
		if (body->block.nstats) {
			scope->symbols = elf_arena_push_zero(compiler->arena, sizeof(*scope->symbols) * body->block.nstats);
		}

		for (u32 i = 0; i < body->block.nstats; ++i)
		{
			Ast declaration = body->block.stats[i];
			if (declaration->kind != AST_STATIC_DECL)
			{
				if (unit_type == COM_UNIT_MODULE || source != 0) {
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, declaration->site, "static module cannot contain executable top-level statements");
				}
				continue;
			}

			Ast name = declaration->static_decl.name;
			if (!name || ast_is_error(name) || name->kind != AST_IDENT)
			{
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, declaration->site, "expected static declaration name");
				continue;
			}
			if (compiler_static_symbol_from_source(compiler, source, name->atom))
			{
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, name->site, "'%s': is already declared", atom_data(name->atom));
				continue;
			}

			Com_StaticSymbol *symbol = scope->symbols + scope->symbol_count++;
			symbol->name = name->atom;
			symbol->site = name->site;
			symbol->expr = declaration->static_decl.expr;
			symbol->source = source;
		}
	}

	for (Com_SourceId source = 0; source < compiler->static_scope_count; ++source)
	{
		Com_StaticScope *scope = compiler->static_scopes + source;
		for (u32 i = 0; i < scope->symbol_count; ++i)
		{
			Com_StaticSymbol *symbol = scope->symbols + i;
			if (symbol->expr->kind == AST_FUNCTION)
			{
				symbol->value.kind = COM_STATIC_FUNCTION;
				symbol->value.function = compiler_add_function(compiler, source, symbol->expr, false, true);
				symbol->state = COM_STATIC_RESOLVED;
			}
		}
	}
	for (Com_SourceId source = 0; source < compiler->static_scope_count; ++source)
	{
		Com_StaticScope *scope = compiler->static_scopes + source;
		for (u32 i = 0; i < scope->symbol_count; ++i) {
			compiler_evaluate_static_symbol(compiler, scope->symbols + i);
		}
	}
	for (u32 i = 0; i < compiler->function_count; ++i)
	{
		Com_Function *function = compiler->functions + i;
		if (function->is_static) {
			compiler_resolve_function_parameters(compiler, function);
		}
	}
	return compiler->report.error_count == 0;
}

void compiler_report_unreferenced_statics(Compiler *compiler, Com_SourceId source)
{
	ASSERT(source < compiler->static_scope_count);
	Com_StaticScope *scope = compiler->static_scopes + source;
	for (u32 i = 0; i < scope->symbol_count; ++i)
	{
		Com_StaticSymbol *symbol = scope->symbols + i;
		if (!symbol->referenced) {
			compiler_report(compiler, COMPILER_WARNING, COMPILER_PHASE_LOWERING, symbol->site, "unreferenced entity");
		}
	}
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

b32 compiler_reportv(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, va_list args)
{
	ASSERT(compiler);
	elf_StrSlice message = compiler_format_message(compiler->arena, format, args);
	if (!message.data) {
		return false;
	}

	Com_SourceId source_id = site.source < compiler->source_count ? site.source : 0;
	Com_Source *source = compiler->sources + source_id;
	const char *source_name = atom_data(source->name);
	elf_arena_align(compiler->arena, _Alignof(Com_Message));
	Com_Message *compiler_message = elf_arena_push_zero(compiler->arena, sizeof(*compiler_message));
	if (!compiler_message) {
		return false;
	}
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

	if (compiler->log_messages)
	{
		LogLevel level = severity == COMPILER_ERROR ? LOG_LEVEL_ERROR : severity == COMPILER_WARNING ? LOG_LEVEL_WARNING : LOG_LEVEL_INFO;
		const char *severity_name = severity == COMPILER_ERROR ? "error" : severity == COMPILER_WARNING ? "warning" : "note";
		if (site.line_index) {
			log_linef(level, "%s [%u:%llu] %s: %s", source_name, site.line_index, source_slice_column(site), severity_name, message.data);
		}
		else {
			log_linef(level, "%s [?] %s: %s", source_name, severity_name, message.data);
		}
		if (source_slice_is_valid(site)) print_source_slice_marker(site, source->source);
	}
	return true;
}

b32 compiler_report(Compiler *compiler, Com_Severity severity, Com_Phase phase, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 result = compiler_reportv(compiler, severity, phase, site, format, args);
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
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			ast = parse_constexpr(parser);
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
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			ast = parse_json_ast(parser);
		}
	}
	return ast;
}

Com_Module *compiler_compile(Compiler *compiler, elf_Arena *output_arena, Com_UnitType unit_type)
{
	ASSERT(compiler);
	ASSERT(output_arena);
	ASSERT(compiler->source_count);
	ASSERT(unit_type == COM_UNIT_FUNCTION || unit_type == COM_UNIT_MODULE);
	Com_Module *module = 0;
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
	if (!compiler_resolve_statics(compiler, unit_type)) {
		return module;
	}
	IrModuleEmitter *ir_module_emitter = ir_create_module_emitter(compiler);
	IrFunctionEmitter *ir_function_emitter = ir_create_function_emitter(ir_module_emitter);
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
	IrModule ir_module = ir_module_emitter->module;
	ASSERT(ir_module.functions || ir_module.function_count == 0);
	ASSERT(!ir_module.has_entry || ir_module.entry_index < ir_module.function_count);

	BcModuleEmitter *bc_module_emitter = bc_create_module_emitter(compiler, ir_module.function_count);
	BcFunctionEmitter *bc_function_emitter = bc_create_function_emitter(bc_module_emitter);
	PROF_BLOCK("compiler.emit")
	{
		for (u32 i = 0; i < ir_module.function_count; ++i) {
			bc_emit_function(bc_function_emitter, ir_module.functions + i);
		}
	}
	ASSERT(bc_module_emitter->generated_function_count == bc_module_emitter->function_count);
	if (compiler->report.error_count) {
		return module;
	}
	for (u32 i = 0; i < ir_module.global_count; ++i) {
		bc_add_global(bc_module_emitter, ir_module.globals[i]);
	}

	elf_arena_align(output_arena, 8);
	module = elf_arena_push_zero(output_arena, sizeof(*module));
	bc_finalize_module(compiler, output_arena, module, ir_module, bc_module_emitter);
	return module;
}
