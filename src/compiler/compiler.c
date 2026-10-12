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
#include "parser.c"
#include "parse_json.c"

#include "ir.c"
#include "lower.c"
#include "bytecode_gen.c"

static Com_ModuleId compiler_append_module(Compiler *compiler, Atom *name, Atom *canonical_path, SourceBuffer source)
{
	if (compiler->module_count == compiler->module_capacity)
	{
		u32 capacity = compiler->module_capacity ? compiler->module_capacity * 2 : 4;
		Com_Module *modules = elf_arena_push_zero(compiler->arena, sizeof(*modules) * capacity);
		if (compiler->module_count) {
			copy_memory(modules, compiler->modules, sizeof(*modules) * compiler->module_count);
		}
		compiler->modules = modules;
		compiler->module_capacity = capacity;
	}

	Com_ModuleId module_id = compiler->module_count++;
	Com_Module *module = compiler->modules + module_id;
	module->name = name;
	module->canonical_path = canonical_path;
	module->source = source;
	return module_id;
}

static void init_atoms(Compiler *compiler)
{
#define INTERN_KEYWORD_ATOM(NAME, TEXT) atom_from_data_id(compiler->atoms, TEXT, XFUSE(TOK_, NAME));
	KEYWORD_DEFINITIONS(INTERN_KEYWORD_ATOM)
#undef INTERN_KEYWORD_ATOM

#define INTERN_MACRO_ATOM(NAME, TEXT) atom_from_data_id(compiler->atoms, "#" TEXT, XFUSE(TOK_, NAME));
	MACRO_DEFINITIONS(INTERN_MACRO_ATOM)
#undef INTERN_MACRO_ATOM
}

Compiler *compiler_create(elf_State *state, elf_Arena *arena)
{
	Compiler *compiler = elf_arena_push_zero(arena, sizeof(*compiler));
	compiler->state = state;
	compiler->arena = arena;
	compiler->atoms = elf_arena_push_zero(arena, sizeof(*compiler->atoms));
	atom_table_init(compiler->atoms, arena);
	init_atoms(compiler);
	compiler->log_messages = true;
	return compiler;
}

void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data)
{
	compiler->module_loader = loader;
	compiler->module_loader_data = user_data;
}

void compiler_set_log_messages(Compiler *compiler, b32 enabled)
{
	compiler->log_messages = enabled;
}

b32 compiler_record_import(Compiler *compiler, Com_ModuleId module_id, Ast import_ast)
{
	ASSERT(module_id < compiler->module_count);
	ASSERT(import_ast && import_ast->kind == AST_IMPORT);
	Com_Module *module = compiler->modules + module_id;
	if (module->import_count == module->import_capacity)
	{
		u32 capacity = module->import_capacity ? module->import_capacity * 2 : 4;
		Ast *imports = elf_arena_push_zero(compiler->arena, sizeof(*imports) * capacity);
		if (!imports) {
			return false;
		}
		if (module->import_count) {
			copy_memory(imports, module->imports, sizeof(*imports) * module->import_count);
		}
		module->imports = imports;
		module->import_capacity = capacity;
	}
	module->imports[module->import_count++] = import_ast;
	return true;
}

static Com_ModuleId compiler_find_module(Compiler *compiler, Atom *canonical_path)
{
	Com_ModuleId result = COM_MODULE_INVALID;
	for (Com_ModuleId module_id = 0; module_id < compiler->module_count; ++module_id)
	{
		if (compiler->modules[module_id].canonical_path == canonical_path) {
			result = module_id;
			break;
		}
	}
	return result;
}

static SourceBuffer compiler_copy_module_source(Compiler *compiler, elf_StrSlice source)
{
	ASSERT(source.size <= 0xffffffffu);
	char *data = elf_arena_push(compiler->arena, source.size + 1);
	if (source.size) {
		copy_memory(data, source.data, source.size);
	}
	data[source.size] = 0;
	return (SourceBuffer) {.data = data, .size = (u32)source.size};
}

