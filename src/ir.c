// /*
// ** See Copyright Notice In elf.h
// ** ir.c
// */

// treeT get_tree(Parser *fs, treeID id) { return fs->ir[id]; }
// treeKi get_tree_kind(Parser *fs, treeID id) { return id->kind; }
// treeTy get_tree_type(Parser *fs, treeID id) { return id->type; }
// Source get_tree_line(Parser *fs, treeID id) { return id->line; }

// treeID tree_xyz(IR_Builder *builder, Source line, treeKi kind, treeTy type, treeID x, treeID y, treeID *z) {
// 	treeID id = ARRAY_LENGTH(builder->ir);

// 	elf_debug_log("%02i IR: %s", id,node2s[kind]);

// 	ASSERT(builder->label);
// 	ASSERT(id == builder->ir_index);
// 	ASSERT(builder->label->src <= builder->label->end);
// 	ASSERT(builder->label->end == builder->ir_index);

// 	/* update current basic block */
// 	builder->label->end ++;

// 	ARRAY_GROW(builder->ir,1);

// 	treeT *ir = & builder->ir[builder->ir_index ++];
// 	ir->line = line;
// 	ir->type = type;
// 	ir->kind = kind;
// 	ir->prox = 0xffff;
// 	ir->x = x;
// 	ir->y = y;
// 	ir->z = z;
// 	return id;
// }

// treeID tree_goto(Parser *parser, Source line, int bb) {
// 	treeID id = tree_x(parser,line,IR_GOTO,NT_NON,bb);
// 	return id;
// }

// treeID tree_if(Parser *fs, Source line, treeID cond, int true_bb, int false_bb) {
// 	treeID id = tree_x(fs,line,IR_IF,NT_NON,cond);
// 	return id;
// }

// treeID tree_xy(Parser *fs, Source line, treeKi k, treeTy t, treeID x, treeID y) {
// 	return tree_xyz(fs,line,k,t,x,y,0);
// }


// treeID tree_x(Parser *fs, Source line, treeKi k, treeTy t, treeID x) {
// 	return tree_xy(fs,line,k,t,x,NO_TREE);
// }


// treeID tree_nullary(Parser *fs, Source line, treeKi k, treeTy t) {
// 	return tree_x(fs,line,k,t,NO_TREE);
// }

// treeID tree_type_guard(Parser *fs, Source line, treeID x, treeTy y) {
// 	return tree_xy(fs,line,IR_TYPEGUARD,y,x,y);
// }


// /* Todo: this is deprecated, doesn't serve any purpose, should
// only be a parse time thing... */
// // treeID tree_group(Parser *fs, Source line, treeID x) {
// // 	treeID id = tree_x(fs,line,IR_GROUP,get_tree_type(fs,x),x);
// // 	return id;
// // }


// treeID tree_int(Parser *parser, Source line, elf_Int i) {
// 	treeID v = tree_nullary(parser,line,IR_INTEGER,NT_INT);
// 	parser->ir[v].i = i;
// 	return v;
// }


// static treeID tree_num(Parser *parser, Source line, elf_Num n) {
// 	treeID v = tree_nullary(parser,line,IR_NUMBER,NT_NUM);
// 	parser->ir[v].n = n;
// 	return v;
// }


// treeID tree_str(Parser *parser, Source line, char *s) {
// 	treeID v = tree_nullary(parser,line,IR_STRING,NT_STR);
// 	parser->ir[v].s = s;
// 	return v;
// }


// treeID tree_table(Parser *fs, Source line, treeID *z) {
// 	return tree_xyz(fs,line,IR_TABLE,NT_TAB,NO_TREE,NO_TREE,z);
// }


// static treeID tree_closure(Parser *fs, Source line, treeID x, treeID *z) {
// 	return tree_xyz(fs,line,IR_CLOSURE,NT_FUN,x,NO_TREE,z);
// }


// treeID tree_nil(Parser *fs, Source line) {
// 	return tree_nullary(fs,line,IR_NIL,NT_NIL);
// }

// // Note: I guess could be the default value, is not
// // used for now...
// static treeID tree_param(Parser *parser, Source line, treeID x) {
// 	return tree_x(parser,line,IR_PARAM,NT_ANY,x);
// }

// static treeID tree_yield(Parser *parser, Source line, treeID x) {
// 	return tree_x(parser,line,IR_YIELD,NT_ANY,x);
// }

// /* Todo: deprecated */
// // notice how we were referencing the stack directly...
// treeID tree_closure_value(Parser *fs, Source line, elf_StackId x) {
// 	__debugbreak();
// 	return tree_x(fs,line,IR_CLSVAL,NT_ANY,x);
// }

// /* memory definition for a value */
// treeID tree_local(Parser *fs, Source line, treeID x) {
// 	return tree_x(fs,line,IR_LOCAL,NT_ANY,x);
// }

// treeID tree_store(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_STORE,NT_NON,x,y);
// }

// treeID tree_load(Parser *parser, Source line, treeID x) {
// 	return tree_x(parser,line,IR_LOAD,NT_ANY,x);
// }

// //todo: hack, refers directly to memory
// treeID tree_load_direct(Parser *parser, Source line, int x) {
// 	return tree_x(parser,line,IR_LOAD_DIRECT,NT_ANY,x);
// }


// static treeID tree_this_ref(Parser *fs, Source line) {
// 	return tree_load_direct(fs,line,0);
// }


