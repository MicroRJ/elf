//
// See Copyright Notice In elf.h
//


// Todo, remove?
static Tree get_tree(Parser *par, TreeId id) { return *id; }
static int get_tree_kind(Parser *par, TreeId id) { return id->kind; }
static int get_tree_type(Parser *par, TreeId id) { return id->type; }
static Source tree_get_line(Parser *par, TreeId id) { return id->line; }

// Todo, put this somewhere proper!
static char *tree2s[] =
{
#define TREE(NAME) #NAME,
	TREEDEF(TREE)
#undef TREE
};

static void tree_chain_prepend(TreeChain *v, Tree *r)
{
	ASSERT(!r->next);

	if (!v->head)
	{
		v->head = v->tail = r;
	}
	else
	{
		r->next = v->head;
		v->head = r;
	}

	++ v->tally;
}

static void tree_chain_add(TreeChain *v, Tree *r)
{
	ASSERT(!r->next);

	if (!v->head)
	{
		v->head = v->tail = r;
	}
	else
	{
		v->tail = v->tail->next = r;
	}

	++ v->tally;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static TreeId tree_get_next(TreeId tr)
{
	return tr->next;
}

static TreeId tree_new(Parser *par, Source line, TreeKind kind, DataType type)
{
	// todo: proper arena
	TreeId r = & par->tree_memory[par->tree_index ++];
	r->line = line;
	r->kind = kind;
	r->type = type;
	return r;
}

static TreeId tree_poly(Parser *par, Source line, TreeKind kind, DataType type, u32 n, TreeId x, TreeId y)
{
	TreeId r = tree_new(par,line,kind,type);
	r->n = n, r->x = x, r->y = y;
	return r;
}

static TreeId tree_nullary(Parser *par, Source line, TreeKind kind, DataType type)
{
	TreeId r = tree_new(par,line,kind,type);
	r->n = 0;
	return r;
}

static TreeId tree_unary(Parser *par, Source line, TreeKind kind, DataType type, TreeId x)
{
	TreeId r = tree_new(par,line,kind,type);
	r->x = x;
	r->n = 1;
	return r;
}

static TreeId tree_binary(Parser *par, Source line, TreeKind kind, DataType type, TreeId x, TreeId y)
{
	TreeId r = tree_new(par,line,kind,type);
	r->x = x, r->y = y;
	r->n = 2;
	return r;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static TreeId tree_nop(Parser *par, Source line)
{
	return tree_nullary(par, line, TREE_NOP, NT_ANY);
}

static TreeId tree_nil(Parser *par, Source line)
{
	return tree_nullary(par, line, EXPR_NIL, NT_NIL);
}

static TreeId tree_int(Parser *par, Source line, i64 i)
{
	TreeId v;
	v = tree_nullary(par, line, EXPR_INT, NT_INT);
	v->expr_int = i;
	return v;
}

static TreeId tree_num(Parser *par, Source line, f64 n)
{
	TreeId v;
	v=tree_nullary(par, line, EXPR_NUM, NT_NUM);
	v->expr_num=n;
	return v;
}

static TreeId tree_str(Parser *par, Source line, char *s)
{
	TreeId v = tree_nullary(par, line, EXPR_STR, NT_STR);
	v->expr_str = s;
	return v;
}

static TreeId tree_table(Parser *par, Source line, TreeChain ch)
{
	return tree_poly(par, line, TREE_NEW_TABLE, NT_TAB, ch.tally, ch.head, ch.tail);
}

// Todo, the data type should be passed in ? ...
static TreeId tree_tuple(Parser *par, Source line, TreeChain ch)
{
	return tree_poly(par, line, TREE_TUPLE, NT_ANY, ch.tally, ch.head, ch.tail);
}

static TreeId tree_tuple2(Parser *par, Source line, TreeId x, TreeId y)
{
	TreeChain ch = {};
	tree_chain_add(&ch, x ? x : tree_nil(par, line));
	tree_chain_add(&ch, y ? y : tree_nil(par, line));
	return tree_tuple(par, line, ch);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static TreeId tree_memory2(Parser *par, Source line, TreeId x, int rem)
{
	ASSERT(rem >= 0);

	TreeId v = tree_unary(par, line, TREE_MEMORY, x->type, x);
	v->tree_memory.mem =  -1;
	v->tree_memory.rem = rem;
	return v;
}

static TreeId tree_memory(Parser *par, Source line, TreeId x)
{
	return tree_memory2(par, line, x, 1);
}

static inline TreeId tree_proxy(Parser *par, Source line, TreeId x)
{
	return tree_unary(par, line, TREE_PROXY, x->type, x);
}

static inline TreeId tree_proxy1(Parser *par, TreeId x)
{
	return tree_unary(par, x->line, TREE_PROXY, x->type, x);
}

static TreeId tree_global(Parser *par, Source line, int x)
{
	TreeId v = tree_nullary(par, line, TREE_GLOBAL, NT_ANY);
	v->expr_global = x;
	return v;
}

// todo: we should already have the Str by now...
static TreeId tree_global_symbol(Parser *par, Source line, const char *name)
{
	V v;
	to_str(&v, _string_new(par->inter, name));
	Index x = _table_getalways(par->R, par->inter->globals, v);

	ASSERT(x != -1);
	return tree_global(par, line, x);
}

static TreeId tree_enforce(Parser *par, Source line, TreeId x, TypeRule rule)
{
	TreeId v = tree_unary(par, line, TREE_ENFORCE, NT_NON, x);
	v->rule = rule;
	return v;
}

static TreeId tree_store(Parser *par, Source line, TreeId x, TreeId y)
{
	ASSERT(istree(x));
	ASSERT(istree(y));
	ASSERT(x->type != NT_NON);
	ASSERT(y->type != NT_NON);
	ASSERT(y->kind != TREE_RANGE_INDEX);
	return tree_binary(par, line, TREE_STORE, NT_NON, x, y);
}

static TreeId tree_add(Parser *par, Source line, TreeId x, TreeId y)
{
	return tree_binary(par, line, EXPR_ADD, NT_ANY, x, y);
}

static TreeId tree_add_int(Parser *par, Source line, TreeId x, Int y)
{
	return tree_add(par, line, x, tree_int(par, line, y));
}

// todo: intrinsic!
static TreeId tree_add_int_store(Parser *par, Source line, TreeId x, Int y)
{
	TreeId v = tree_add_int(par, line, x, y);
	return tree_store(par, line, x, v);
}

static TreeId tree_length(Parser *par, Source line, TreeId x)
{
	return tree_unary(par, line, TREE_LENGTH, NT_INT, x);
}

// Note, offset is an additional constant offset ...
static TreeId tree_index(Parser *par, Source line, TreeId x, TreeId y, i32 offset)
{
	// todo: builtin
	if (offset != 0)
	{
		y = tree_add_int(par, line, y, offset);
	}
	return tree_binary(par,line,TREE_INDEX,NT_ANY,x,y);
}

static TreeId tree_table_field(Parser *par, Source line, TreeId x, TreeId y)
{
	return tree_binary(par,line,TREE_TABLE_FIELD,NT_ANY,x,y);
}

static TreeId tree_ranged_index(Parser *par, Source line, TreeId x, TreeId y)
{
	return tree_binary(par,line,TREE_RANGE_INDEX,NT_ANY,x,y);
}

static TreeId tree_less_than(Parser *par, Source line, TreeId x, TreeId y)
{
	return tree_binary(par,line,EXPR_LT,NT_BOL,x,y);
}

static TreeId tree_eq_nil(Parser *par, Source line, TreeId x)
{
	return tree_binary(par,line,EXPR_EQ,NT_BOL,x,tree_nil(par,line));
}

static TreeId tree_meta_field(Parser *par, Source line, TreeId x, TreeId y)
{
	return tree_binary(par,line,EXPR_METAFIELD,NT_ANY,x,y);
}

static TreeId tree_closure_value(Parser *par, Source line, int x)
{
	TreeId v = tree_nullary(par, line, TREE_UPVALUE, NT_ANY);
	v->expr_cvalue = x;
	return v;
}

static TreeId tree_if(Parser *par, Source line, TreeId pred, TreeId true_clause, TreeId else_clause)
{
	TreeId v = tree_new(par,line,TREE_IF,NT_NON);
	v->tree_ifstat.pred = pred;
	v->tree_ifstat.true_clause = true_clause;
	v->tree_ifstat.else_clause = else_clause;
	return v;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline TreeId tree_get_first_call_arg(Tree *r)
{
	ASSERT(r->kind == TREE_CALL);
	ASSERT(r->x);
	ASSERT(r->x->next);
	return r->x->next;
}

static inline u32 tree_get_num_call_args(Tree *r)
{
	ASSERT(r->kind == TREE_CALL);
	ASSERT(r->n >= 1);
	return r->n - 1;
}

static inline TreeId tree_get_call_target(Tree *r)
{
	ASSERT(r->kind == TREE_CALL);
	ASSERT(r->x);
	return r->x;
}

static TreeId tree_call(Parser *par, Source line, TreeId x, TreeChain z)
{
	tree_chain_prepend(&z, x);
	TreeId v = tree_poly(par, line, TREE_CALL, NT_ANY, z.tally, z.head, z.tail);
	return v;
}

// Meta calls cannot be sugar coated at parse time because of bytecode optimizations done at build time
static TreeId tree_meta_call(Parser *par, Source curs, TreeId x, char *name, TreeChain z)
{
	TreeId field = tree_meta_field(par, curs, x, tree_str(par, curs, name));
	return tree_call(par, curs, field, z);
}

// todo: where to put this
// enum, symbol, variadic, num args, num rets
#define DEFCOREAPI(_) \
_(ASSERT,   "elf.assert"   , 0,  2,  0) \
_(FORMAT,   "elf.format"   , 0,  1,  2) \
_(SETMETA,  "elf.set_meta" , 0,  2,  1) \
_(GETMETA,  "elf.get_meta" , 0,  1,  1) \
_(LOADFILE, "elf.load_file", 0,  1,  1) \
/* end */


enum {
#define BUILTIN(EN, SY, V, A, R) BUILTIN_##EN,
	DEFCOREAPI(BUILTIN)
#undef BUILTIN
};

static const char *coreapi2s[] = {
#define BUILTIN(EN, SY, V, A, R) SY,
	DEFCOREAPI(BUILTIN)
#undef BUILTIN
};

static TreeId tree_callcoreapi(Parser *par, Source line, int id, TreeChain args)
{
	TreeId v = tree_global_symbol(par, line, coreapi2s[id]);
	v = tree_call(par, line, v, args);
	return v;
}

static TreeId tree_callcoreapi2(Parser *par, Source line, int id, TreeId x, TreeId y)
{
	TreeChain ch = {};
	tree_chain_add(&ch, x);
	tree_chain_add(&ch, y);
	TreeId v = tree_callcoreapi(par, line, id, ch);
	return v;
}

static TreeId tree_callcoreapi3(Parser *par, Source line, int id, TreeId x, TreeId y, TreeId z)
{
	TreeChain ch = {};
	tree_chain_add(&ch, x);
	tree_chain_add(&ch, y);
	tree_chain_add(&ch, z);
	TreeId v = tree_callcoreapi(par, line, id, ch);
	return v;
}

static TreeId tree_return(Parser *par, Source line, TreeChain z)
{
	return tree_poly(par, line, TREE_RET, NT_NON, z.tally, z.head, z.tail);
}

static TreeId tree_return1(Parser *par, Source line, TreeId x)
{
	TreeChain ch = {};
	tree_chain_add(&ch, x);
	return tree_return(par, line, ch);
}

static TreeId tree_goto(Parser *par, Source line)
{
	return tree_nullary(par,line,TREE_GOTO,NT_ANY);
}

static TreeId tree_block(Parser *par, Source line, TreeChain z)
{
	return tree_poly(par, line, TREE_BLOCK, NT_NON, z.tally, z.head, z.tail);
}
