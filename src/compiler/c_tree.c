//
// See Copyright Notice In elf.h
//



static bool is_tree_trivial_constant(Parser *parser, treeID id) {
	return id->kind == EXPR_INT || id->kind == EXPR_NUM;
}



static Tree get_tree(Parser *parser, treeID id) { return *id; }

static int get_tree_kind(Parser *parser, treeID id) { return id->kind; }

static int get_tree_type(Parser *parser, treeID id) { return id->type; }

static Source get_tree_line(Parser *parser, treeID id) { return id->line; }



static treeID new_tree(Parser *parser, Source line, int kind, int type) {
	// todo: proper arena
	treeID tree = & parser->tree_memory[parser->tree_index ++];
	tree->line = line;
	tree->kind = kind;
	tree->type = type;
	return tree;
}




static treeID tree_xyz(Parser *parser, Source line, int kind, int type, treeID x, treeID y, treeID *z) {
	treeID v=new_tree(parser,line,kind,type);
	v->x=x,v->y=y,v->z=z;
	return v;
}

static treeID tree_nullary(Parser *parser, Source line, int k, int t) {
	return tree_xyz(parser,line,k,t,Y_NULL,Y_NULL,Y_NULL);
}

static treeID tree_unary(Parser *parser, Source line, int k, int t, treeID x) {
	return tree_xyz(parser,line,k,t,x,Y_NULL,Y_NULL);
}

static treeID tree_binary(Parser *parser, Source line, int k, int t, treeID x, treeID y) {
	return tree_xyz(parser,line,k,t,x,y,0);
}

static treeID tree_nop(Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_NOP,NT_ANY);
}

static treeID tree_global(Parser *parser, Source line, int x) {
	treeID v = tree_nullary(parser,line,TREE_GLOBAL,NT_ANY);
	v->expr_global = x;
	return v;
}






// todo: we should already have the Str by now...
static treeID tree_global_symbol(Parser *parser, Source line, const char *name) {

	V v;
	to_str(&v, new_string(parser->inter, name));
	Index x = _table_getalways(parser->inter->globals, v);

	ASSERT(x != -1);
	return tree_global(parser, line, x);
}









static treeID tree_enforce(Parser *parser, Source line, treeID x, TypeRule rule) {
	treeID v = tree_unary(parser, line, TREE_ENFORCE, NT_NON, x);
	v->tree_enforce.rule = rule;
	return v;
}







static treeID tree_nil(Parser *parser, Source line) {
	return tree_nullary(parser,line,EXPR_NIL,NT_NIL);
}




static treeID tree_int(Parser *parser, Source line, elf_Integer i) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_INT,NT_INT);
	v->expr_int=i;
	return v;
}



static treeID tree_num(Parser *parser, Source line, elf_Number n) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_NUM,NT_NUM);
	v->expr_num=n;
	return v;
}



static treeID tree_str(Parser *parser, Source line, char *s) {
	treeID v = tree_nullary(parser,line,EXPR_STR,NT_STR);
	v->expr_str = s;
	return v;
}


static treeID tree_table(Parser *parser, Source line, treeID *key_value_tuples) {
	treeID v = tree_nullary(parser, line, TREE_NEW_TABLE, NT_TAB);
	v->expr_newtable.key_value_tuples = key_value_tuples;
	return v;
}


static treeID tree_closure(Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,EXPR_CLOSURE,NT_FUN,x,Y_NULL,z);
}



static treeID tree_ret2(Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser, line, TREE_RET, NT_NON, Y_NULL, Y_NULL, z);
}



static treeID tree_ret(Parser *parser, Source line, treeID x) {
	treeID *z = 0;
	if (x) {
		heap_array_add(z, x);
	}
	return tree_ret2(parser, line, z);
}


static treeID tree_goto(Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_GOTO,NT_ANY);
}

#if 0
static treeID tree_exit_loop(Parser *parser, Source line) {
	return tree_nullary(parser,line,TREE_EXIT_LOOP,NT_ANY);
}
static treeID tree_trace(Parser *parser, Source line) {
	return tree_nullary(parser, line, TREE_TRACE, NT_NON);
}
#endif

// , treeID *defers, treeID *breaks
static treeID tree_block(Parser *parser, Source line, treeID *stats) {
	treeID v = tree_nullary(parser,line,TREE_MEMORY_BLOCK,NT_NON);
	v->tree_blockstat.stats = stats;
	// v->tree_blockstat.defers = defers;
	// v->tree_blockstat.breaks = breaks;
	return v;

}




