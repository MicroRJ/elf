/*
** See Copyright Notice In elf.h
** tree.c
*/


treeT get_tree(Parser *parser, treeID id) { return *id; }
int get_tree_kind(Parser *parser, treeID id) { return id->kind; }
int get_tree_type(Parser *parser, treeID id) { return id->type; }
Source get_tree_line(Parser *parser, treeID id) { return id->line; }

static treeID new_tree(Parser *parser, int kind, Source line) {
	// elf_debug_log("NEW TREE: %s",tree2s[kind]);
	//todo:switch to linear allocator, this is slow!
	treeID tree=calloc(sizeof(treeT),1);
	tree->kind=kind;
	tree->line=line;
	return tree;
}
static treeID tree_xyz(Parser *parser, Source line, int kind, int type, treeID x, treeID y, treeID *z) {
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
static treeID tree_xy(Parser *parser, Source line, int k, int t, treeID x, treeID y) {
	return tree_xyz(parser,line,k,t,x,y,0);
}
static treeID tree_x(Parser *parser, Source line, int k, int t, treeID x) {
	return tree_xy(parser,line,k,t,x,NO_TREE);
}
static treeID tree_nullary(Parser *parser, Source line, int k, int t) {
	return tree_x(parser,line,k,t,NO_TREE);
}
static treeID tree_nil(Parser *parser, Source line) {
	return tree_nullary(parser,line,EXPR_NIL,NT_NIL);
}
static treeID tree_int(Parser *parser, Source line, elf_Int i) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_INT,NT_INT);
	v->expr_int=i;
	return v;
}
static treeID tree_num(Parser *parser, Source line, elf_Num n) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_NUM,NT_NUM);
	v->expr_num=n;
	return v;
}
static treeID tree_str(Parser *parser, Source line, char *s) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_STR,NT_STR);
	v->expr_str=s;
	return v;
}
static treeID tree_table(Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,EXPR_TAB,NT_TAB,NO_TREE,NO_TREE,z);
}
static treeID tree_closure(Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,EXPR_CLOSURE,NT_FUN,x,NO_TREE,z);
}
// xx static treeID tree_type_guard(Parser *parser, Source line, treeID x, int y) {
// xx 	return tree_xy(parser,line,EXPR_TYPEGUARD,y,x,y);
// xx }
static treeID tree_ret(Parser *parser, Source line, treeID x) {
	return tree_x(parser,line,TREE_RET,NT_ANY,x);
}
static treeID tree_block(Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,STAT_BLOCK,NT_NON,NO_TREE,NO_TREE,z);
}

static treeID tree_field(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,EXPR_FIELD,NT_ANY,x,y);
}
static treeID tree_index(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,EXPR_INDEX,NT_ANY,x,y);
}
static treeID tree_ranged_index(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,EXPR_RANGE_INDEX,NT_ANY,x,y);
}
static treeID tree_metafield(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,EXPR_METAFIELD,NT_ANY,x,y);
}
static treeID tree_call(Parser *parser, Source line, treeID x, treeID *z) {
	return tree_xyz(parser,line,EXPR_CALL,NT_ANY,x,NO_TREE,z);
}
static treeID tree_multi(Parser *parser, Source line, treeID *z) {
	return tree_xyz(parser,line,EXPR_MULTI,NT_ANY,NO_TREE,NO_TREE,z);
}

static treeID tree_assign_mem(Parser *parser, Source line, treeID x) {
	return tree_x(parser,line,TREE_ASSIGN_MEM,NT_NON,x);
}
static treeID tree_store(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,TREE_STORE,NT_NON,x,y);
}
static treeID tree_less_than(Parser *parser, Source line, treeID x, treeID y) {
	return tree_xy(parser,line,EXPR_LT,NT_BOL,x,y);
}
static treeID tree_eq_nil(Parser *parser, Source line, treeID x) {
	return tree_xy(parser,line,EXPR_EQ,NT_BOL,x,tree_nil(parser,line));
}


static treeID tree_call_metafield(Parser *parser, Source line, treeID x, treeID *z, char *name) {
	treeID field = tree_metafield(parser,line,x,tree_str(parser,line,name));
	return tree_call(parser,line,field,z);
}

static treeID tree_this_ref(Parser *parser, Source line) {
	return tree_nullary(parser,line,EXPR_THIS_REF,NT_ANY);
}

static treeID tree_global_ref(Parser *parser, Source line, char *name, int x) {
	treeID v;
	v=tree_nullary(parser,line,EXPR_GLOBAL_REF,NT_ANY);
	v->expr_global=x;
	return v;
}

static treeID tree_global_ref_by_name(Parser *parser, Source line, char *name) {
	int x = elf_get_global(parser->R->M,elf_alloc_string(parser->R,name));
	ASSERT(x != -1);
	return tree_global_ref(parser,line,name,x);
}


static treeID tree_call_pf(Parser *parser, Source line, treeID *args) {
	treeID fn = tree_global_ref_by_name(parser,line,"elf.pf");
	return tree_call(parser,line,fn,args);
}


static treeID tree_call_set_metatable(Parser *parser, Source line, treeID object, treeID metatable) {
	treeID fn = tree_global_ref_by_name(parser,line,"elf.set_object_metatable");
	treeID *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return tree_call(parser,line,fn,z);
}

// todo: this doesn't require the id anymore, is just
// the current function
#if 0

static elf_Bool is_binary_node(int kind) {
	return kind >= EXPR_AND && kind <= EXPR_BIT_OR;
}


static void fpf_node(Parser *parser, FILE *io, treeID id) {
	treeT node = get_tree(parser,id);
	if (is_binary_node(node.kind)) {
		fprintf(io, "(%s ", node2s[node.kind]);
		fpf_node(parser,io,node.x);
		fprintf(io, ", ");
		fpf_node(parser,io,node.y);
		fprintf(io, ")");
	} else switch (node.kind) {
		case EXPR_INDEX: {
			fpf_node(parser,io,node.x);
			fprintf(io, "[");
			fpf_node(parser,io,node.y);
			fprintf(io, "]");
		} break;
		case EXPR_INTEGER: fprintf(io,"int(%lli)",node.i); break;
		case EXPR_NUMBER: fprintf(io,"num(%f)",node.n); break;
		case EXPR_NIL: fprintf(io,"nil"); break;
		// xx case EXPR_GROUP: {
		// xx 	fprintf(io,"(");
		// xx 	fpf_node(parser,io,node.x);
		// xx 	fprintf(io,")");
		// xx } break;
		default: fprintf(io,"%s",node2s[node.kind]);
	}
}
static ByteOP ir2b(int tt);

static elf_Bool tree_is_lvalue(int kind) {
	switch (kind) {
		case EXPR_RANGE_INDEX:
		case EXPR_GLOBAL:
		case EXPR_LOCAL:
		case EXPR_INDEX:
		case EXPR_FIELD: {
			return 1;
		}
		default: {
			return 0;
		}
	}
}
#endif


// elf_ValueTag node2tag(int ty) {
// 	switch (ty) {
// 		case NT_SYS: return elf_TAG_SYS;
// 		case NT_NUM: return elf_TAG_NUM;
// 		case NT_INT: return elf_TAG_INT;
// 		default: NO_CODE;
// 	}
// 	return elf_TAG_NIL;
// }