// treeID tree_global_ref(Parser *fs, Source line, elf_SymbolId x) {
// 	return tree_x(fs,line,IR_GLOBAL,NT_ANY,x);
// }


// treeID tree_field(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_FIELD,NT_ANY,x,y);
// }


// treeID tree_index(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_INDEX,NT_ANY,x,y);
// }


// treeID tree_ranged_index(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_RANGE_INDEX,NT_ANY,x,y);
// }


// treeID tree_metafield(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_METAFIELD,NT_ANY,x,y);
// }


// treeID tree_call(Parser *fs, Source line, treeID x, treeID *z) {
// 	return tree_xyz(fs,line,IR_CALL,NT_ANY,x,NO_TREE,z);
// }


// treeID tree_multi(Parser *fs, Source line, treeID *z) {
// 	return tree_xyz(fs,line,IR_MULTI,NT_ANY,NO_TREE,NO_TREE,z);
// }


// treeID tree_less_than(Parser *fs, Source line, treeID x, treeID y) {
// 	return tree_xy(fs,line,IR_LT,NT_BOL,x,y);
// }


// treeID tree_eq_nil(Parser *fs, Source line, treeID x) {
// 	return tree_xy(fs,line,IR_EQ,NT_BOL,x,tree_nil(fs,line));
// }


// treeID tree_call_metafield(Parser *fs, Source line, treeID x, treeID *z, char *name) {
// 	treeID field = tree_metafield(fs,line,x,tree_str(fs,line,name));
// 	return tree_call(fs,line,field,z);
// }


// treeID tree_global_ref_by_name(Parser *fs, Source line, char *name) {
// 	elf_SymbolId x = elf_get_global(fs->M,elf_alloc_string(fs->R,name));
// 	ASSERT(x != -1);
// 	return tree_global_ref(fs,line,x);
// }


// treeID tree_call_pf(Parser *fs, Source line, treeID *args) {
// 	treeID fn = tree_global_ref_by_name(fs,line,"elf.pf");
// 	return tree_call(fs,line,fn,args);
// }


// treeID tree_call_set_metatable(Parser *fs, Source line, treeID object, treeID metatable) {
// 	treeID fn = tree_global_ref_by_name(fs,line,"elf.set_object_metatable");
// 	treeID *z = 0;
// 	ARRAY_ADD(z,object);
// 	ARRAY_ADD(z,metatable);
// 	return tree_call(fs,line,fn,z);
// }

// // xx treeID tree_basic_block(Parser *parser, Source line, int bb) {
// // xx 	return tree_x(parser,line,IR_BASIC_BLOCK,NT_NON,bb);
// // xx }
// treeID tree_push_memory_state(Parser *parser, Source src) {
// 	return tree_nullary(parser,src,IR_PUSH_MEMORY_STATE,NT_NON);
// }
// treeID tree_pop_memory_state(Parser *parser, Source src) {
// 	return tree_nullary(parser,src,IR_POP_MEMORY_STATE,NT_NON);
// }



// // todo: this doesn't require the id anymore, is just
// // the current function
// void ir_set_func_arity(IR_Builder *builder, IR_FuncId id, int arity) {
// 	ASSERT(builder->func == id);
// 	IR_Function *func = & builder->funcs[id];
// 	func->arity = arity;
// }






// static elf_Bool is_binary_node(treeKi kind) {
// 	return kind >= IR_AND && kind <= IR_BIT_OR;
// }


// static void fpf_node(Parser *fs, FILE *io, treeID id) {
// 	treeT node = get_tree(fs,id);
// 	if (is_binary_node(node.kind)) {
// 		fprintf(io, "(%s ", node2s[node.kind]);
// 		fpf_node(fs,io,node.x);
// 		fprintf(io, ", ");
// 		fpf_node(fs,io,node.y);
// 		fprintf(io, ")");
// 	} else switch (node.kind) {
// 		case IR_INDEX: {
// 			fpf_node(fs,io,node.x);
// 			fprintf(io, "[");
// 			fpf_node(fs,io,node.y);
// 			fprintf(io, "]");
// 		} break;
// 		case IR_INTEGER: fprintf(io,"int(%lli)",node.i); break;
// 		case IR_NUMBER: fprintf(io,"num(%f)",node.n); break;
// 		case IR_NIL: fprintf(io,"nil"); break;
// 		// xx case IR_GROUP: {
// 		// xx 	fprintf(io,"(");
// 		// xx 	fpf_node(fs,io,node.x);
// 		// xx 	fprintf(io,")");
// 		// xx } break;
// 		default: fprintf(io,"%s",node2s[node.kind]);
// 	}
// }

// static ByteOP ir2b(treeKi tt);



// static elf_Bool tree_is_lvalue(treeKi kind) {
// 	switch (kind) {
// 		case IR_RANGE_INDEX:
// 		case IR_GLOBAL:
// 		case IR_LOCAL:
// 		case IR_INDEX:
// 		case IR_FIELD: {
// 			return 1;
// 		}
// 		default: {
// 			return 0;
// 		}
// 	}
// }


// elf_ValueTag node2tag(treeTy ty) {
// 	switch (ty) {
// 		case NT_SYS: return elf_TAG_SYS;
// 		case NT_NUM: return elf_TAG_NUM;
// 		case NT_INT: return elf_TAG_INT;
// 		default: NO_CODE;
// 	}
// 	return elf_TAG_NIL;
// }
// #endif