static Com_ModuleId compiler_register_module(Compiler *compiler, Com_ModuleLoadParams *params, SourceSite site)
{
	ASSERT(params->canonical_path);
	Com_ModuleId module_id = compiler_find_module(compiler, params->canonical_path);
	if (module_id != COM_MODULE_INVALID) {
		return module_id;
	}

	b32 source_is_oversized = params->source.size > UINT32_MAX;
	b32 source_is_invalid = params->source.size && !params->source.data;
	b32 source_has_nul = !source_is_oversized && !source_is_invalid && params->source.size && memchr(params->source.data, 0, params->source.size);
	if (source_is_oversized || source_is_invalid || source_has_nul)
	{
		if (compiler->module_count == 0)
		{
			Atom *name = params->name ? params->name : params->canonical_path;
			module_id = compiler_append_module(compiler, name, params->canonical_path, (SourceBuffer) {.data = params->source.data});
			site.module = module_id;
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
		return COM_MODULE_INVALID;
	}

	Atom *name = params->name ? params->name : params->canonical_path;
	SourceBuffer source = compiler_copy_module_source(compiler, params->source);
	return compiler_append_module(compiler, name, params->canonical_path, source);
}

Com_ModuleId compiler_add_root_module(Compiler *compiler, const char *name, elf_StrSlice source)
{
	ASSERT(compiler->module_count == 0);
	Atom *module_name = atom_from_data(compiler->atoms, name ? name : "<unknown>");
	Com_ModuleLoadParams params = {
		.compiler = compiler,
		.importer = COM_MODULE_INVALID,
		.path = module_name,
		.name = module_name,
		.canonical_path = module_name,
		.source = source,
	};
	Com_ModuleId module_id = compiler_register_module(compiler, &params, (SourceSite) {});
	ASSERT(module_id == COM_MODULE_INVALID || module_id == 0);
	return module_id;
}

static b32 compiler_resolve_import(Compiler *compiler, Com_ModuleId importer, Ast import_ast)
{
	ASSERT(importer < compiler->module_count);
	ASSERT(import_ast && import_ast->kind == AST_IMPORT);
	b32 result = false;
	if (!compiler->module_loader)
	{
		compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, import_ast->site, "cannot import '%s': no module loader is configured", atom_data(import_ast->import.path));
	}
	else
	{
		Com_ModuleLoadParams params = {
			.compiler = compiler,
			.importer = importer,
			.path = import_ast->import.path,
			.user_data = compiler->module_loader_data,
		};
		if (!compiler->module_loader(&params))
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, import_ast->site, "failed to load module '%s'", atom_data(import_ast->import.path));
		}
		else if (!params.canonical_path)
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, import_ast->site, "module loader returned no canonical path for '%s'", atom_data(import_ast->import.path));
		}
		else
		{
			Com_ModuleId module_id = compiler_register_module(compiler, &params, import_ast->site);
			if (module_id != COM_MODULE_INVALID)
			{
				import_ast->import.module = module_id;
				result = true;
			}
		}
	}
	return result;
}

b32 compiler_parse_modules(Compiler *compiler)
{
	b32 result = true;
	for (Com_ModuleId module_id = 0; result && module_id < compiler->module_count; ++module_id)
	{
		// TODO(RJ): the parser should take a scratch arena for scratch allocations, and arena
		// for results
		Parser *parser = parser_create(compiler, module_id);
		if (!parser) {
			return false;
		}
		Ast ast = elf_parse_file(parser);
		compiler->modules[module_id].ast = ast;
		if (ast_is_error(ast))
		{
			if (compiler->report.error_count == 0) {
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_PARSER, (SourceSite) {.module = module_id}, "failed to parse source");
			}
			result = false;
		}
		else
		{
			Ast *imports = compiler->modules[module_id].imports;
			u32 import_count = compiler->modules[module_id].import_count;
			for (u32 i = 0; i < import_count; ++i)
			{
				if (!compiler_resolve_import(compiler, module_id, imports[i])) {
					result = false;
				}
			}
		}
	}
	return result;
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

	Com_ModuleId module_id = site.module < compiler->module_count ? site.module : 0;
	Com_Module *source_module = compiler->modules + module_id;
	const char *source_name = atom_data(source_module->name);
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
		if (source_slice_is_valid(site)) print_source_slice_marker(site, source_module->source);
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

static b32 compiler_eval_constexpr_ast(Compiler *compiler, Ast ast, elf_Value *out)
{
	if (!ast)
	{
		*out = value_nil();
		return true;
	}

	switch (ast->kind)
	{
		case AST_TUPLE:
		{
			if (ast->tuple.nargs != 1)
			{
				compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "constant expression must produce one value");
				return false;
			}
			return compiler_eval_constexpr_ast(compiler, ast->tuple.args[0], out);
		}

		case AST_NIL_LITERAL:
		{
			*out = value_nil();
			return true;
		}

		case AST_INTEGER_LITERAL:
		{
			*out = value_from_integer(ast->integer_value);
			return true;
		}

		case AST_NUMBER_LITERAL:
		{
			*out = value_from_number(ast->number_value);
			return true;
		}

		case AST_STRING_LITERAL:
		{
			*out = value_from_string(elf_string_from_atom(compiler->state, ast->atom));
			return true;
		}

		case AST_TABLE:
		{
			elf_Table *table = elf_new_table_rogue(compiler->state);
			table->json_shape = ast->table.json_shape;
			for (u32 i = 0; i < ast->table.nargs; ++ i)
			{
				Ast entry_ast = ast->table.args[i];
				if (!entry_ast || entry_ast->kind != AST_TABLE_ENTRY)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "invalid table constant");
					return false;
				}

				elf_Value value = {};
				if (!compiler_eval_constexpr_ast(compiler, entry_ast->table_entry.value, &value))
				{
					return false;
				}

				if (entry_ast->table_entry.key)
				{
					elf_Value key = {};
					if (!compiler_eval_constexpr_ast(compiler, entry_ast->table_entry.key, &key))
					{
						return false;
					}
					elf_table_set(compiler->state, table, key, value);
				}
				else
				{
					elf_deprecated_array_add(compiler->state, table, value);
				}
			}

			*out = value_from_table(table);
			return true;
		}

		case AST_ARRAY:
		{
			elf_Array *array = elf_array_new(compiler->state);
			for (u32 i = 0; i < ast->array.nargs; ++i)
			{
				elf_Value value = {};
				if (!compiler_eval_constexpr_ast(compiler, ast->array.args[i], &value))
				{
					return false;
				}
				elf_array_add(compiler->state, array, value);
			}
			*out = value_from_array(array);
			return true;
		}

		default:
		{
			compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "'%s' is not a constant expression", ast_type_name(ast->kind));
			return false;
		}
	}
}

