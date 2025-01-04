
char *tree2s[]={
#define TREE(NAME) #NAME,
	TREEDEF(TREE)
#undef TREE
};

static int tok2tree(int tok);
static int get_tok_prec(int tok);
static treeID parse_stat2(Parser *parser);


static void tree2b(Parser *parser, treeID tree){
	int mem;
	switch(tree->kind){
		case STAT_DECL:{
			treeID value;
			treeID name;

			name=tree->stat_decl.name;
			value=tree->stat_decl.value;

			mem=tree2mem(C,0,-1,1,value);
			ASSERT(mem!=NO_SLOT);
		} break;
	}
}

int tree2mem(Parser *P, treeID id, int nreg) {
	ASSERT(reg == NO_SLOT);
	elf_State *S;
	Tree node;
	Source line;
	int reg;

	node=*id;

	switch (node.kind) {
		case IR_NIL: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			emit_bytexy(C,line,BC_LOADNIL,reg,0);
		} break;
		case IR_INTEGER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			int yy = elf_add_const_int(S,node.i);
			emit_bytexy(C,line,BC_GETKINT,reg,yy);
		} break;
		case IR_NUMBER: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			/* todo: interning */
			int yy = elf_add_const_num(S,node.n);
			emit_bytexy(C,line,BC_GETKNUM,reg,yy);
		} break;
		case IR_STRING: {
			if (nreg<1) goto esc;
			if (reg<0) reg=set_mem(C,id);

			/* todo: interning */
			int xx = elf_set_global(S->M,0,VSTR(elf_alloc_string(C->R,node.s)));
			emit_bytexy(C,line,BC_GETGLOBAL,reg,xx);
		} break;
		case EXPR_EQ: case EXPR_NEQ:
		case EXPR_GT: case EXPR_GTEQ: case EXPR_LT: case EXPR_LTEQ:
		case EXPR_DIV: case EXPR_MUL: case EXPR_MOD:
		case EXPR_SUB: case EXPR_ADD: case EXPR_POW:
		case EXPR_BIT_SHL: case EXPR_BIT_SHR:
		case EXPR_BIT_XOR:
		case EXPR_BIT_AND: case EXPR_BIT_OR: {
			int x,y;
			x=tree.expr_binary.x;
			y=tree.expr_binary.y;
			if ((node.kind==IR_GT)||(node.kind==IR_GTEQ)) {
				mem=get_mem_state(C);
				rx=to_any_mem(C,node.y);
				ry=to_any_mem(C,node.x);
				set_mem_state(C,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=set_mem(C,id);
				emit_bytexyz(C,node.line,ir2b(node.k^1),reg,rx,ry);
			} else {
				mem=get_mem_state(C);
				rx=to_any_mem(C,node.x);
				ry=to_any_mem(C,node.y);
				set_mem_state(C,mem);
				if (nreg<1) goto esc;
				if (reg<0) reg=set_mem(C,id);
				elf_debug_log("%s %i, %i, %i",node2s[node.kind], reg,rx,ry);
				emit_bytexyz(C,node.line,ir2b(node.kind),reg,rx,ry);
			}
		} break;
		case IR_CALL: {
			IR_Node xx;
			int nargs;


			mem=get_mem_state(C); {
				ASSERT(reg<mem);
				/* meta-call */
				xx=get_ir(C->P,node.x);
				if ((xx.kind==IR_FIELD)||(xx.kind==IR_METAFIELD)) {
					ry=to_mem(C,0,-1,1,xx.y);
					rx=to_mem(C,0,-1,1,xx.x);
					emit_bytexyz(C,line,ir2b(xx.kind),ry,rx,ry);
				} else {
					/* regular call with context 'this' */
					ry=to_mem(C,0,-1,1,node.x);
					rx=to_mem(C,0,-1,1,ir_this(C->P,line));
				}
				ASSERT(ry==mem+0);
				ASSERT(rx==mem+1);
				FOR_ARRAY(i,node.z) {
					rz=to_mem(C,0,-1,1,node.z[i]);
					ASSERT(rz==mem+2+i);
				}
			} set_mem_state(C,mem);

			nargs=ARRAY_LENGTH(node.z)+1;
			emit_bytexyz(C,line,BC_CALL,mem,nargs,nreg);

			if (nreg<1) goto esc;
			if (nreg>1) parser_dialog(C->P,line,"multi-returns are not supported yet!");

			if (reg<0) reg=set_mem(C,id);
			//todo: call instruction that puts
			//the result in a specific registers
			if (reg!=mem) {
				emit_bytexy(C,line,BC_RELOAD,reg,mem);
			}
		} break;
		default: {
			parser_dialog(C->P,line,"invalid node (%s)",node2s[node.kind]);
			NO_CODE;
		}
	}
	esc:
	return reg;
}



