//
// See Copyright Notice In elf.h
//

#define NO_STATIC_SYMBOL 0xffffffffu

typedef u32 StaticSymbolId;

typedef struct
{
	Atom       *name;
	Ast         declaration;
	SourceSite  site;
}
StaticSymbol;

struct StaticModule
{
	Ast           ast;
	StaticSymbol *symbols;
	u32           symbol_count;
};

static StaticModule *static_module_collect(Compiler *compiler, Ast file);
static StaticSymbolId static_module_find_symbol(StaticModule *module, Atom *name);
