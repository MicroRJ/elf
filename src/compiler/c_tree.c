//
// See Copyright Notice In elf.h
//



static bool is_tree_trivial_constant(elf_Parser *parser, treeID id) {
	return id->kind == EXPR_INT || id->kind == EXPR_NUM;
}



static Tree get_tree(elf_Parser *parser, treeID id) { return *id; }

static int get_tree_kind(elf_Parser *parser, treeID id) { return id->kind; }

static int get_tree_type(elf_Parser *parser, treeID id) { return id->type; }

static Source get_tree_line(elf_Parser *parser, treeID id) { return id->line; }



static treeID new_tree(elf_Parser *parser, Source line, int kind, int type) {
	// todo: proper arena
	treeID tree = & parser->tree_memory[parser->tree_index ++];
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
	return tree_xyz(parser,line,k,t,Y_NULL,Y_NULL,Y_NULL);
}

static treeID tree_unary(elf_Parser *parser, Source line, int k, int t, treeID x) {
	return tree_xyz(parser,line,k,t,x,Y_NULL,Y_NULL);
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



// todo: we should already have the Str by now...
static treeID tree_global_symbol(elf_Parser *parser, Source line, const char *name) {

	V v;
	vsetstr(&v, newstr(parser->inter, name));
	Index x = tablecreateindex(parser->inter->globals, v);

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


static treeID tree_table(elf_Parser *parser, Source line, treeID *key_value_tuples) {
	treeID v = tree_nullary(parser, line, TREE_NEW_TABLE, NT_TAB);
	v->expr_newtable.key_value_tuples = key_value_tuples;
	return v;
}


static treeID tree_closure(elf_Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,EXPR_CLOSURE,NT_FUN,x,Y_NULL,z);
}


static treeID tree_ret(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser,line,TREE_RET,NT_ANY,x);
}


static treeID tree_goto(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_GOTO,NT_ANY);
}

#if 0
static treeID tree_exit_loop(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_EXIT_LOOP,NT_ANY);
}
static treeID tree_trace(elf_Parser *parser, Source line) {
	return tree_nullary(parser, line, TREE_TRACE, NT_NON);
}
#endif

// , treeID *defers, treeID *breaks
static treeID tree_block(elf_Parser *parser, Source line, treeID *stats) {
	treeID v = tree_nullary(parser,line,TREE_MEMORY_BLOCK,NT_NON);
	v->tree_blockstat.stats = stats;
	// v->tree_blockstat.defers = defers;
	// v->tree_blockstat.breaks = breaks;
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
	return tree_xyz(parser,line,TREE_CALL,NT_ANY,x,Y_NULL,z);
}

static treeID tree_tuple(elf_Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,TREE_TUPLE,NT_ANY,Y_NULL,Y_NULL,z);
}

static treeID tree_tuple2(elf_Parser *parser, Source line, treeID x, treeID y) {
	return tree_xyz(parser,line,TREE_TUPLE,NT_ANY, x, y, 0);
}


static treeID tree_memory(elf_Parser *parser, Source line, treeID x) {
	treeID v = tree_unary(parser,line,TREE_MEMORY,x->type,x);
	v->tree_memory.mem = -1;
	return v;
}


static treeID tree_store(elf_Parser *parser, Source line, treeID x, treeID y) {
	ASSERT(TREE_NO_ERROR(x));
	ASSERT(TREE_NO_ERROR(y));
	return tree_binary(parser,line,TREE_STORE,NT_NON,x,y);
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


// todo: meta calls cannot be sugar coated at parse time
// becase of bytecode optimizations
static treeID tree_meta_call(elf_Parser *parser, Source line, treeID x, char *name, treeID *z) {
	//	treeID y = tree_str(parser,line,name);
	//	return tree_xyz(parser,line,TREE_META_CALL,NT_ANY,x,y,z);

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