static treeID new_tree(Parser *parser, int kind, Source line) {
	elf_debug_log("NEW TREE: %s",tree2s[kind]);
	treeID tree=calloc(sizeof(Tree),1);
	tree->kind=kind;
	tree->line=line;
	return tree;
}

static treeID parse_ident(Parser *parser){
	treeID v;
	tokenT tok;

	tok=take_tok(parser,EXPR_IDENT);
	v=new_tree(parser,EXPR_IDENT,tok.line);
	v->expr_ident=tok.text;
	return v;
}

static treeID parse_fun2(Parser *parser){
	tokenT tok;
	int arity;
	treeID *params,param,body,v,enclosing;

	tok=take_tok(parser,TK_FUN);
	params=0;

	v=new_tree(parser,EXPR_FUN,tok.line);

	enclosing=parser->enclosing;
	parser->enclosing=v;

	take_tok(parser,TK_PAREN_LEFT);
	if (!test_tok(parser,TK_PAREN_RIGHT)) do {
		param=parse_ident(parser);
		ARRAY_ADD(params,param);
	} while (pick_tok(parser,TK_COMMA));

	if (!test_tok(parser,TK_PAREN_RIGHT)) {
		parser_dialog(parser,0,"did you miss a ',' ?");
	}
	take_tok(parser,TK_PAREN_RIGHT);
	pick_tok(parser,TK_QMARK);
	body=parse_stat2(parser);

	v->expr_fun.body=body;
	v->expr_fun.params=params;
	v->expr_fun.enclosing=enclosing;

	parser->enclosing=enclosing;

	return v;
}

static treeID parse_unary2(Parser *parser, bool flags) {
	tokenT tok;
	treeID v,x;

	v=NO_TREE;
	tok=parser->tok;

	switch (tok.type) {
		case TK_WORD: {
			get_tok(parser);
			v=new_tree(parser,EXPR_IDENT,tok.line);
			v->expr_ident=tok.text;
		} break;
		case TK_CURLY_LEFT: {
		} break;
		case TK_PAREN_LEFT: {
		} break;
		case TK_FUN: {
		} break;
		case TK_NIL: {
			get_tok(parser);
			v=new_tree(parser,EXPR_NIL,tok.line);
		} break;
		case TK_TRUE:{
			get_tok(parser);
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=1;
		} break;
		case TK_FALSE: {
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=0;
		} break;
		case TK_LETTER: case TK_INTEGER: {
			get_tok(parser);
			v=new_tree(parser,EXPR_INT,tok.line);
			v->expr_int=tok.integer;
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			v=new_tree(parser,EXPR_NUM,tok.line);
			v->expr_num=tok.number;
		} break;
		case TK_STRING: {
			get_tok(parser);
			v=new_tree(parser,EXPR_STR,tok.line);
			v->expr_str=tok.text;
		} break;
		default: {
			parser_dialog(parser,tok.line,"'%s': unexpected token", elf_token_intel[tok.type].name);
			elf_fail(parser->R,0,"syntax error: unexpected token");
		} break;
	}

	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}

	esc:
	return v;
}

static treeID parse_subexpr2(Parser *parser, int rank, int flags) {
	int oper,prio;
	treeID x,y;
	tokenT tok;

	x=parse_unary2(parser,flags|EXPR_ALLOW_POSTFIX);
	if (x==NO_TREE) goto esc;

	retry:
	oper=parser->tok.type;
	prio=get_tok_prec(oper);
	if (prio<=rank) goto esc;
	if (parser->tok_prox.type==TK_ASSIGN) goto esc;
	tok=get_tok(parser);
	y=parse_subexpr2(parser,prio,flags);
	if (y==NO_TREE) goto esc;

	x=new_tree(parser,tok2tree(oper),tok.line);
	x->expr_binary.x=x;
	x->expr_binary.y=y;

	goto retry;

	esc:
	return x;
}