static treeID tree_memory2(Parser *parser, Source line, treeID x, int rem) {
	ASSERT(rem >= 0);

	treeID v = tree_unary(parser, line, TREE_MEMORY, x->type, x);
	v->tree_memory.mem =  -1;
	v->tree_memory.rem = rem;
	return v;
}




static treeID tree_memory(Parser *parser, Source line, treeID x) {
	return tree_memory2(parser, line, x, 1);
}




static treeID tree_store(Parser *parser, Source line, treeID x, treeID y) {
	ASSERT(istree(x));
	ASSERT(istree(y));
	return tree_binary(parser,line,TREE_STORE,NT_NON,x,y);
}




static treeID tree_add(Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser, line, EXPR_ADD, NT_ANY, x, y);
}



static treeID tree_add_int(Parser *parser, Source line, treeID x, Int y) {
	return tree_add(parser, line, x, tree_int(parser, line, y));
}



// todo: intrinsic!
static treeID tree_add_int_store(Parser *parser, Source line, treeID x, Int y) {
	treeID v = tree_add_int(parser, line, x, y);
	return tree_store(parser, line, x, v);
}




static treeID tree_length(Parser *parser, Source line, treeID x) {
	return tree_unary(parser, line, TREE_LENGTH, NT_INT, x);
}




static treeID tree_index(Parser *parser, Source line, treeID x, treeID y, int offset) {
	// todo: builtin
	if (offset != 0) {
		y = tree_add_int(parser, line, y, offset);
	}
	return tree_binary(parser,line,TREE_INDEX,NT_ANY,x,y);
}





static treeID tree_table_field(Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,TREE_TABLE_FIELD,NT_ANY,x,y);
}








static treeID tree_ranged_index(Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_RANGE_INDEX,NT_ANY,x,y);
}




static treeID tree_call(Parser *parser, Source line, treeID x, treeID *z) {
	treeID v = tree_xyz(parser,line,TREE_CALL,NT_ANY,x,Y_NULL,z);
	return v;
}



static treeID tree_tuple(Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,TREE_TUPLE,NT_ANY,Y_NULL,Y_NULL,z);
}



static treeID tree_tuple2(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xyz(parser,line,TREE_TUPLE,NT_ANY, x, y, 0);
}



static treeID tree_less_than(Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_LT,NT_BOL,x,y);
}
static treeID tree_eq_nil(Parser *parser, Source line, treeID x) {
	return tree_binary(parser,line,EXPR_EQ,NT_BOL,x,tree_nil(parser,line));
}


static treeID tree_meta_field(Parser *parser, Source line, treeID x, treeID y) {
	return tree_binary(parser,line,EXPR_METAFIELD,NT_ANY,x,y);
}








// todo: meta calls cannot be sugar coated at parse time
// because of bytecode optimizations, that we wouldn't otherwise
// know how to do because we be dumb like that
static treeID tree_meta_call(Parser *parser, Source line, treeID x, char *name, treeID *z) {
	treeID field = tree_meta_field(parser,line,x,tree_str(parser,line,name));
	return tree_call(parser,line,field,z);
}




static treeID tree_closure_value(Parser *parser, Source line, int x) {
	treeID v;
	v=tree_nullary(parser,line,TREE_UPVALUE,NT_ANY);
	v->expr_cvalue=x;
	return v;
}

static treeID tree_if(Parser *parser, Source line, treeID pred, treeID true_clause, treeID else_clause) {
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




static treeID tree_callcoreapi(Parser *parser, Source line, int id, treeID *z) {
	treeID v = tree_global_symbol(parser, line, coreapi2s[id]);
	v = tree_call(parser, line, v, z);
	return v;
}





static treeID tree_callcoreapi2(Parser *parser, Source line, int id, treeID x, treeID y) {
	treeID *args = 0;
	heap_array_add(args, x);
	heap_array_add(args, y);
	treeID v = tree_callcoreapi(parser, line, id, args);
	return v;
}





static treeID tree_callcoreapi3(Parser *parser, Source line, int id, treeID x, treeID y, treeID z) {
	treeID *args = 0;
	heap_array_add(args, x);
	heap_array_add(args, y);
	heap_array_add(args, z);
	treeID v = tree_callcoreapi(parser, line, id, args);
	return v;
}
