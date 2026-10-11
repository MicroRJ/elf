//
// See Copyright Notice In elf.h
//

static StaticSymbolId static_module_find_symbol(StaticModule *module, Atom *name)
{
	StaticSymbolId result = NO_STATIC_SYMBOL;
	for (u32 i = 0; i < module->symbol_count; ++i)
	{
		if (atoms_equal(module->symbols[i].name, name))
		{
			result = i;
			break;
		}
	}
	return result;
}

typedef struct
{
	Compiler     *compiler;
	StaticModule *module;
}
StaticEvalContext;

static b32 static_module_evaluate_symbol(StaticEvalContext *ctx, StaticSymbolId symbol_id);

static b32 static_eval_expr(StaticEvalContext *ctx, Ast expr, StaticValue *out)
{
	b32 result = true;
	switch (expr->kind)
	{
		case AST_NIL_LITERAL:
		{
			out->kind = STATIC_VALUE_NIL;
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			out->kind = STATIC_VALUE_INTEGER;
			out->integer = expr->integer_value;
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			out->kind = STATIC_VALUE_NUMBER;
			out->number = expr->number_value;
		}
		break;

		case AST_STRING_LITERAL:
		{
			out->kind = STATIC_VALUE_STRING;
			out->string = expr->atom;
		}
		break;

		case AST_FUNCTION:
		{
			out->kind = STATIC_VALUE_FUNCTION;
			out->function = expr;
		}
		break;

		case AST_IDENT:
		{
			StaticSymbolId symbol_id = static_module_find_symbol(ctx->module, expr->atom);
			if (symbol_id == NO_STATIC_SYMBOL)
			{
				compiler_report(ctx->compiler, ELF_DIAGNOSTIC_ERROR, ELF_DIAGNOSTIC_PHASE_LOWERING, expr->site, "'%.*s' is not a static symbol", (int)atom_size(expr->atom), atom_data(expr->atom));
				result = false;
			}
			else if (static_module_evaluate_symbol(ctx, symbol_id))
			{
				*out = ctx->module->symbols[symbol_id].value;
			}
			else
			{
				result = false;
			}
		}
		break;

		default:
		{
			compiler_report(ctx->compiler, ELF_DIAGNOSTIC_ERROR, ELF_DIAGNOSTIC_PHASE_LOWERING, expr->site, "'%s' is not supported in a static expression", ast_type_name(expr->kind));
			result = false;
		}
		break;
	}
	return result;
}

static b32 static_module_evaluate_symbol(StaticEvalContext *ctx, StaticSymbolId symbol_id)
{
	ASSERT(symbol_id < ctx->module->symbol_count);
	StaticSymbol *symbol = ctx->module->symbols + symbol_id;
	b32 result = symbol->value.kind != STATIC_VALUE_INVALID;
	if (symbol->state == STATIC_SYMBOL_RESOLVING)
	{
		compiler_report(ctx->compiler, ELF_DIAGNOSTIC_ERROR, ELF_DIAGNOSTIC_PHASE_LOWERING, symbol->site, "cyclic static definition for '%.*s'", (int)atom_size(symbol->name), atom_data(symbol->name));
		result = false;
	}
	else if (symbol->state == STATIC_SYMBOL_UNRESOLVED)
	{
		symbol->state = STATIC_SYMBOL_RESOLVING;
		result = static_eval_expr(ctx, symbol->declaration->static_decl.expr, &symbol->value);
		symbol->state = STATIC_SYMBOL_RESOLVED;
	}
	return result;
}

static void static_module_collect_declarations(Compiler *compiler, StaticModule *module, AstArray declarations)
{
	module->symbols = elf_arena_push_zero(compiler->arena, sizeof(*module->symbols) * declarations.nargs);
	for (u32 i = 0; i < declarations.nargs; ++i)
	{
		Ast declaration = declarations.args[i];
		ASSERT(declaration->kind == AST_STATIC_DECL);
		Ast name_ast = declaration->static_decl.name;
		ASSERT(name_ast && name_ast->kind == AST_IDENT);
		Atom *name = name_ast->atom;
		StaticSymbolId existing = static_module_find_symbol(module, name);
		if (existing != NO_STATIC_SYMBOL)
		{
			compiler_report(compiler, ELF_DIAGNOSTIC_ERROR, ELF_DIAGNOSTIC_PHASE_LOWERING, name_ast->site, "'%.*s' is already declared as a static symbol", (int)atom_size(name), atom_data(name));
		}
		else
		{
			StaticSymbol *symbol = module->symbols + module->symbol_count++;
			symbol->name = name;
			symbol->declaration = declaration;
			symbol->site = name_ast->site;
		}
	}
}

static void static_module_evaluate(Compiler *compiler, StaticModule *module)
{
	StaticEvalContext ctx = { .compiler = compiler, .module = module };
	for (StaticSymbolId i = 0; i < module->symbol_count; ++i) {
		static_module_evaluate_symbol(&ctx, i);
	}
}

static StaticModule *static_module_build(Compiler *compiler, Ast file)
{
	ASSERT(file && file->kind == AST_FILE);
	ASSERT(file->file.body && file->file.body->kind == AST_BLOCK_STAT);

	StaticModule *module = elf_arena_push_zero(compiler->arena, sizeof(*module));
	module->ast = file;
	static_module_collect_declarations(compiler, module, file->file.static_decls);
	if (!compiler->error_count) {
		static_module_evaluate(compiler, module);
	}
	return module;
}
