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

static StaticModule *static_module_collect(Compiler *compiler, Ast file)
{
	ASSERT(file && file->kind == AST_FILE);
	ASSERT(file->file.body && file->file.body->kind == AST_BLOCK_STAT);

	StaticModule *module = elf_arena_push_zero(compiler->arena, sizeof(*module));
	module->ast = file;

	AstArray static_decls = file->file.static_decls;
	module->symbols = elf_arena_push_zero(compiler->arena, sizeof(*module->symbols) * static_decls.nargs);
	for (u32 i = 0; i < static_decls.nargs; ++i)
	{
		Ast declaration = static_decls.args[i];
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

	return module;
}
