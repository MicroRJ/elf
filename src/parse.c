/*
** parse.c
** See Copyright Notice In elf.h
*/



/* Todo: should be the instruction not the register*/
void emit_continue(Parser *fs, Source line, int reg);
void emit_break(Parser *fs, Source line, elf_StackId with_value_register);

char *parser_get_name(Parser *parser) {
	return parser->filename;
}

void end_parser(Parser *parser) {
	close_function(parser);
}

int begin_parser(Parser *parser, char *filename, char *filetext) {
	if ((filename == 0) || (filetext == 0)) {
		return -1;
	}
	parser->filename   = filename;
	parser->filetext   = filetext;
	parser->thischar   = filetext;
	parser->linechar   = filetext;
	parser->linenumber = 1;

	/* kick start by lexing the first two tokens */
	get_tok(parser);
	get_tok(parser);

	begin_function(parser,parser->tok.line);
	return 1;
}


static bool check_expr(Parser *fs, Source line, IR_Id id) {
	if (id != NO_IR) return 0;
	parser_dialog(fs,line,"invalid expression");
	return 1;
}


/* Todo: use line number instead
test the current token if on the same line as the previous token */
static bool test_token_inline(Parser *parser, elf_TokenType k) {
	return parser->tok.type == k && parser->tok_prev.eol != 1;
}


static bool test_tok(Parser *parser, elf_TokenType k) {
	return parser->tok.type == k;
}

/* whether there are no more tokens or whether the
current token is a match. */
static bool term_token(Parser *parser, elf_TokenType k) {
	return parser->tok.type == TK_NONE || parser->tok.type == k;
}

static bool term_eol_token(Parser *parser) {
	return parser->tok.type == TK_NONE || parser->tok_prev.eol == 1;
}

static bool pick_tok(Parser *parser, elf_TokenType k) {
	return test_tok(parser,k) && (get_tok(parser), 1);
}


static bool pick_token_inline(Parser *parser, elf_TokenType k) {
	return test_token_inline(parser,k) && (get_tok(parser), 1);
}


static tokenT take_tok(Parser *fs, int k) {
	tokenT tk = fs->tk;
	if (!pick_tok(fs,k)) {
		parser_dialog(fs,fs->tok.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tk;
}


static tokenT get_token_inline(Parser *fs, int k) {
	tokenT tok = fs->tok;
	if (!pick_token_inline(fs,k)) {
		parser_dialog(fs,tok.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tok;
}


/* looks for an enclosed entity within the function,
and returns the index where the entity, the index
can then be used to emit instructions. */
static int get_closure_value_index(IR_Function *fn, EntityIdGuard id) {
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) {
			return i;
		}
	}
	return NO_SLOT;
}


/* encloses an entity within the given function. */
static void enclose_entity(Parser *fs, IR_Function *fn, EntityIdGuard id) {
	/* ensure the entity should actually be captured */
	// ASSERT(id.id < fn->entities);
	/* check whether the entity was already captured */
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) return;
	}
	ARRAY_ADD(fn->enclosure,id.id);
}

/* find the last declared entity for the given register within the current function only! */
// xx static int find_local_entity(Parser *fs, int args) {
// xx 	int id;
// xx 	for (id = fs->nentities-1; id >= fs->fn->entities; id-=1) {
// xx 		if ((fs->entities[id].kind==ENTITY_LOCAL) && (fs->entities[id].args==args)) {
// xx 			return id;
// xx 		}
// xx 	}
// xx 	return -1;
// xx }


