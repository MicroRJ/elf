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
#include "elf_core.h"
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

void compiler_set_module_loader(Compiler *compiler, Com_ModuleLoader *loader, void *user_data)
{
	compiler->module_loader = loader;
	compiler->module_loader_data = user_data;
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

static Com_SourceId compiler_register_source(Compiler *compiler, Com_ModuleLoadParams *params, SourceSite site)
{
	ASSERT(params->canonical_path);
	Com_SourceId source_id = compiler_find_source(compiler, params->canonical_path);
	if (source_id != COM_SOURCE_INVALID) {
		return source_id;
	}

	b32 source_is_oversized = params->source.size > UINT32_MAX;
	b32 source_is_invalid = params->source.size && !params->source.data;
	b32 source_has_nul = !source_is_oversized && !source_is_invalid && params->source.size && memchr(params->source.data, 0, params->source.size);
	if (source_is_oversized || source_is_invalid || source_has_nul)
	{
		if (compiler->source_count == 0)
		{
			Atom *name = params->name ? params->name : params->canonical_path;
			source_id = compiler_append_source(compiler, name, params->canonical_path, (SourceBuffer) {.data = params->source.data});
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

	Atom *name = params->name ? params->name : params->canonical_path;
	SourceBuffer source = compiler_copy_source(compiler, params->source);
	return compiler_append_source(compiler, name, params->canonical_path, source);
}

Com_SourceId compiler_add_root_source(Compiler *compiler, const char *name, elf_StrSlice source)
{
	ASSERT(compiler->source_count == 0);
	Atom *source_name = atom_from_data(compiler->atoms, name ? name : "<unknown>");
	Com_ModuleLoadParams params = {
		.compiler = compiler,
		.importer = COM_SOURCE_INVALID,
		.path = source_name,
		.name = source_name,
		.canonical_path = source_name,
		.source = source,
	};
	Com_SourceId source_id = compiler_register_source(compiler, &params, (SourceSite) {});
	ASSERT(source_id == COM_SOURCE_INVALID || source_id == 0);
	return source_id;
}

static b32 compiler_resolve_import(Compiler *compiler, Com_SourceId importer, Ast import_ast)
{
	ASSERT(importer < compiler->source_count);
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
			Com_SourceId source_id = compiler_register_source(compiler, &params, import_ast->site);
			if (source_id != COM_SOURCE_INVALID)
			{
				import_ast->import.source = source_id;
				result = true;
			}
		}
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

static b32 compiler_eval_constexpr_ast(Compiler *compiler, elf_State *state, Ast ast, elf_Value *out)
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
			return compiler_eval_constexpr_ast(compiler, state, ast->tuple.args[0], out);
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
			*out = value_from_string(elf_string_from_atom(state, ast->atom));
			return true;
		}

		case AST_TABLE:
		{
			elf_Table *table = elf_table_new(state);
			for (u32 i = 0; i < ast->table.nargs; ++ i)
			{
				Ast entry_ast = ast->table.args[i];
				if (!entry_ast || entry_ast->kind != AST_TABLE_ENTRY)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, ast->site, "invalid table constant");
					return false;
				}

				elf_Value value = {};
				if (!compiler_eval_constexpr_ast(compiler, state, entry_ast->table_entry.value, &value))
				{
					return false;
				}

				if (!entry_ast->table_entry.key)
				{
					compiler_report(compiler, COMPILER_ERROR, COMPILER_PHASE_EVALUATION, entry_ast->site, "invalid unkeyed table constant");
					return false;
				}
				elf_Value key = {};
				if (!compiler_eval_constexpr_ast(compiler, state, entry_ast->table_entry.key, &key))
				{
					return false;
				}
				elf_table_set(state, table, key, value);
			}

			*out = value_from_table(table);
			return true;
		}

		case AST_ARRAY:
		{
			elf_Array *array = elf_array_new(state);
			for (u32 i = 0; i < ast->array.nargs; ++i)
			{
				elf_Value value = {};
				if (!compiler_eval_constexpr_ast(compiler, state, ast->array.args[i], &value))
				{
					return false;
				}
				elf_array_add(state, array, value);
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

static b32 compiler_push_constexpr_ast(Compiler *compiler, elf_State *state, Ast ast)
{
	if (ast_is_error(ast)) {
		return false;
	}

	elf_Value value = {};
	if (!compiler_eval_constexpr_ast(compiler, state, ast, &value)) {
		return false;
	}
	push_value(state, value);
	return true;
}

static void restore_source_stack(elf_State *state, elf_Value *checkpoint)
{
	ASSERT(checkpoint >= state->frame->framebase && checkpoint <= state->stack_ptr);
	value_zero_many(checkpoint, state->stack_ptr - checkpoint);
	state->stack_ptr = checkpoint;
}

int compiler_push_json(Compiler *compiler, elf_State *state)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count);
	elf_Value *stack_checkpoint = state->stack_ptr;
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	int result = 0;
	if (!compiler->report.error_count) {
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			result = compiler_push_constexpr_ast(compiler, state, parse_json_ast(parser));
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
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, name, source);
	int result = compiler_push_json(compiler, state);
	elf_end_scratch(scratch);
	return result;
}

