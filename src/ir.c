/*
** See Copyright Notice In elf.h
** ir.c
*/


IR_Id node_push_memory_state(Parser *parser, Source src);
IR_Id node_pop_memory_state(Parser *parser, Source src);



#define IR_ENUM(NAME) #NAME,
INTERNAL char *node2s[] = {
	"NONE",
	IRDEF(IR_ENUM)
};
#undef IR_ENUM


IR_Node get_ir(Parser *fs, IR_Id id) { return fs->ir[id]; }
/* Todo: Actually this reminds me, we could encode the ir type and kind
in the id, and use the low 16 bits for the id itself... */
IR_Kind get_ir_kind(Parser *fs, IR_Id id) { return get_ir(fs,id).kind; }
IR_DataTy get_ir_type(Parser *fs, IR_Id id) { return get_ir(fs,id).type; }
Source get_ir_line(Parser *fs, IR_Id id) { return get_ir(fs,id).line; }

IR_Id ir_get_label(Parser *parser) {
	return parser->ir_index;
}


void ir_add_prox(Parser *parser, IR_Id prox) {
	IR_Id prev = parser->prev;
	//note:we can still link to a node from
	//the previous label
	ASSERT(prox > 0);
	ASSERT(prox >= parser->label->src);
	ASSERT(prox < parser->label->end);
	//note:the previous nodes should not be linked
	//to anything
	ASSERT(get_ir(parser,prox).prox == 0xffff);
	ASSERT(get_ir(parser,prev).prox == 0xffff);

	elf_debug_log("%s -> %s"
	, node2s[get_ir_kind(parser,prev)]
	, node2s[get_ir_kind(parser,prox)]);

	parser->ir[prev].prox = prox;
	parser->prev = prox;
}


// seems we're going for stateful approach
typedef Parser IR_Builder;

int ir_start_label(Parser *parser, char *name) {
	IR_Function *func = & parser->funcs[parser->func];
	int label = ARRAY_GROW(func->labels,1);
	func->labels[label].name = name;
	func->labels[label].mark = 0;
	func->labels[label].src = parser->ir_index;
	func->labels[label].end = parser->ir_index;
	elf_debug_log("-- LABEL %s [%i] @ %04X",name,label,parser->ir_index);
	parser->label = & func->labels[label];
	return label;
}

void ir_close_func(IR_Builder *parser) {
	IR_Id ir = node_pop_memory_state(parser,0);
	ir_add_prox(parser,ir);
	IR_Function *func = & parser->funcs[parser->func];
	parser->func = func->enclosing;
}

IR_FuncId ir_begin_func(IR_Builder *parser, Source line) {
	/* Todo: switch to using some other allocation strategy */

	IR_Function func = {};
	func.enclosing   = parser->func;
	func.line        = line;

	IR_FuncId id = ARRAY_GROW(parser->funcs,1);
	parser->funcs[id] = func;

	elf_debug_log("IR.FUNC %i", id);

	parser->func = id;

	ir_start_label(parser,"START");

	IR_Id ir;
	ir=node_push_memory_state(parser,line);

	parser->prev=ir;
	return id;
}

IR_Id node_xyz(IR_Builder *builder, Source line, IR_Kind kind, IR_DataTy type, IR_Id x, IR_Id y, IR_Id *z) {

	//todo:switch allocation strategy
	IR_Id id = ARRAY_LENGTH(builder->ir);

	elf_debug_log("%02i IR: %s", id,node2s[kind]);

	ASSERT(builder->label);
	ASSERT(id == builder->ir_index);
	ASSERT(builder->label->src <= builder->label->end);
	ASSERT(builder->label->end == builder->ir_index);

	/* update current basic block */
	builder->label->end ++;

	ARRAY_GROW(builder->ir,1);

	IR_Node *ir = & builder->ir[builder->ir_index ++];
	ir->line = line;
	ir->type = type;
	ir->kind = kind;
	ir->prox = 0xffff;
	ir->x = x;
	ir->y = y;
	ir->z = z;
	return id;
}

IR_Id node_goto(Parser *parser, Source line, int bb) {
	IR_Id id = node_x(parser,line,IR_GOTO,NT_NON,bb);
	return id;
}

IR_Id node_if(Parser *fs, Source line, IR_Id cond, int true_bb, int false_bb) {
	IR_Id id = node_x(fs,line,IR_IF,NT_NON,cond);
	return id;
}