/* Finds the last declared entity with the given name. */
static EntityId get_entity(Parser *parser, char *name, bool enclose) {

	// IR_Function *fn = parser->fn;

	/* we're using a linear search method here, but I can guarantee
	that this is actually fast enough, introducing table will not
	be profitable for a long while...
	^ That was 2024 me... */
	EntityId id;
	for (id = parser->nentities-1; id > -1; -- id) {
		if (text_eq(parser->entities[id].name,name)) {
			// this little enclose thing is to add support
			// for closures, so whenever a reference to some entity
			// outside of this function's scope happens, the parser
			// will know, and it will add it to a a list of enclosed
			// or captured entities, later when the closure is instantiated,
			// you pass in a list of these things.
			// I've actually never had a reason to use closures, and honestly
			// it just adds a good amount of runtime overhead, so I might
			// just get rid of this...
			// But probably far from now.

			// if ((id < fn->entities) && (enclose)) {
			// 	enclose_entity(parser,fn,(EntityIdGuard){id});
			// }

			// Todo: so here we're adding this flag, but do we know that
			// this was the script itself referencing it, or us?
			// Because if its always the script, then why pass in the
			// enclose flag?
			parser->entities[id].flags |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}


/* binds something to a name within the current scope,
the result is the entity id.
I don't think this necessarily has to be an IR_Id, because
we can also use this for more syntactic things, like maybe
constants, which are parse time evaluated... although, maybe
this should be code generator's job, either, this remains
fairly open ended for now ... */
static EntityId parser_bind(Parser *parser, Source line, int flags, char *name, int args) {
	// so to the entity system seems like it would work with the
	// newer stuff

	// IR_Function *fn = parser->fn;

	// You also notice how my coding style has changed, like before
	// I didn't add any spacing in between things, and I pre-declared
	// things....
	EntityId entity_id = get_entity(parser,name,0);
	if (entity_id != NO_ENTITY) {
		FileEntity entity = parser->entities[entity_id];
		// Symbol directories were a pretty interesting idea,
		// but that I've used the programming language more,
		// I've noticed that perhaps they are not as convenient
		// I thought.
		if (entity.kind == ENTITY_DIRECTORY)  {
			parser_dialog(parser,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.level == parser->block_index) {
			/* reassignment */
			// don't know why this was commented before...
			parser_dialog(parser,line,"'%s': is already declared",name);
		} else {

			__debugbreak();
			// if (entity_id >= fn->entities) {
				// /* only issue this warning if the entity we found
				// is within this function... */
				// parser_dialog(parser,line,"'%s': this declaration shadows another one",name);
			// }

		}
	}

	EntityId id = parser->nentities ++;
	ARRAY_GROW(parser->entities,parser->nentities-ARRAY_LENGTH(parser->entities));

	// this should be nblocks-1, it doesn't really matter...
	parser->entities[id].kind  = ENTITY_LOCAL;
	parser->entities[id].flags = flags;
	parser->entities[id].args  = args;
	parser->entities[id].line  = line;
	parser->entities[id].name  = name;
	parser->entities[id].level = parser->block_index;
	return id;
}


static IR_Id find_name(Parser *F, Source line, int flags, char *name) {
	EntityId id = get_entity(F,name,1);
	if (id != NO_ENTITY) {
		if (flags) {
			if (~F->entities[id].flags & ENTITY_ASSIGNED) {
				parser_dialog(F,line,"warning: usage of possibly unassigned variable");
			}
		}
		// IR_Function *fn = F->fn;
		// if (id < fn->entities) {
			// if (id < fn->enclosing->entities) {
				// parser_dialog(F,line,"too many layers for closure");
			// }
			// return node_closure_value(F,line,get_closure_value_index(fn,ENTITY(id)));
		// } else
		{
			return node_load(F,line,F->entities[id].args);
		}
	} else {
		/* todo: check whether we've assigned a value
		to this entity already, otherwise issue a warning that
		we're using something that hasn't got a value yet... */
		return node_global_name(F,line,name);
	}
}

#include "tree.c"



void begin_scope(Parser *parser) {
	parser->scope_stack[parser->scope_index ++] = parser->scope;
	parser->scope = parser->nentities;
}

void close_scope(Parser *parser) {
	parser->scope = parser->scope_stack[-- parser->scope_index];
}


// We can just repurpose these
void parser_begin_block(Parser *parser, int flags) {
	IR_Id ir = node_push_memory_state(parser,parser->tok.line);
	ir_add_prox(parser,ir);

	/* Todo: get rid of this? */
	parser->block_stack[parser->block_index ++] = parser->block;
	parser->block = (FileBlock){};
	parser->block.entry = ir_get_label(parser);

	begin_scope(parser);
	#if 0
	bl->loop.array_register = NO_SLOT;
	bl->loop.index_register = NO_SLOT;
	bl->loop.value_register = NO_SLOT;
	bl->level = level;
	bl->xmemory = fs->fn->xmemory;
	bl->xentity = fs->nentities;
	bl->xnode = fs->nnodes;
	bl->flags = flags;
	bl->entry = get_instr_cursor(fs);
	bl->jumpover = bl->entry;
	fs->nloops += (flags & BLOCK_LOOP) != 0;
	return level;
	#endif
}


void parser_close_block(Parser *parser) {
	close_scope(parser);
	parser->block = parser->block_stack[-- parser->block_index];

	Source line = parser->tok.line;
	IR_Id ir = node_pop_memory_state(parser,line);
	ir_add_prox(parser,ir);


#if 0
	// xx FileBlock block = parser->block;
	// xx return ir_add_basic_block(parser,line,block.entry,ir_get_label(parser));

	ASSERT(fs->nentities >= fs->fn->entities);
	FileBlock *bl = get_block(fs,-1);
	EntityId id;
	/* xentity is the first entity within a block, if any. */
	for (id = bl->xentity; id < fs->nentities; ++ id) {
		if (~fs->entities[id].flags & ENTITY_REFERENCED) {
			parser_dialog(fs,fs->entities[id].line,"'%s': unreferenced entity", fs->entities[id].name);
		}
	}
	fs->nentities = bl->xentity;
	fs->nnodes    = bl->xnode;
	fs->nblocks  -= 1;
	// ASSERT(bl->level == fs->level);
	// fs->fn->block = bl->enclosing;
	fs->fn->xmemory = bl->xmemory;
	if (bl->leavejumps != 0) {
		patch_jumps(fs,bl->leavejumps);
		ARRAY_DELETE(bl->leavejumps);
		bl->leavejumps = 0;
	}
	fs->nloops -= (bl->flags & BLOCK_LOOP) != 0;
#endif
}

static IR_FuncId begin_function(Parser *parser, Source line) {
	IR_FuncId id = ir_begin_func(parser,line);

	ir_set_func_arity(parser,id,1);

	IR_Id ir = ir_param(parser,line,NO_IR);
	ir_add_prox(parser,ir);

	parser_bind(parser, line
	, ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED
	, "this", ir);
	return id;
}


static void close_function(Parser *parser) {
	ir_close_func(parser);

	// IR_Module *ir_module = parser->ir_module;
	// IR_Function *func = & ir_module->funcs[parser->func];
	// parser->func = func->enclosing;

	// Source line = parser->tok.line;
	// this handled in the code generator?
	// xxx patch_jumps(parser,func->yield_jumps);
	// xxx ARRAY_DELETE(func->yield_jumps);
	// xxx func->yield_jumps = 0;

	// emit_bytex_deprecated(parser,line,BC_LEAVE,0);


	// ASSERT(func->entry_block == parser->nblocks);
	// ASSERT(parser->nentities == func->entities);
}


/* {x} | ( x { ... } ) | { <table-initializer-list> } */
static IR_Id *parse_call_args(Parser *fs) {
	IR_Id x;
	IR_Id *z,*n;
	z=0;
	if (test_tok(fs,TK_CURLY_LEFT)) {
		x=parse_table(fs);
		ARRAY_ADD(z,x);
	} else if (pick_tok(fs,TK_PAREN_LEFT)) {
		if (!test_tok(fs,TK_PAREN_RIGHT)) do {
			x=parse_expr(fs,0,0);
			if (x!=NO_IR) {
				if (get_ir_kind(fs,x)==IR_MULTI) {
					n=get_ir(fs,x).z;
					FOR_ARRAY(i,n){
						ARRAY_ADD(z,n[i]);
					}
				} else ARRAY_ADD(z,x);
			} else break;
		} while (pick_tok(fs,TK_COMMA));
		take_tok(fs,TK_PAREN_RIGHT);
	} else {
		x=parse_expr(fs,0,0);
		if (x!=NO_IR)ARRAY_ADD(z,x);
	}
	return z;
}


static elf_TokenType word2tok(tokenT tk) {
	/* todo: meh... remove this */
	if (tk.type != TK_WORD) return tk.type;
	if (!strcmp(tk.text,"and")) return TK_LOG_AND;
	if (!strcmp(tk.text,"or"))  return TK_LOG_OR;
	if (!strcmp(tk.text,"is"))  return TK_EQ;
	return TK_WORD;
}


static int get_tok_prec(elf_TokenType type) {
	return elf_token_intel[type].prec;
}


static IR_Kind tok2node(elf_TokenType tk) {
	switch (tk) {
		case TK_DOT_DOT: return IR_RANGE;
		case TK_LOG_AND: return IR_AND;
		case TK_LOG_OR: return IR_OR;
		case TK_NIL_OR: return IR_NIL_OR;
		case TK_NIL_AND: return IR_NIL_AND;
		case TK_ADD: return IR_ADD;
		case TK_SUB: return IR_SUB;
		case TK_DIV: return IR_DIV;
		case TK_MUL: return IR_MUL;
		case TK_POW: return IR_POW;
		case TK_MOD: return IR_MOD;
		case TK_NEQ: return IR_NEQ;
		case TK_EQ: return IR_EQ;
		case TK_GT: return IR_GT;
		case TK_GTEQ: return IR_GTEQ;
		case TK_LT: return IR_LT;
		case TK_LTEQ: return IR_LTEQ;
		case TK_SHL: return IR_BIT_SHL;
		case TK_SHR: return IR_BIT_SHR;
		case TK_BIT_XOR: return IR_BIT_XOR;
		case TK_BIT_OR: return IR_BIT_OR;
		case TK_BIT_AND: return IR_BIT_AND;
		default: return IR_NONE;
	}
}


IR_Id parse_subexpr(Parser *fs, BooleanJumps *expr, int rank, int flags) {
	int oper,prio;
	IR_Id x,y;
	tokenT tk;

	x=parse_unary(fs,expr,flags|EXPR_ALLOW_POSTFIX);
	if (x==NO_IR) goto esc;

	retry:
	oper=word2tok(fs->tok);
	prio=get_tok_prec(oper);
	if (prio<=rank) goto esc;
	if (fs->tok_prox.type==TK_ASSIGN) goto esc;
	tk=get_tok(fs);
	y=parse_subexpr(fs,0,prio,flags);
	if (y==NO_IR) goto esc;
	x=node_xy(fs,tk.line,tok2node(oper),NT_ANY,x,y);
	goto retry;

	esc:
	return x;
}

static IR_Id parse_function(Parser *parser) {
	tokenT tok = take_tok(parser,TK_FUN);
	IR_FuncId fun = begin_function(parser,tok.line);

	int arity = 1;

	take_tok(parser,TK_PAREN_LEFT);
	if (!test_tok(parser,TK_PAREN_RIGHT)) do {
		/* todo: default values would be pretty easy to add, maybe? */

		tokenT name = take_tok(parser,TK_WORD);

		IR_Id param = ir_param(parser,name.line,NO_IR);
		ir_add_prox(parser,param);

		parser_bind(parser,name.line,ENTITY_PARAMETER|ENTITY_ASSIGNED,name.text,param);

		arity += 1;
	} while (pick_tok(parser,TK_COMMA));

	ir_set_func_arity(parser,fun,arity);

	if (!test_tok(parser,TK_PAREN_RIGHT)) {
		parser_dialog(parser,0,"did you miss a ','?");
	}
	take_tok(parser,TK_PAREN_RIGHT);

	/* '?' are now optional */
	pick_tok(parser,TK_QMARK);

	if (test_tok(parser,TK_CURLY_LEFT)) {
		take_tok(parser,TK_CURLY_LEFT);

		while (parse_stat(parser)) {
			tok = parser->tok;
		}
		/* By default we emit a 'leave this' instruction,
		because this tends to be more convenient... */

		/* todo: only emit the yield if this block
		wasn't terminated by another leave instruction,
		otherwise this is wasteful
		Todo: I was under the impression that 'this' was
		just the default return because it was the first
		register, but I guess not...
		Maybe we can make it the be in the first register... */
		// emit_yield(parser,tok.line,ir_this(parser,tok.line));

		ir_yield(parser,tok.line,ir_this(parser,tok.line));

		take_tok(parser,TK_CURLY_RIGHT);
	} else {

		// emit_yield(parser,tok.line,parse_expr(parser,0,0));
		ir_yield(parser,tok.line,parse_expr(parser,0,0));
	}

	close_function(parser);

	// No longer do this
	// patch_jump(parser,fj);

	/* create a new prototype and add this function
	to the list of prototypes */
	#if 0
	elf_Prototype fp = {0};
	fp.arity	  = arity;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = parser->M->nbytes - fn.bytes;
	int f = elf_add_function(parser->M,fp);
	#endif

	// will remove this temporarily
	#if 0
	IR_Id *z = 0;
	FOR_ARRAY(i,fn.enclosure) {
		FileEntity entity = parser->entities[fn.enclosure[i]];
		ARRAY_ADD(z,node_load(parser,entity.line,entity.args));
	}
	#endif

	// now IR instruction use IR functions....
	return ir_new_closure(parser,tok.line,fun,0);
}

// todo: deprecated
#if 0
void parse_assign(Parser *fs, IR_Id lexpr) {
	if (lexpr == NO_IR) {
		return;
	}

	IR_Id x,y;
	int mem;
	tokenT tk,op;

	tk=fs->tk;
	mem=get_mem_state_deprecated(fs);
	x=desugar_range_expr(fs,lexpr,0);
	x=emit_preload_deprecated(fs,x);
	if (pick_tok(fs,TK_ASSIGN)) {
		y=parse_expr(fs,0,0);
		emit_store_deprecated(fs,tk.line,x,y);
	} else if (pick_tok(fs,TK_NIL_ASSIGN)) {
		BooleanJumps js = {0};
		y=parse_expr(fs,0,0);
		emit_jump_if_not_nil(fs,tk.line,&js,x);
		emit_store_deprecated(fs,tk.line,x,y);
		patch_jumps(fs,js.f);
	} else {
		if (get_tok_prec(tk.type) > 0) {
			op=get_tok(fs);
			take_tok(fs,TK_ASSIGN);
			/* todo: optimization! */
			y=parse_expr(fs,0,0);
			y=node_xy(fs,op.line,tok2node(op.type),get_ir_type(fs,x),x,y);
			emit_store_deprecated(fs,op.line,x,y);
		} else {
			emit_eval_deprecated(fs,0,-1,0,lexpr);
		}
	}
	desugar_range_expr_epilogue(fs,lexpr);
	set_mem_state_deprecated(fs,mem);
}
#endif


IR_Id parse_table(Parser *parser) {
	IR_Id table,key,field,store,value;
	tokenT token;
	int index;
	// xx *args

	token=take_tok(parser,TK_CURLY_LEFT);

	table=node_new_table(parser,token.line,0);
	table=node_local(parser,token.line,table);
	ir_add_prox(parser,table);
	// xx args=0;
	index=0;
	token=parser->tok;


	for (;(token.type!=TK_NONE)&&(token.type!=TK_CURLY_RIGHT);token=parser->tok) {
		ir_add_prox(parser,node_push_memory_state(parser,token.line));

		value=NO_IR;
		if ((token.type==TK_WORD)&&(parser->tok_prox.type==TK_ASSIGN)) {
			token=get_tok(parser);
			key=node_str(parser,token.line,token.text);
		} else {
			key=value=parse_expr(parser,0,0);
		}
		token=parser->tok;
		if (pick_tok(parser,TK_ASSIGN)) {
			value=parse_expr(parser,0,0);
		} else {
			key=node_int(parser,token.line,index++);
		}

		check_expr(parser,token.line,key);
		check_expr(parser,token.line,value);
		token=parser->tok;
		field=node_field(parser,token.line,table,key);

		store=node_store(parser,token.line,field,value);
		ir_add_prox(parser,store);

		// xx ARRAY_ADD(args,store);
		ir_add_prox(parser,node_pop_memory_state(parser,token.line));

		if (pick_tok(parser,TK_COMMA)) {
			continue;
		}
	}
	take_tok(parser,TK_CURLY_RIGHT);

	// xx parser->ir[table].z = args;

	return table;
}


IR_Id parse_expr(Parser *fs, BooleanJumps *expr, int flags) {
	switch (fs->tok.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_IR;
		}
	}
	return parse_subexpr(fs,expr,0,flags);
}


/* todo: add support for:
specifing which for loop you're reffering to. */
#if 0
elf_StackId target_value_register = NO_SLOT;
if (!term_eol_token(fs)) {
	IR_Id value = parse_expr(fs,0);
	if (value != NO_IR) {
		target_value_register = get_irreg_deprecated(fs,NODE(value));
		if (target_value_register < 0) {
			parser_dialog(fs,get_ir_line(fs,value),"invalid value");
		}
	}
}
#endif

static IR_Id parse_unary(Parser *parser, BooleanJumps *expr, bool flags) {
	IR_Id v;
	tokenT tk;
	IR_Id x;

	v=NO_IR;
	tk=parser->tok;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			get_tok(parser);
			int reg;

			if (tk.type==TK_M_ARRAY) reg=SPECIAL_REGISTER_ARRAY; else
			if (tk.type==TK_M_VALUE) reg=SPECIAL_REGISTER_VALUE; else reg=SPECIAL_REGISTER_INDEX;

			v=node_load(parser,tk.line,reg);
		} break;
		/* todo: make this an intrinsic instruction! */
		case TK_M_INT: case TK_M_NUM: {
			char *name;
			IR_Id fn, *z=0;

			get_tok(parser);
			x=parse_unary(parser,0,flags|EXPR_ALLOW_POSTFIX);
			name=tk.type==TK_M_INT?"ntoi":"iton";
			fn=node_global_name(parser,tk.line,name);
			ARRAY_ADD(z,x);
			v=node_call(parser,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: {
			get_tok(parser);
			tk=take_tok(parser,TK_WORD);
			x=get_entity(parser,tk.text,0);
			if (x!=NO_ENTITY) {
				v=node_int(parser,tk.line,parser->entities[x].args);
			} else parser_dialog(parser,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			get_tok(parser);
			IR_Id *call_args = parse_call_args(parser);

			IR_Id load_file_func = node_global_name(parser,tk.line,"elf.loadfile");
			IR_Id call_load_file = node_call(parser,tk.line,load_file_func,call_args);
			v = call_load_file;
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.set_object_metatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			get_tok(parser);
			IR_Id meta_field_name, get_meta_field, call_new
			, meta_table, *call_args, table;

			meta_table=parse_unary(parser,0,0);
			call_args=parse_call_args(parser);

			/* so if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (get_ir_kind(parser,call_args[0]) == IR_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = node_new_table(parser,tk.line,0);
			}

			table=node_call_set_metatable(parser,tk.line,table,meta_table);

			meta_field_name=node_str(parser,tk.line,"__new");
			get_meta_field=node_metafield(parser,tk.line,table,meta_field_name);
			call_new=node_call(parser,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* empty ranges are allowed and interpreted as min...max + 1 */
		case TK_DOT_DOT: {
			get_tok(parser);
			v=node_xy(parser,tk.line,IR_RANGE,NT_ANY,NO_IR,NO_IR);
		} break;
		/* todo: this is temporary */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (pick_tok(parser,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!test_token_inline(parser,TK_DOT)) {
					parser_dialog(parser,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}

			take_tok(parser,TK_DOT);
			do {
				get_token_inline(parser,TK_WORD);
				strcat(dir,".");
				strcat(dir,parser->tok_prev.text);
			} while (pick_token_inline(parser,TK_DOT));
			int x;
			x=elf_get_global(parser->M,elf_alloc_string(parser->R,dir));
			v=node_global(parser,tk.line,x);
		} break;
		case TK_WORD: {
			get_tok(parser);
			v=find_name(parser,tk.line,flags,tk.text);
		} break;
		case TK_SUB: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000,flags);
			v=node_xy(parser,tk.line,IR_SUB,NT_INT,node_int(parser,tk.line,0),v);
		} break;
		case TK_ADD: {
			get_tok(parser);
			v=parse_subexpr(parser,0,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v=parse_table(parser);
		} break;
		case TK_PAREN_LEFT: {
			get_tok(parser);
			v=parse_expr(parser,0,EXPR_ALLOW_POSTFIX);
			take_tok(parser,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v!=NO_IR) {
				// xx v=node_group(parser,tk.line,v);
				// xx ASSERT(v != NO_IR);
			}
		} break;
		case TK_FUN: {
			v=parse_function(parser);
		} break;
		case TK_NIL: {
			get_tok(parser);
			v=node_nil(parser,tk.line);
		} break;
		case TK_TRUE: case TK_FALSE: {
			get_tok(parser);
			v=node_int(parser,tk.line,tk.type==TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			get_tok(parser);
			v=node_int(parser,tk.line,tk.integer);
		} break;
		case TK_NUMBER: {
			get_tok(parser);
			v=ir_number(parser,tk.line,tk.number);
		} break;
		case TK_STRING: {
			get_tok(parser);
			v=node_str(parser,tk.line,tk.text);
		} break;
		case TK_DEFAULT: {
			parser_dialog(parser,tk.line,"syntax error: default expressions can only be top level");
			elf_fail(parser->R,0,"syntax error: default expressions can only be top level");
		} break;
		default: {
			parser_dialog(parser,tk.line,"'%s': unexpected token", elf_token_intel[tk.type].name);
			elf_fail(parser->R,0,"syntax error: unexpected token");
		} break;
	}

	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}

	/* ensure we don't parse a postfix past a line */
	while (!term_eol_token(parser)) {
		tk = parser->tk;

		switch (tk.type) {
			case TK_DOT: {
				get_tok(parser);
				// table.(x,y) -> (table.x, table.y)
				if (pick_tok(parser,TK_PAREN_LEFT)) {
					IR_Id *z = {0};
					do {
						tokenT n = take_tok(parser,TK_WORD);
						IR_Id x,y;
						y=node_str(parser,n.line,n.text);
						x=node_field(parser,tk.line,v,y);
						ARRAY_ADD(z,x);
					} while (pick_tok(parser,TK_COMMA));
					v = node_multi(parser,tk.line,z);
					take_tok(parser,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (pick_tok(parser,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					tokenT name;
					IR_Id field;
					name=take_tok(parser,TK_WORD);
					field=node_str(parser,name.line,name.text);
					v=node_field(parser,tk.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil  */
			case TK_SQUARE_LEFT: {
				take_tok(parser,TK_SQUARE_LEFT);
				IR_Id *z;
				IR_Id index;
				do {
					index=parse_expr(parser,0,0);
					if (index==NO_IR) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (get_ir_kind(parser,index)==IR_MULTI) {
						z=get_ir(parser,index).z;
						FOR_ARRAY(i,z) {
							v=node_index(parser,tk.line,v,z[i]);
						}
					} else if (get_ir_kind(parser,index)==IR_RANGE) {
						v=node_ranged_index(parser,tk.line,v,index);
					} else {
						v=node_index(parser,tk.line,v,index);
					}

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(pick_tok(parser,TK_COMMA));
				take_tok(parser,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				tokenT n;
				IR_Id y;
				get_tok(parser);
				n=take_tok(parser,TK_WORD);
				y=node_str(parser,n.line,n.text);
				v=node_metafield(parser,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				IR_Id *z;
				z=parse_call_args(parser);
				v=node_call(parser,tk.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:
	return v;
}

static IR_Id *parse_expr_list(Parser *fs) {
	IR_Id y,*yz,*z=0;
	do {
		y=parse_expr(fs,0,0);
		if(get_ir_kind(fs,y)==IR_MULTI) {
			yz=get_ir(fs,y).z;
			FOR_ARRAY(i,yz) ARRAY_ADD(z,yz[i]);
		} else {
			ARRAY_ADD(z,y);
		}
	} while (pick_tok(fs,TK_COMMA));
	return z;
}


void parse_for_loop(Parser *fs) {
	__debugbreak();
#if 0
	tokenT tk,name;
	IR_Id y,*z,value,array,index,lo,hi;
	int block,block_head=NO_BYTE,block_tail=NO_BYTE;
	int value_register,array_register,index_register;

	tk=take_tok(fs,TK_FOR);
	name=take_tok(fs,TK_WORD);
	take_tok(fs,TK_ASSIGN);
	z=parse_expr_list(fs);
	take_tok(fs,TK_QMARK);

	block = parser_begin_block(fs,BLOCK_LOOP);
	value_register = reg_alloc_deprecated(fs);
	value = node_load(fs,name.line,value_register);

	/* todo: remove REFERENCED, instead allow the user to not have to specify the name */
	parser_bind(fs,name.line,ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
	, name.text,value_register);

	FOR_ARRAY(i,z){
		y=z[i];
		array=NO_IR;
		array_register=NO_SLOT;
		index=NO_IR;
		index_register=NO_SLOT;

		if (get_ir_kind(fs,y)==IR_RANGE_INDEX) {
			array=get_ir(fs,y).x;
			array_register=compiler_ir2anyreg(fs,array);

			y=get_ir(fs,y).y;
			ASSERT(get_ir_kind(fs,y)==IR_RANGE);
		}
		if (get_ir_kind(fs,y)==IR_RANGE) {
			lo=get_ir(fs,y).x;
			hi=get_ir(fs,y).y;
			if (array==NO_IR) {
				index=value;
				// for ... ? { }
				if (lo==NO_IR)lo=node_int(fs,tk.line,0);
				if (hi==NO_IR)hi=node_int(fs,tk.line,-1);
			} else {
				// for array[...] ? { }
				if (lo==NO_IR)lo=node_int(fs,tk.line,0);
				if (hi==NO_IR)hi=node_call_metafield(fs,tk.line,array,0,"length");
				/* todo: we're allocating this here, and never freeing it! */
				index=node_load(fs,tk.line,reg_alloc_deprecated(fs));
			}

			begin_range_loop(fs,tk.line,index,lo,hi);

			get_block(fs,block)->loop.value_register=value_register;
			get_block(fs,block)->loop.array_register=array_register;

			if (array!=NO_IR) {
				IR_Id *z = {0};
				ARRAY_ADD(z,index);
				emit_store_deprecated(fs,name.line,value,node_call_metafield(fs,tk.line,array,z,"idx"));
			}
			/* todo: could be neater */
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail) {
					emit_byte_deprecated(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
				}
			}
			close_range_loop(fs,tk.line);
		} else {
			emit_store_deprecated(fs,tk.line,value,y);
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail){
					emit_byte_deprecated(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
				}
			}
			/* because we are within a loop
			the user can use "continue" and
			"break", continues are the ones
			we need to handle here, which
			mean move on to the next step. */
			FileBlock *loop;

			loop=get_block(fs,block);
			patch_jumps(fs,loop->loop.true_jumps);
			ARRAY_DELETE(loop->loop.true_jumps);
			loop->loop.true_jumps = 0;
		}
	}
	parser_close_block(fs);
#endif
}

void parse_block(Parser *parser) {
	parser_begin_block(parser,0);
	if (pick_tok(parser,TK_CURLY_LEFT)) {
		while (!term_token(parser,TK_CURLY_RIGHT)) {
			parse_stat(parser);
		}
		take_tok(parser,TK_CURLY_RIGHT);
	} else parse_stat(parser);
	parser_close_block(parser);
}

int parse_stat(Parser *parser) {
	return (int) parse_stat2(parser);
}


#if 0
int parse_stat(Parser *parser) {
	IR_Id ir = NO_IR;
	tokenT tok = parser->tok;
	switch (tok.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}


	// xxx FileBlock *bl = get_block(parser,-1);
	// xxx if (bl->flags & BLOCK_ENDED) {
	// xxx 	parser_dialog(parser,tok.line,"warning: unreachable statement");
	// xxx }

	switch (tok.type) {
		/* todo: go back to := */
		case TK_LET: {
			get_tok(parser);
			do {
				if (test_tok(parser,TK_LET)) {
					parser_dialog(parser,parser->tok_prev.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					parser_dialog(parser,parser->tok.line,"invalid declaration, 'let' after comma");
					elf_fail(parser->R,0,"syntax error: invalid declaration");
				}

				tokenT name = take_tok(parser,TK_WORD);
				take_tok(parser,TK_ASSIGN);

				IR_Id x = parse_expr(parser,0,0);

				ir = node_local(parser,name.line,x);
				ir_add_prox(parser,ir);

				parser_bind(parser,name.line,0,name.text,ir);

				// xxx parse_assign(parser,node_load(parser,name.line,reg));

			} while (pick_tok(parser,TK_COMMA));
		} break;
		case TK_LASTLY: case TK_FINALLY: {
			__debugbreak();
			#if 0
			get_tok(parser);
			if (tk.type == TK_FINALLY) {
				parser_dialog(parser,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			begin_delay_block(parser,tk.line);
			parse_stat(parser);
			close_delay_block(parser,tk.line);
			ASSERT(get_mem_state_deprecated(parser)==mem);
			#endif
		} break;
		case TK_IF: case TK_IFF: {
			ASSERT(tok.type == TK_IF);

			get_tok(parser);

			IR_Id x;

			x=parse_expr(parser,0,0);
			take_tok(parser,TK_QMARK);
			// xxx BranchJumps s = {0};
			// xxx begin_if(parser,tk.line,&s,x,L_IF);
			//note: if node has to be created prior
			//to its labels to assume control
			ir=node_if(parser,tok.line,x,-1,-1);
			ir_add_prox(parser,ir);

			int t,f;
			t=ir_start_label(parser,"IF.T");
			parse_block(parser);
			f=ir_start_label(parser,"IF.F");
			// parse_block(parser);
			ir_start_label(parser,"RESUME");

			parser->prev=ir;
			parser->ir[ir].prox=0xffff;
			ARRAY_ADD(parser->ir[ir].z,t);
			ARRAY_ADD(parser->ir[ir].z,f);





#if 0
			while (!test_tok(parser,TK_NONE)) {
				if (pick_tok(parser,TK_ELIF)) {
					parser_begin_block(parser,0);

					x = parse_expr(parser,0,0);
					take_tok(parser,TK_QMARK);

					// xxx add_elif_clause(parser,parser->tok_prev.line,&s,x);
					// xxx parse_stat(parser);

					parser_close_block(parser);
				} else if (pick_tok(parser,TK_THEN)) {
					parser_begin_block(parser,0);

					// xx add_then_clause(parser,parser->tok_prev.line,&s);
					// xx parse_stat(parser);

					parser_close_block(parser);
				} else if (pick_tok(parser,TK_ELSE)) {
					parser_begin_block(parser,0);

					// xxx add_else_clause(parser,parser->tok_prev.line,&s);
					// xxx parse_stat(parser);

					parser_close_block(parser);
				} else break;
			}
#endif
			// xxx close_if(parser,parser->tok_prev.line,&s);
		} break;
		case TK_LEAVE: {
			get_tok(parser);
			IR_Id x = parse_expr(parser,0,0);

			// emit_yield(parser,tk.line,x);

		} break;

#if 0
		case TK_BREAK: case TK_CONTINUE: {
			int reg = NO_IR;
			get_tok(parser);
			/* Todo: defer til code generation */
			if (!term_eol_token(parser)) {
				IR_Id value = parse_unary(parser,0,0);
				if (value == NO_IR) {
					if ((reg=get_irreg_deprecated(parser,NODE(value)))<0) {
						parser_dialog(parser,get_ir_line(parser,value),"invalid value");
					}
				}
			}
			if (tk.type==TK_CONTINUE) emit_continue(parser,tk.line,reg);
			else emit_break(parser,tk.line,reg);
		} break;
		case TK_WHILE: {
			get_tok(parser);
			parser_begin_block(parser,BLOCK_LOOP);
			x=parse_expr(parser,0,0);
			take_tok(parser,TK_QMARK);
			begin_while_loop(parser,x);
			parse_stat(parser);
			close_while_loop(parser);
			parser_close_block(parser);
		} break;
		case TK_DO: {
			get_tok(parser);
			parser_begin_block(parser,BLOCK_LOOP);
			begin_do_while_loop(parser,tk.line);
			parse_stat(parser);
			take_tok(parser,TK_WHILE);
			x=parse_expr(parser,0,0);
			close_do_while_loop(parser,tk.line,x);
			parser_close_block(parser);
		} break;
		/* parse a block */
		case TK_CURLY_LEFT: {
			parser_begin_block(parser,0);
			get_tok(parser);
			while (!term_token(parser,TK_CURLY_RIGHT)) {
				parse_stat(parser);
			}
			take_tok(parser,TK_CURLY_RIGHT);
			x = parser_close_block(parser);
		} break;
		case TK_FOR: {
			parse_for_loop(parser);
		} break;
#endif
		default: {
#if 0
			EntityId entity;
			int reg;
			if (parser->nblocks>1 && parser->tok_prox.type==TK_ASSIGN){
				get_tok(parser);
				entity=get_entity(parser,tk.text,0);
				if (entity==-1){
					parser_dialog(parser,tk.line,"new implicit entity: %s", tk.text);
					reg=reg_alloc_deprecated(parser);
					entity=parser_bind(parser,tk.line,0,tk.text,reg);
				} else {
					reg=parser->entities[entity].args;
				}
				x=node_load(parser,tk.line,reg);
			} else {
				x=parse_expr(parser,0,0);
			}
#endif
			ir_add_prox(parser,node_push_memory_state(parser,tok.line));
			ir = parse_expr(parser,0,0);
			if (ir != NO_IR) {
				ir_add_prox(parser,ir);
				// xx parse_assign(parser,x);
			} else {
				parser_dialog(parser,tok.line,"invalid statement");
			}
			ir_add_prox(parser,node_pop_memory_state(parser,tok.line));
		} break;
	}


	return 1;
}
#endif


#if 0

//
//	I think if statements don't have to be IR, because it wouldn't
// really matter, and 'if' is just a conditional jump, so we can
// translate that directly from code to lower level IR.
//
// yield has to be IR, it's literally an instruction.
//
// break / continue, those can be IR, but all the backend would
// do is store the addresses patch, and it would require the
// backend know what a loop is... So we can translate them to
// lower level IR instead...
//
// '&&' and expressions that can be short-circuited could be
// translated to lower level IR, but then it becomes harder to
// optimize? But then we'd have to replicate the same code for
// each backend.. Ok, since we're not doing and sort of optimization
// yet, we can translate it to lower level IR directly, and then
// if it becomes a problem we can revert.
// All loops can become lower level IR, since loop are pretty easy
// to find anyways... Alrighty then, we would have to remove some
// IR instructions, like AND and OR and so on... we  can leave that
// for last...
//
void begin_if(Parser *fs, Source line, BranchJumps *s, IR_Id x, int z) {
	BooleanJumps js = {0};
	emit_branch_if(fs,&js,z,x);
	// if  0 = jz
	// iff 1 = jnz
	if (z == L_IF) {
		ASSERT(js.f != 0);
		patch_jumps(fs,js.t);
		ARRAY_DELETE(js.t);
		js.t = 0;
		s->jz = js.f;
	} else {
		ASSERT(js.t != 0);
		patch_jumps(fs,js.f);
		ARRAY_DELETE(js.f);
		js.f = 0;
		s->jz = js.t;
	}
}


/*
** Closes previous conditional block by emitting
** escape jump, patches previous jz (jump if false)
** list to enter this block.
*/
void add_else_clause(Parser *fs, Source line, BranchJumps *s) {
	if (s->jz == 0) {
		parser_dialog(fs,line,"invalid else clause");
	}
	ASSERT(s->jz != 0);
	int j = emit_jump(fs,line,-1);
	ARRAY_ADD(s->j,j);

	patch_jumps(fs,s->jz);
	ARRAY_DELETE(s->jz);
	s->jz = 0;
}


void add_elif_clause(Parser *fs, Source line, BranchJumps *s, int x) {
	add_else_clause(fs,line,s);
	begin_if(fs,line,s,x,L_IF);
}


void add_then_clause(Parser *fs, Source line, BranchJumps *s) {
	/* we don't need to close the previous block, it can just fall
	through to our branch, do collect all the other exit jumps and
	tie them to this branch block, naturally we don't need to add
	an exit jump since else and elif or closeif will terminate
	this block, multiple then blocks are simply chained together
	naturally. */
	patch_jumps(fs,s->j);
	ARRAY_DELETE(s->j);
	s->j = 0;
}


void close_if(Parser *fs, Source line, BranchJumps *s) {
	/* collect missing else branch */
	if (s->jz != 0) {
		patch_jumps(fs,s->jz);
		ARRAY_DELETE(s->jz);
		s->jz = 0;
	}
	/* collect missing then branch */
	if (s->j != 0) {
		patch_jumps(fs,s->j);
		ARRAY_DELETE(s->j);
		s->j = 0;
	}
}

void begin_do_while_loop(Parser *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block
	ASSERT(bl->flags & BLOCK_LOOP);
	bl->loop.entry = get_instr_cursor(fs);
	bl->loop.false_jumps = 0;
	bl->loop.x = NO_IR;
	bl->loop.index_register = NO_SLOT;
}


void close_do_while_loop(Parser *fs, Source line, IR_Id x) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	BooleanJumps js = {0};
	emit_jump_if_true(fs,&js,x);

	patch_jumps2(fs,js.t,bl->loop.entry);
	ARRAY_DELETE(js.t);
	js.t = 0;

	patch_jumps2(fs,bl->loop.true_jumps,bl->loop.entry);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
}


void begin_while_loop(Parser *fs, IR_Id x) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	bl->loop.x = x;

	bl->loop.entry = get_instr_cursor(fs);

	/* todo: this is temporary */
	emit_bytex_deprecated(fs,NO_LINE,BC_LOOP,-1);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,x);
}


void close_while_loop(Parser *fs) {
	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;

	/* todo: this is temporary */
	ASSERT(BC_OP(get_byte(fs,bl->loop.entry))==BC_LOOP);
	// fs->M->bytes[bl->loop.entry].x = get_instr_cursor(fs);

	emit_jump(fs,NO_LINE,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}


void begin_range_loop(Parser *fs, Source line, IR_Id index_node, IR_Id lo, IR_Id hi) {
	__debugbreak();

	FileBlock *bl = get_block(fs,-1);
	ASSERT(bl->flags & BLOCK_LOOP);

	ASSERT(index_node != NO_IR);

	elf_StackId index_register; // xxx = compiler_ir2anyreg(fs,index_node);
	index_node = node_load(fs,line,index_register);

	bl->loop.index_register = index_register;
	bl->loop.x = index_node;
	emit_eval_deprecated(fs,0,index_register,1,node_type_guard(fs,get_ir_line(fs,lo),lo,NT_INT));

	bl->loop.entry = get_instr_cursor(fs);

	__debugbreak();
	elf_StackId hi_register; // xxx = compiler_ir2anyreg(fs,node_type_guard(fs,get_ir_line(fs,hi),hi,NT_INT));
	hi = node_load(fs,line,hi_register);
	IR_Id c = node_less_than(fs,line,index_node,hi);

	ASSERT(bl->loop.false_jumps == 0);

	BooleanJumps js = {0};
	bl->loop.false_jumps = emit_jump_if_false(fs,&js,c);
}


void close_range_loop(Parser *fs, Source line) {
	FileBlock *bl = get_block(fs,-1); // fs->fn->block;
	ASSERT(bl->flags & BLOCK_LOOP);

	patch_jumps(fs,bl->loop.true_jumps);
	ARRAY_DELETE(bl->loop.true_jumps);
	bl->loop.true_jumps = 0;
	// IR_Id index_node = bl->loop.index_node;
	IR_Id index_node = node_load(fs,NO_LINE,bl->loop.index_register);
	IR_Id k = node_xy(fs,NO_LINE,IR_ADD,NT_INT,index_node,node_int(fs,NO_LINE,1));

	__debugbreak();
	// xxx emit_store_deprecated(fs,line,index_node,k);

	emit_jump(fs,line,bl->loop.entry);

	patch_jumps(fs,bl->loop.false_jumps);
	ARRAY_DELETE(bl->loop.false_jumps);
	bl->loop.false_jumps = 0;
}
#endif

#if 0
// this is the code that emits a byte-code yield
// this should be over in emit, all we do here is
// emit the yield ir
void emit_yield(Parser *fs, Source line, IR_Id id) {
	/* todo: add support for multiple results */
	/* todo: if we only return one value we don't have to reload */
	int mem,reg,nreg,j;
	mem=get_mem_state_deprecated(fs);
	if (id!=NO_IR) {
		/* todo: multi-returns */
		emit_eval_deprecated(fs,0,reg=reg_alloc_deprecated(fs),nreg=1,id);
		if (fs->fn->nyield < nreg) fs->fn->nyield = nreg;
		j=emit_bytexyz_deprecated(fs,line,BC_YIELD,NO_JUMP,reg,nreg);
		ARRAY_ADD(fs->fn->yield_jumps,j);
	} else emit_bytex_deprecated(fs,line,BC_LEAVE,0);
	set_mem_state_deprecated(fs,mem);
	add_block_flags(fs,BLOCK_ENDED);
}

void emit_continue(Parser *fs, Source line, int reg) {
	__debugbreak();

	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,reg);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->loop.true_jumps,jmp);
}

// get_instr_cursor is only for bytecode
void emit_break(Parser *fs, Source line, elf_StackId with_value_register) {
	__debugbreak();

	ASSERT(fs->nloops > 0);
	Instr jmp;
	FileBlock *bl;

	bl=get_loop_block(fs,with_value_register);
	ASSERT(bl!=0);
	add_block_flags(fs,BLOCK_ENDED);

	jmp=emit_jump(fs,line,get_instr_cursor(fs));
	ARRAY_ADD(bl->leavejumps,jmp);
}
#endif