static b32 compiler_push_constexpr_ast(Compiler *compiler, Ast ast)
{
	if (ast_is_error(ast)) {
		return false;
	}

	elf_Value value = {};
	if (!compiler_eval_constexpr_ast(compiler, ast, &value)) {
		return false;
	}
	push_value(compiler->state, value);
	return true;
}

static void restore_source_stack(elf_State *state, elf_Value *checkpoint)
{
	ASSERT(checkpoint >= state->frame->framebase && checkpoint <= state->stack_ptr);
	value_zero_many(checkpoint, state->stack_ptr - checkpoint);
	state->stack_ptr = checkpoint;
}

int compiler_push_json(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->module_count);
	elf_State *state = compiler->state;
	elf_Value *stack_checkpoint = state->stack_ptr;
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	int result = 0;
	if (!compiler->report.error_count) {
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			result = compiler_push_constexpr_ast(compiler, parse_json_ast(parser));
		}
	}
	if (!result) restore_source_stack(state, stack_checkpoint);
	else ASSERT(state->stack_ptr == stack_checkpoint + 1);

	state->gc_mode = saved_gc_mode;
	return result;
}

int elf_push_json_source(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena);
	compiler_add_root_module(compiler, name, source);
	int result = compiler_push_json(compiler);
	elf_end_scratch(scratch);
	return result;
}

int compiler_push_constant_expr(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->module_count);
	elf_State *state = compiler->state;
	elf_Value *stack_checkpoint = state->stack_ptr;
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	int result = 0;
	if (!compiler->report.error_count) {
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			result = compiler_push_constexpr_ast(compiler, parse_constexpr(parser));
		}
	}
	if (!result) restore_source_stack(state, stack_checkpoint);
	else ASSERT(state->stack_ptr == stack_checkpoint + 1);

	state->gc_mode = saved_gc_mode;
	return result;
}

int elf_push_constant_expr_source(elf_State *state, const char *name, elf_StrSlice source)
{
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena);
	compiler_add_root_module(compiler, name, source);
	int result = compiler_push_constant_expr(compiler);
	elf_end_scratch(scratch);
	return result;
}

static char *persist_compiled_source(elf_State *state, SourceBuffer source)
{
	u64 storage_size = ((u64)source.size + 1 + 7) & ~(u64)7;
	char *data = elf_arena_push_zero(&state->arena, storage_size);
	copy_memory(data, source.data, source.size);
	return data;
}

BcFunctionRef compiler_compile(Compiler *compiler)
{
	ASSERT(compiler);
	ASSERT(compiler->module_count);
	elf_State *state = compiler->state;
	u64 arena_checkpoint = state->arena.used;
	BcFunctionRef file_entry = {};
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;
	if (compiler->report.error_count) goto done;

	b32 parsed = false;
	PROF_BLOCK("compiler.parse")
	{
		parsed = compiler_parse_modules(compiler);
	}
	if (!parsed || compiler->report.error_count)
	{
		state->arena.used = arena_checkpoint;
		goto done;
	}
	Ast ast_file = compiler->modules[0].ast;

	LowerContext *ctx = elf_create_lower_context(compiler);
	IrModule ir_module;
	PROF_BLOCK("compiler.lower")
	{
		ir_module = elf_lower_ast_file(ctx, ast_file);
	}
	if (compiler->report.error_count) {
		state->arena.used = arena_checkpoint;
		goto done;
	}

	elf_Module generated_module = {};
	file_entry = generate_module(compiler, &generated_module, ir_module);
	if (!bc_function_ref_is_valid(file_entry) || compiler->report.error_count) {
		file_entry = (BcFunctionRef) {};
		state->arena.used = arena_checkpoint;
		goto done;
	}

	elf_arena_align(&state->arena, 8);
	elf_Module *module = elf_arena_push_zero(&state->arena, sizeof(*module));
	*module = generated_module;
	Com_Module *root_module = compiler->modules;
	module->source_name = elf_string_from_atom(state, root_module->name);
	module->source_data = persist_compiled_source(state, root_module->source);
	module->source_size = root_module->source.size;
	module->next = state->modules;
	state->modules = module;
	file_entry.module = module;

done:
	state->gc_mode = saved_gc_mode;
	return file_entry;
}

BcFunctionRef elf_compile_source(elf_State *state, char const *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(state, scratch.arena);
	compiler_add_root_module(compiler, name, source);
	BcFunctionRef file_entry = compiler_compile(compiler);
	elf_end_scratch(scratch);
	return file_entry;
}