IR_Id node_xy(Parser *fs, Source line, IR_Kind k, IR_DataTy t, IR_Id x, IR_Id y) {
	return node_xyz(fs,line,k,t,x,y,0);
}


IR_Id node_x(Parser *fs, Source line, IR_Kind k, IR_DataTy t, IR_Id x) {
	return node_xy(fs,line,k,t,x,NO_IR);
}


IR_Id node_nullary(Parser *fs, Source line, IR_Kind k, IR_DataTy t) {
	return node_x(fs,line,k,t,NO_IR);
}

IR_Id node_type_guard(Parser *fs, Source line, IR_Id x, IR_DataTy y) {
	return node_xy(fs,line,IR_TYPEGUARD,y,x,y);
}


/* Todo: this is deprecated, doesn't serve any purpose, should
only be a parse time thing... */
// IR_Id node_group(Parser *fs, Source line, IR_Id x) {
// 	IR_Id id = node_x(fs,line,IR_GROUP,get_ir_type(fs,x),x);
// 	return id;
// }


IR_Id node_int(Parser *parser, Source line, elf_Int i) {
	IR_Id v = node_nullary(parser,line,IR_INTEGER,NT_INT);
	parser->ir[v].i = i;
	return v;
}


IR_Id ir_number(Parser *parser, Source line, elf_Num n) {
	IR_Id v = node_nullary(parser,line,IR_NUMBER,NT_NUM);
	parser->ir[v].n = n;
	return v;
}


IR_Id node_str(Parser *parser, Source line, char *s) {
	IR_Id v = node_nullary(parser,line,IR_STRING,NT_STR);
	parser->ir[v].s = s;
	return v;
}


IR_Id node_new_table(Parser *fs, Source line, IR_Id *z) {
	return node_xyz(fs,line,IR_TABLE,NT_TAB,NO_IR,NO_IR,z);
}


IR_Id ir_new_closure(Parser *fs, Source line, IR_Id x, IR_Id *z) {
	return node_xyz(fs,line,IR_CLOSURE,NT_FUN,x,NO_IR,z);
}


IR_Id node_nil(Parser *fs, Source line) {
	return node_nullary(fs,line,IR_NIL,NT_NIL);
}

// Note: I guess could be the default value, is not
// used for now...
IR_Id ir_param(Parser *parser, Source line, IR_Id x) {
	return node_x(parser,line,IR_PARAM,NT_ANY,x);
}

IR_Id ir_yield(Parser *parser, Source line, IR_Id x) {
	return node_x(parser,line,IR_YIELD,NT_ANY,x);
}

/* Todo: deprecated */
// notice how we were referencing the stack directly...
IR_Id node_closure_value(Parser *fs, Source line, elf_StackId x) {
	__debugbreak();
	return node_x(fs,line,IR_CLSVAL,NT_ANY,x);
}

/* memory definition for a value */
IR_Id node_local(Parser *fs, Source line, IR_Id x) {
	return node_x(fs,line,IR_LOCAL,NT_ANY,x);
}

IR_Id node_store(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_STORE,NT_NON,x,y);
}

IR_Id node_load(Parser *parser, Source line, IR_Id x) {
	return node_x(parser,line,IR_LOAD,NT_ANY,x);
}

//todo: hack, refers directly to memory
IR_Id node_load_direct(Parser *parser, Source line, int x) {
	return node_x(parser,line,IR_LOAD_DIRECT,NT_ANY,x);
}


IR_Id ir_this(Parser *fs, Source line) {
	return node_load_direct(fs,line,0);
}


IR_Id node_global(Parser *fs, Source line, elf_SymbolId x) {
	return node_x(fs,line,IR_GLOBAL,NT_ANY,x);
}


IR_Id node_field(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_FIELD,NT_ANY,x,y);
}


IR_Id node_index(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_INDEX,NT_ANY,x,y);
}


IR_Id node_ranged_index(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_RANGE_INDEX,NT_ANY,x,y);
}


IR_Id node_metafield(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_METAFIELD,NT_ANY,x,y);
}


IR_Id node_call(Parser *fs, Source line, IR_Id x, IR_Id *z) {
	return node_xyz(fs,line,IR_CALL,NT_ANY,x,NO_IR,z);
}