static treeID parse_expr2(Parser *parser, int flags) {
	switch (parser->tok.type) {
		case TK_NONE:
		case TK_LET:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_TREE;
		}
	}
	return parse_subexpr2(parser,0,flags);
}

static int tok2tree(int tok) {
	switch (tok) {
		case TK_DOT_DOT: return EXPR_RANGE;
		case TK_LOG_AND: return EXPR_AND;
		case TK_LOG_OR: return EXPR_OR;
		case TK_NIL_OR: return EXPR_NIL_OR;
		case TK_NIL_AND: return EXPR_NIL_AND;
		case TK_ADD: return EXPR_ADD;
		case TK_SUB: return EXPR_SUB;
		case TK_DIV: return EXPR_DIV;
		case TK_MUL: return EXPR_MUL;
		case TK_POW: return EXPR_POW;
		case TK_MOD: return EXPR_MOD;
		case TK_NEQ: return EXPR_NEQ;
		case TK_EQ: return EXPR_EQ;
		case TK_GT: return EXPR_GT;
		case TK_GTEQ: return EXPR_GTEQ;
		case TK_LT: return EXPR_LT;
		case TK_LTEQ: return EXPR_LTEQ;
		case TK_SHL: return EXPR_BIT_SHL;
		case TK_SHR: return EXPR_BIT_SHR;
		case TK_BIT_XOR: return EXPR_BIT_XOR;
		case TK_BIT_OR: return EXPR_BIT_OR;
		case TK_BIT_AND: return EXPR_BIT_AND;
		default: return EXPR_NONE;
	}
}

static treeID parse_stat2(Parser *parser) {
	treeID v;
	tokenT tok;

	v=NO_TREE;
	tok=parser->tok;

	switch (tok.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}

	switch (tok.type) {
		case TK_CURLY_LEFT: {
			treeID *stats,stat;

			stats=0;

			get_tok(parser);
			while (!term_token(parser,TK_CURLY_RIGHT)) {
				stat=parse_stat2(parser);
				ARRAY_ADD(stats,stat);
			}
			take_tok(parser,TK_CURLY_RIGHT);

			v=new_tree(parser,STAT_BLOCK,tok.line);
			v->stat_block=stats;
		} break;
		case TK_LET: {
			get_tok(parser);
			if (test_tok(parser,TK_LET)) {
				parser_dialog(parser,parser->tok_prev.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
				parser_dialog(parser,parser->tok.line,"invalid declaration, 'let' after comma");
				elf_fail(parser->R,0,"syntax error: invalid declaration");
			}

			treeID name,value;

			name=parse_unary2(parser,0);
			take_tok(parser,TK_ASSIGN);

			value=parse_expr2(parser,0);

			v=new_tree(parser,STAT_DECL,tok.line);
			v->stat_decl.name=name;
			v->stat_decl.value=value;
		} break;
		case TK_IF: case TK_IFF: {
			ASSERT(tok.type==TK_IF);
			get_tok(parser);

			treeID pred;
			treeID true_clause;
			treeID else_clause;

			pred=parse_expr2(parser,0);
			take_tok(parser,TK_QMARK);
			true_clause=parse_stat2(parser);
			if (pick_tok(parser,TK_ELSE)) {
				else_clause=parse_stat2(parser);
			}else else_clause=NO_TREE;

			v=new_tree(parser,STAT_IF,tok.line);
			v->stat_if.true_clause=true_clause;
			v->stat_if.else_clause=else_clause;
		} break;
		default: {
			treeID x,y;
			x=v=parse_expr2(parser,0);
			if(pick_tok(parser,TK_ASSIGN)){
				y=parse_expr2(parser,0);
				v=new_tree(parser,STAT_ASSIGN,tok.line);
				v->stat_assign.x=x;
				v->stat_assign.y=y;
			}
		} break;
	}
	return v;
}