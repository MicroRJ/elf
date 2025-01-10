/*
** See Copyright Notice In elf.h
** tree.c
*/


treeT get_tree(elf_Parser *parser, treeID id) { return *id; }
int get_tree_kind(elf_Parser *parser, treeID id) { return id->kind; }
int get_tree_type(elf_Parser *parser, treeID id) { return id->type; }
Source get_tree_line(elf_Parser *parser, treeID id) { return id->line; }

static treeID new_tree(elf_Parser *parser, int kind, Source line) {
	// elf_debug_log("NEW TREE: %s",tree2s[kind]);
	//todo:switch to linear allocator, this is slow!
	treeID tree=calloc(sizeof(treeT),1);
	tree->kind=kind;
	tree->line=line;
	return tree;
}
static treeID tree_xyz(elf_Parser *parser, Source line, int kind, int type, treeID x, treeID y, treeID *z) {
	treeID v;
	v=new_tree(parser,kind,line);
	v->line=line;
	v->type=type;
	v->kind=kind;
	v->x=x;
	v->y=y;
	v->z=z;
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
	treeID v;
	v=tree_nullary(parser,line,TREE_GLOBAL,NT_ANY);
	v->expr_global=x;
	return v;
}

static treeID tree_global_ref_by_name(elf_Parser *parser, Source line, char *name) {
	int x = elf_get_global(parser->R->M,elf_alloc_string(parser->R,name));
	ASSERT(x != -1);
	return tree_global(parser,line,x);
}


static treeID tree_nil(elf_Parser *parser, Source line) {
	return tree_nullary(parser,line,EXPR_NIL,NT_NIL);
}
static treeID tree_int(elf_Parser *parser, Source line, elf_Int i) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_INT,NT_INT);
	v->expr_int=i;
	return v;
}
static treeID tree_num(elf_Parser *parser, Source line, elf_Num n) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_NUM,NT_NUM);
	v->expr_num=n;
	return v;
}
static treeID tree_str(elf_Parser *parser, Source line, char *s) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_STR,NT_STR);
	v->expr_str=s;
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

static treeID tree_block(elf_Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,STAT_BLOCK,NT_NON,NO_TREE,NO_TREE,z);
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
static treeID tree_multi(elf_Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,EXPR_MULTI,NT_ANY,NO_TREE,NO_TREE,z);
}

static treeID tree_assign_mem(elf_Parser *parser, Source line, treeID x) {
	return tree_unary(parser,line,TREE_ASSIGN_MEM,NT_NON,x);
}
static treeID tree_store(elf_Parser *parser, Source line, treeID x, treeID y) {
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

static treeID tree_call_set_meta(elf_Parser *parser, Source line, treeID object, treeID metatable) {
	treeID fn = tree_global_ref_by_name(parser,line,"elf.set_meta");
	treeID *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return tree_call(parser,line,fn,z);
}


static treeID tree_meta_call(elf_Parser *parser, Source line, treeID x, treeID *z, char *name) {
	treeID field = tree_meta_field(parser,line,x,tree_str(parser,line,name));
	return tree_call(parser,line,field,z);
}

static treeID tree_upvalue_ref(elf_Parser *parser, Source line, int x) {
	treeID v;
	v=tree_nullary(parser,line,TREE_UPVALUE,NT_ANY);
	v->expr_upvalue=x;
	return v;
}

static treeID tree_call_pf(elf_Parser *parser, Source line, treeID *args) {
	treeID fn = tree_global_ref_by_name(parser,line,"elf.pf");
	return tree_call(parser,line,fn,args);
}

static treeID tree_if(elf_Parser *parser, Source line, treeID pred, treeID true_clause, treeID else_clause) {
	treeID v;
	v=new_tree(parser,TREE_IF,line);
	v->stat_if.pred=pred;
	v->stat_if.true_clause=true_clause;
	v->stat_if.else_clause=else_clause;
	return v;
}