int compiler_push_constant_expr(Compiler *compiler, elf_State *state)
{
	ASSERT(compiler);
	ASSERT(compiler->source_count);
	elf_Value *stack_checkpoint = state->stack_ptr;
	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;

	int result = 0;
	if (!compiler->report.error_count) {
		Parser *parser = parser_create(compiler, 0);
		if (parser) {
			result = compiler_push_constexpr_ast(compiler, state, parse_constexpr(parser));
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
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, name, source);
	int result = compiler_push_constant_expr(compiler, state);
	elf_end_scratch(scratch);
	return result;
}

static char *copy_runtime_source(elf_State *state, SourceBuffer source)
{
	u64 storage_size = ((u64)source.size + 1 + 7) & ~(u64)7;
	char *data = elf_arena_push_zero(&state->arena, storage_size);
	copy_memory(data, source.data, source.size);
	return data;
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
	Ast ast_file = compiler->sources[0].ast;

	LowerContext *ctx = elf_create_lower_context(compiler);
	IrModule ir_module;
	PROF_BLOCK("compiler.lower")
	{
		if (unit_type == COM_UNIT_FUNCTION) {
			ir_module = elf_lower_ast_function(ctx, ast_file);
		}
		else {
			ir_module = elf_lower_ast_module(ctx, ast_file);
		}
	}
	if (compiler->report.error_count) {
		return module;
	}

	elf_arena_align(output_arena, 8);
	module = elf_arena_push_zero(output_arena, sizeof(*module));
	if (!generate_module(compiler, output_arena, module, ir_module) || compiler->report.error_count) {
		module = 0;
	}
	return module;
}

static b32 compiled_module_is_valid(const Com_Module *compiled_module)
{
	if (!compiled_module || !compiled_module->source_count || compiled_module->root_source >= compiled_module->source_count) {
		return false;
	}
	if (compiled_module->has_entry && compiled_module->entry_index >= compiled_module->function_count) {
		return false;
	}
	for (u32 i = 0; i < compiled_module->global_count; ++i)
	{
		if (compiled_module->globals[i].name >= compiled_module->string_count) {
			return false;
		}
	}
	for (u32 i = 0; i < compiled_module->global_relocation_count; ++i)
	{
		Com_GlobalRelocation relocation = compiled_module->global_relocations[i];
		if (relocation.bytecode_index >= compiled_module->bytecode_count || relocation.global_index >= compiled_module->global_count) {
			return false;
		}
		BytecodeType type = BC_TYPE(compiled_module->bytecode[relocation.bytecode_index]);
		if (type != BC_GETGLOBAL && type != BC_SETGLOBAL) {
			return false;
		}
	}
	return true;
}

elf_Module *elf_link_module(elf_State *state, const Com_Module *compiled_module)
{
	ASSERT(state);
	if (!compiled_module_is_valid(compiled_module)) {
		return 0;
	}

	elf_Scratch scratch = elf_begin_scratch();
	u32 *global_slots = elf_arena_push_zero(scratch.arena, sizeof(*global_slots) * compiled_module->global_count);
	u32 next_new_slot = state->globals.count;
	for (u32 i = 0; i < compiled_module->global_count; ++i)
	{
		Com_String name = compiled_module->strings[compiled_module->globals[i].name];
		elf_StrSlice slice = {(char *)name.data, name.size};
		u32 slot = 0;
		if (!elf_globals_find(&state->globals, slice, &slot)) {
			slot = next_new_slot++;
		}
		if (slot > 32767u)
		{
			elf_end_scratch(scratch);
			return 0;
		}
		global_slots[i] = slot;
	}

	u32 saved_gc_mode = state->gc_mode;
	state->gc_mode = ELF_GC_PAUSED;
	for (u32 i = 0; i < compiled_module->global_count; ++i)
	{
		Com_String name = compiled_module->strings[compiled_module->globals[i].name];
		global_slots[i] = elf_globals_ensure(&state->globals, (elf_StrSlice) {(char *)name.data, name.size});
	}

	elf_arena_align(&state->arena, 8);
	elf_Module *module = elf_arena_push_zero(&state->arena, sizeof(*module));
	Com_ModuleSource root_source = compiled_module->sources[compiled_module->root_source];
	module->source_name = elf_string_from_data_size(state, root_source.name.data, root_source.name.size);
	module->source_data = copy_runtime_source(state, root_source.source);
	module->source_size = root_source.source.size;

	module->string_count = compiled_module->string_count;
	if (module->string_count)
	{
		elf_arena_align(&state->arena, 8);
		module->strings = elf_arena_push_zero(&state->arena, sizeof(*module->strings) * module->string_count);
		for (u32 i = 0; i < module->string_count; ++i)
		{
			Com_String string = compiled_module->strings[i];
			module->strings[i] = elf_string_from_data_size(state, string.data, string.size);
		}
	}

	module->integer_constant_count = compiled_module->integer_constant_count;
	if (module->integer_constant_count)
	{
		elf_arena_align(&state->arena, 8);
		module->integer_constants = elf_arena_push_copy(&state->arena, sizeof(*module->integer_constants) * module->integer_constant_count, compiled_module->integer_constants);
	}
	module->number_constant_count = compiled_module->number_constant_count;
	if (module->number_constant_count)
	{
		elf_arena_align(&state->arena, 8);
		module->number_constants = elf_arena_push_copy(&state->arena, sizeof(*module->number_constants) * module->number_constant_count, compiled_module->number_constants);
	}
	module->bytecode_count = compiled_module->bytecode_count;
	if (module->bytecode_count)
	{
		elf_arena_align(&state->arena, 8);
		module->bytecode = elf_arena_push_copy(&state->arena, sizeof(*module->bytecode) * module->bytecode_count, compiled_module->bytecode);
	}
	module->bytecode_function_count = compiled_module->function_count;
	if (module->bytecode_function_count)
	{
		elf_arena_align(&state->arena, 8);
		module->bytecode_functions = elf_arena_push_copy(&state->arena, sizeof(*module->bytecode_functions) * module->bytecode_function_count, compiled_module->functions);
	}
	for (u32 i = 0; i < module->bytecode_function_count; ++i)
	{
		BcFunction *function = module->bytecode_functions + i;
		if (function->source_map_count)
		{
			elf_arena_align(&state->arena, 8);
			function->source_map = elf_arena_push_copy(&state->arena, sizeof(*function->source_map) * function->source_map_count, function->source_map);
		}
	}
	for (u32 i = 0; i < compiled_module->global_relocation_count; ++i)
	{
		Com_GlobalRelocation relocation = compiled_module->global_relocations[i];
		Bytecode *bytecode = module->bytecode + relocation.bytecode_index;
		i16 slot = (i16)global_slots[relocation.global_index];
		if (BC_TYPE(*bytecode) == BC_GETGLOBAL) {
			bytecode->b_y = slot;
		}
		else {
			bytecode->b_x = slot;
		}
	}

	module->next = state->modules;
	state->modules = module;
	state->gc_mode = saved_gc_mode;
	elf_end_scratch(scratch);
	return module;
}

BcFunctionRef elf_compile_source(elf_State *state, char const *name, elf_StrSlice source)
{
	ASSERT(name);
	ASSERT(source.data);
	elf_Scratch scratch = elf_begin_scratch();
	Compiler *compiler = compiler_create(scratch.arena);
	compiler_add_root_source(compiler, name, source);
	Com_Module *compiled_module = compiler_compile(compiler, scratch.arena, COM_UNIT_FUNCTION);
	elf_Module *module = elf_link_module(state, compiled_module);
	BcFunctionRef entry = {};
	if (module && compiled_module->has_entry)
	{
		entry.module = module;
		entry.index = compiled_module->entry_index;
	}
	elf_end_scratch(scratch);
	return entry;
}
