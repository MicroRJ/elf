//
// See Copyright Notice In elf.h
//






static bool is_tree_trivial_constant(elf_Parser *parser, treeID id) {
	return id->kind == EXPR_INT || id->kind == EXPR_NUM;
}

static treeT get_tree(elf_Parser *parser, treeID id) { return *id; }

static int get_tree_kind(elf_Parser *parser, treeID id) { return id->kind; }

static int get_tree_type(elf_Parser *parser, treeID id) { return id->type; }

static Source get_tree_line(elf_Parser *parser, treeID id) { return id->line; }

static treeID new_tree(elf_Parser *parser, Source line, int kind, int type) {
	// todo: proper arena
	treeID tree = calloc(sizeof(treeT),1);
	tree->line = line;
	tree->kind = kind;
	tree->type = type;
	return tree;
}

static treeID tree_xyz(elf_Parser *parser, Source line, int kind, int type, treeID x, treeID y, treeID *z) {
	treeID v=new_tree(parser,line,kind,type);
	v->x=x,v->y=y,v->z=z;
	return v;
}

static treeID tree_nullary(elf_Parser *parser, Source line, int k, int t) {
	return tree_xyz(parser,line,k,t,NO_TREE,NO_TREE,NO_TREE);
}

static treeID tree_unary(elf_Parser *parser, Source line, int k, int t, treeID x) {
	return tree_xyz(parser,line,k,t,x,NO_TREE,NO_TREE);
}

static treeID tree_binary(elf_Parser *parser, Source line, int k, int t, treeID x, treeID y) {
	return tree_xyz(parser,line,k,t,x,y,0);
}

static treeID tree_nop(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_NOP,NT_ANY);
}

static treeID tree_global(elf_Parser *parser, Source line, int x) {
	treeID v = tree_nullary(parser,line,TREE_GLOBAL,NT_ANY);
	v->expr_global = x;
	return v;
}



// todo: we should already have the String by now...
static treeID tree_global_symbol(elf_Parser *parser, Source line, const char *name) {
	int x = elf_get_global_slot(parser->R, elf_alloc_string(parser->R, name));
	ASSERT(x != -1);
	return tree_global(parser, line, x);
}



static treeID tree_label(elf_Parser *parser, Source line, treeID *z) {
	treeID v = tree_nullary(parser,line,TREE_GLOBAL,NT_ANY);
	v->z = z;
	return v;
}

static treeID tree_nil(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,EXPR_NIL,NT_NIL);
}
static treeID tree_int(elf_Parser *parser, Source line, elf_Integer i) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_INT,NT_INT);
	v->expr_int=i;
	return v;
}



static treeID tree_num(elf_Parser *parser, Source line, elf_Number n) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_NUM,NT_NUM);
	v->expr_num=n;
	return v;
}



static treeID tree_str(elf_Parser *parser, Source line, char *s) {
	treeID v = tree_nullary(parser,line,EXPR_STR,NT_STR);
	v->expr_str = s;
	return v;
}

static treeID tree_table(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_NEW_TABLE,NT_TAB);
}
static treeID tree_closure(elf_Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,EXPR_CLOSURE,NT_FUN,x,NO_TREE,z);
}

// xx static treeID tree_type_guard(elf_Parser *parser, Source line, treeID x, int y) {
// xx 	return tree_binary(parser,line,EXPR_TYPEGUARD,y,x,y);
// xx }
static treeID tree_ret(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser,line,TREE_RET,NT_ANY,x);
}
static treeID tree_goto(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_GOTO,NT_ANY);
}

static treeID tree_block(elf_Parser *parser, Source line, treeID *stats, treeID *defers, treeID *breaks ) {
	treeID v = tree_nullary(parser,line,STAT_BLOCK,NT_NON);
	v->tree_blockstat.stats = stats;
	v->tree_blockstat.defers = defers;
	v->tree_blockstat.breaks = breaks;
	return v;

}

static treeID tree_field(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_FIELD,NT_ANY,x,y);
}
static treeID tree_index(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_INDEX,NT_ANY,x,y);
}
static treeID tree_ranged_index(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_RANGE_INDEX,NT_ANY,x,y);
}
static treeID tree_call(elf_Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,TREE_CALL,NT_ANY,x,NO_TREE,z);
}
static treeID tree_tuple(elf_Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,TREE_TUPLE,NT_ANY,NO_TREE,NO_TREE,z);
}

static treeID tree_assign_mem(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser,line,TREE_SETMEM,NT_NON,x);
}
static treeID tree_store(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,TREE_STORE,NT_NON,x,y);
}

// if the tree doesn't have any memory, it gets loaded, otherwise it gets reloaded
// into a new memory slot that slot is associated with this tree.
static treeID tree_reload(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser, line, TREE_RELOAD, get_tree_type(parser, x), x);
}

static treeID tree_proxy(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser, line, TREE_PROXY, get_tree_type(parser, x), x);
}

static treeID tree_less_than(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_LT,NT_BOL,x,y);
}
static treeID tree_eq_nil(elf_Parser *parser, Source line, treeID x) {
	return tree_binary(parser,line,EXPR_EQ,NT_BOL,x,tree_nil(parser,line));
}


static treeID tree_meta_field(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_METAFIELD,NT_ANY,x,y);
}

static treeID tree_call_set_meta(elf_Parser *parser, Source line, treeID object, treeID metatable) {
	treeID name = tree_global_symbol(parser,line,"elf.set_meta");
	treeID *z = 0;
	darr_add(z, object);
	darr_add(z, metatable);
	return tree_call(parser,line,name,z);
}


static treeID tree_meta_call(elf_Parser *parser, Source line, treeID x, treeID *z, char *name) {
	treeID field = tree_meta_field(parser,line,x,tree_str(parser,line,name));
	return tree_call(parser,line,field,z);
}

static treeID tree_closure_value(elf_Parser *parser, Source line, int x) {
	treeID v;
	v=tree_nullary(parser,line,TREE_UPVALUE,NT_ANY);
	v->expr_upvalue=x;
	return v;
}

static treeID tree_if(elf_Parser *parser, Source line, treeID pred, treeID true_clause, treeID else_clause) {
	treeID v=new_tree(parser,line,TREE_IF,NT_NON);
	v->tree_ifstat.pred=pred;
	v->tree_ifstat.true_clause=true_clause;
	v->tree_ifstat.else_clause=else_clause;
	return v;
}

// todo: where to put this
// enum, symbol, variadic, num args, num rets
#define DEFCOREAPI(_) \
_(ASSERT,  "elf.assert"  , 0,  2,  0) \
_(FORMAT,  "elf.format"  , 0,  1,  2) \
_(SETMETA, "elf.set_meta", 0,  2,  1) \
_(GETMETA, "elf.get_meta", 0,  1,  1) \
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


static treeID tree_callcoreapi(elf_Parser *parser, Source line, int id, treeID *z) {
	treeID v = tree_global_symbol(parser, line, coreapi2s[id]);
	v = tree_call(parser, line, v, z);
	return v;
}

static treeID tree_callcoreapi2(elf_Parser *parser, Source line, int id, treeID x, treeID y) {
	treeID *args = 0;
	darr_add(args, x);
	darr_add(args, y);
	treeID v = tree_callcoreapi(parser, line, id, args);
	return v;
}

static treeID tree_callcoreapi3(elf_Parser *parser, Source line, int id, treeID x, treeID y, treeID z) {
	treeID *args = 0;
	darr_add(args, x);
	darr_add(args, y);
	darr_add(args, z);
	treeID v = tree_callcoreapi(parser, line, id, args);
	return v;
}
