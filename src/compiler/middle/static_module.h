//
// See Copyright Notice In elf.h
//

#define NO_STATIC_SYMBOL 0xffffffffu

typedef u32 StaticSymbolId;

typedef enum
{
	STATIC_SYMBOL_UNRESOLVED,
	STATIC_SYMBOL_RESOLVING,
	STATIC_SYMBOL_RESOLVED,
}
StaticSymbolState;

typedef enum
{
	STATIC_VALUE_INVALID,
	STATIC_VALUE_NIL,
	STATIC_VALUE_INTEGER,
	STATIC_VALUE_NUMBER,
	STATIC_VALUE_STRING,
	STATIC_VALUE_FUNCTION,
}
StaticValueKind;

typedef struct
{
	StaticValueKind kind;
	union
	{
		i64   integer;
		f64   number;
		Atom *string;
		Ast   function;
	};
}
StaticValue;

typedef struct
{
	Atom              *name;
	Ast                declaration;
	SourceSite         site;
	StaticSymbolState  state;
	StaticValue        value;
}
StaticSymbol;

struct StaticModule
{
	Ast           ast;
	StaticSymbol *symbols;
	u32           symbol_count;
};

static StaticModule *static_module_build(Compiler *compiler, Ast file);
static StaticSymbolId static_module_find_symbol(StaticModule *module, Atom *name);