IR_Id node_multi(Parser *fs, Source line, IR_Id *z) {
	return node_xyz(fs,line,IR_MULTI,NT_ANY,NO_IR,NO_IR,z);
}


IR_Id node_less_than(Parser *fs, Source line, IR_Id x, IR_Id y) {
	return node_xy(fs,line,IR_LT,NT_BOL,x,y);
}


IR_Id node_eq_nil(Parser *fs, Source line, IR_Id x) {
	return node_xy(fs,line,IR_EQ,NT_BOL,x,node_nil(fs,line));
}


IR_Id node_call_metafield(Parser *fs, Source line, IR_Id x, IR_Id *z, char *name) {
	IR_Id field = node_metafield(fs,line,x,node_str(fs,line,name));
	return node_call(fs,line,field,z);
}


IR_Id node_global_name(Parser *fs, Source line, char *name) {
	elf_SymbolId x = elf_get_global(fs->M,elf_alloc_string(fs->R,name));
	ASSERT(x != -1);
	return node_global(fs,line,x);
}


IR_Id node_call_pf(Parser *fs, Source line, IR_Id *args) {
	IR_Id fn = node_global_name(fs,line,"elf.pf");
	return node_call(fs,line,fn,args);
}


IR_Id node_call_set_metatable(Parser *fs, Source line, IR_Id object, IR_Id metatable) {
	IR_Id fn = node_global_name(fs,line,"elf.set_object_metatable");
	IR_Id *z = 0;
	ARRAY_ADD(z,object);
	ARRAY_ADD(z,metatable);
	return node_call(fs,line,fn,z);
}

// xx IR_Id node_basic_block(Parser *parser, Source line, int bb) {
// xx 	return node_x(parser,line,IR_BASIC_BLOCK,NT_NON,bb);
// xx }
IR_Id node_push_memory_state(Parser *parser, Source src) {
	return node_nullary(parser,src,IR_PUSH_MEMORY_STATE,NT_NON);
}
IR_Id node_pop_memory_state(Parser *parser, Source src) {
	return node_nullary(parser,src,IR_POP_MEMORY_STATE,NT_NON);
}



// todo: this doesn't require the id anymore, is just
// the current function
void ir_set_func_arity(IR_Builder *builder, IR_FuncId id, int arity) {
	ASSERT(builder->func == id);
	IR_Function *func = & builder->funcs[id];
	func->arity = arity;
}






static elf_Bool is_binary_node(IR_Kind kind) {
	return kind >= IR_AND && kind <= IR_BIT_OR;
}


static void fpf_node(Parser *fs, FILE *io, IR_Id id) {
	IR_Node node = get_ir(fs,id);
	if (is_binary_node(node.kind)) {
		fprintf(io, "(%s ", node2s[node.kind]);
		fpf_node(fs,io,node.x);
		fprintf(io, ", ");
		fpf_node(fs,io,node.y);
		fprintf(io, ")");
	} else switch (node.kind) {
		case IR_INDEX: {
			fpf_node(fs,io,node.x);
			fprintf(io, "[");
			fpf_node(fs,io,node.y);
			fprintf(io, "]");
		} break;
		case IR_INTEGER: fprintf(io,"int(%lli)",node.i); break;
		case IR_NUMBER: fprintf(io,"num(%f)",node.n); break;
		case IR_NIL: fprintf(io,"nil"); break;
		// xx case IR_GROUP: {
		// xx 	fprintf(io,"(");
		// xx 	fpf_node(fs,io,node.x);
		// xx 	fprintf(io,")");
		// xx } break;
		default: fprintf(io,"%s",node2s[node.kind]);
	}
}

static ByteOP ir2b(IR_Kind tt);



static elf_Bool node_is_lvalue(IR_Kind kind) {
	switch (kind) {
		case IR_RANGE_INDEX:
		case IR_GLOBAL:
		case IR_LOCAL:
		case IR_INDEX:
		case IR_FIELD: {
			return 1;
		}
		default: {
			return 0;
		}
	}
}


elf_ValueTag node2tag(IR_DataTy ty) {
	switch (ty) {
		case NT_SYS: return elf_TAG_SYS;
		case NT_NUM: return elf_TAG_NUM;
		case NT_INT: return elf_TAG_INT;
		default: NO_CODE;
	}
	return elf_TAG_NIL;
}
