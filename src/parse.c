/*
** parse.c
** See Copyright Notice In elf.h
*/

char *parser_get_name(Parser *parser) {
	return parser->filename;
}

void parser_end(Parser *parser) {
	close_function(parser);
}

int parser_begin(Parser *parser, char *filename, char *filetext) {
	if ((filename == 0) || (filetext == 0)) {
		return -1;
	}
	parser->filename   = filename;
	parser->filetext   = filetext;
	parser->thischar   = filetext;
	parser->linechar   = filetext;
	parser->linenumber = 1;

	/* kick start by lexing the first two tokens */
	poll_token(parser);
	poll_token(parser);

	begin_function(parser,&parser->function,parser->this_token.line);
	return 1;
}


static bool check_expr(Parser *fs, Source line, TreeId id) {
	if (id != NO_NODE) return 0;
	parser_dialog(fs,line,"invalid expression");
	return 1;
}


/* Todo: use line number instead
test the current token if on the same line as the previous token */
static bool test_token_inline(Parser *parser, elf_TokenType k) {
	return parser->this_token.type == k && parser->last_token.eol != 1;
}


static bool test_token(Parser *parser, elf_TokenType k) {
	return parser->this_token.type == k;
}

/* whether there are no more tokens or whether the
current token is a match. */
static bool term_token(Parser *parser, elf_TokenType k) {
	return parser->this_token.type == TK_NONE || parser->this_token.type == k;
}

static bool term_eol_token(Parser *parser) {
	return parser->this_token.type == TK_NONE || parser->last_token.eol == 1;
}

static bool pick_token(Parser *parser, elf_TokenType k) {
	return test_token(parser,k) && (poll_token(parser), 1);
}


static bool pick_token_inline(Parser *parser, elf_TokenType k) {
	return test_token_inline(parser,k) && (poll_token(parser), 1);
}


static FileToken take_token(Parser *fs, int k) {
	FileToken tk = fs->tk;
	if (!pick_token(fs,k)) {
		parser_dialog(fs,fs->this_token.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tk;
}


static FileToken get_token_inline(Parser *fs, int k) {
	FileToken tok = fs->this_token;
	if (!pick_token_inline(fs,k)) {
		parser_dialog(fs,tok.line,"expected '%s'\n",elf_token_intel[k].name);
	}
	return tok;
}


/* looks for an enclosed entity within the function,
and returns the index where the entity, the index
can then be used to emit instructions. */
static int get_closure_value_index(FileFunction *fn, EntityIdGuard id) {
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) {
			return i;
		}
	}
	return NO_SLOT;
}


/* encloses an entity within the given function. */
static void enclose_entity(Parser *fs, FileFunction *fn, EntityIdGuard id) {
	/* ensure the entity should actually be captured */
	ASSERT(id.id < fn->entities);
	/* check whether the entity was already captured */
	FOR_ARRAY(i,fn->enclosure) {
		if (fn->enclosure[i] == id.id) return;
	}
	ARRAY_ADD(fn->enclosure,id.id);
}


/* find the last declared entity for the given register
within the current function only! */
static int find_local_entity(Parser *fs, int args) {
	int id;
	for (id = fs->nentities-1; id >= fs->fn->entities; id-=1) {
		if ((fs->entities[id].kind==ENTITY_LOCAL) && (fs->entities[id].args==args)) {
			return id;
		}
	}
	return -1;
}


/* finds the last declared entity with the given
name. the id is absolute. */
static EntityId get_entity(Parser *fs, char *name, elf_Bool enclose) {
	FileFunction *fn = fs->fn;
	/* we're using a linear search method here, but I can guarantee
	that this is actually fast enough, introducing table will not
	be profitable for a long while... */
	EntityId id;
	for (id = fs->nentities-1; id > -1; -- id) {
		if (text_eq(fs->entities[id].name,name)) {
			if ((id < fn->entities) && (enclose)) {
				enclose_entity(fs,fn,(EntityIdGuard){id});
			}
			fs->entities[id].flags |= ENTITY_REFERENCED;
			return id;
		}
	}
	return NO_ENTITY;
}


/* register a local entity within the current function and level. */
static TreeId new_entity(Parser *fs, Source line, int flags, char *name, int args) {
	FileFunction *fn;
	EntityId already;
	FileEntity entity;

	fn=fs->fn;

	already=get_entity(fs,name,0);
	if (already!=NO_ENTITY) {
		entity=fs->entities[already];
		if (entity.kind==ENTITY_DIRECTORY)  {
			parser_dialog(fs,line,"'%s': name is reserved for symbol directory",name);
		}
		if (entity.level==fs->nblocks) {
			/* reassignment */
			// parser_dialog(fs,line,"'%s': is already declared",name);
		} else {
			/* only issue this warning if the entity we found
			is within this function... */
			if (already>=fn->entities) {
				parser_dialog(fs,line,"'%s': this declaration shadows another one",name);
			}
		}
	}

	EntityId id = fs->nentities ++;
	ARRAY_GROW(fs->entities,fs->nentities-ARRAY_LENGTH(fs->entities));

	fs->entities[id].kind  = ENTITY_LOCAL;
	fs->entities[id].flags = flags;
	fs->entities[id].args  = args;
	fs->entities[id].line  = line;
	fs->entities[id].name  = name;
	fs->entities[id].level = fs->nblocks; // this should be nblocks-1, it doesn't really matter...
	return id;
}


static TreeId find_name(Parser *F, Source line, int flags, char *name) {
	EntityId id = get_entity(F,name,1);
	if (id != NO_ENTITY) {
		if (flags) {
			if (~F->entities[id].flags & ENTITY_ASSIGNED) {
				parser_dialog(F,line,"warning: usage of possibly unassigned variable");
			}
		}
		FileFunction *fn = F->fn;
		if (id < fn->entities) {
			if (id < fn->enclosing->entities) {
				parser_dialog(F,line,"too many layers for caching");
			}
			return node_closure_value(F,line,get_closure_value_index(fn,ENTITY(id)));
		} else {
			return node_local(F,line,F->entities[id].args);
		}
	} else {
		/* todo: check whether we've assigned a value
		to this entity already, otherwise issue a warning that
		we're using something that hasn't got a value yet... */
		return node_global_name(F,line,name);
	}
}


static void begin_function(Parser *fs, FileFunction *fn, char *line) {
	fn->enclosing = fs->fn;
	fn->entities = fs->nentities;
	fn->bytes = fs->M->nbytes;
	fn->line = line;
	fn->yield_jumps = 0;
	/* todo: "begin_block" requires fn to be set
	for xmemory, can xmemory simply be in the
	file state instead? */
	fs->fn = fn;
	fn->entry_block = begin_block(fs,0);

	int flags=ENTITY_PARAMETER|ENTITY_ASSIGNED|ENTITY_CONSTANT|ENTITY_REFERENCED;
	new_entity(fs, line, flags, "this", reg_alloc(fs));
}


static void close_function(Parser *fs) {
	FileFunction *fn;
	Source line;
	fn = fs->fn;
	line = fs->this_token.line;
	patch_jumps(fs,fn->yield_jumps);
	ARRAY_DELETE(fn->yield_jumps);
	fn->yield_jumps = 0;
	emit_bytex(fs,line,BC_LEAVE,0);
	close_block(fs);
	ASSERT(fn->entry_block == fs->nblocks);
	ASSERT(fs->nentities == fn->entities);
	fs->fn = fn->enclosing;
}


/* {x} | ( x { ... } ) | { <table-initializer-list> } */
static TreeId *parse_call_args(Parser *fs) {
	TreeId x;
	TreeId *z,*n;
	z=0;
	if (test_token(fs,TK_CURLY_LEFT)) {
		x=parse_table(fs);
		ARRAY_ADD(z,x);
	} else if (pick_token(fs,TK_PAREN_LEFT)) {
		if (!test_token(fs,TK_PAREN_RIGHT)) do {
			x=parse_expr(fs,0,0);
			if (x!=NO_NODE) {
				if (get_node_kind(fs,x)==NODE_MULTI) {
					n=get_node(fs,x).z;
					FOR_ARRAY(i,n){
						ARRAY_ADD(z,n[i]);
					}
				} else ARRAY_ADD(z,x);
			} else break;
		} while (pick_token(fs,TK_COMMA));
		take_token(fs,TK_PAREN_RIGHT);
	} else {
		x=parse_expr(fs,0,0);
		if (x!=NO_NODE)ARRAY_ADD(z,x);
	}
	return z;
}


static elf_TokenType word2tok(FileToken tk) {
	/* todo: meh... remove this */
	if (tk.type != TK_WORD) return tk.type;
	if (!strcmp(tk.text,"and")) return TK_LOG_AND;
	if (!strcmp(tk.text,"or"))  return TK_LOG_OR;
	if (!strcmp(tk.text,"is"))  return TK_EQ;
	return TK_WORD;
}


static int get_token_prec(elf_TokenType type) {
	return elf_token_intel[type].prec;
}


static TreeKi tok2node(elf_TokenType tk) {
	switch (tk) {
		case TK_DOT_DOT: return NODE_RANGE;
		case TK_LOG_AND: return NODE_AND;
		case TK_LOG_OR: return NODE_OR;
		case TK_NIL_OR: return NODE_NIL_OR;
		case TK_NIL_AND: return NODE_NIL_AND;
		case TK_ADD: return NODE_ADD;
		case TK_SUB: return NODE_SUB;
		case TK_DIV: return NODE_DIV;
		case TK_MUL: return NODE_MUL;
		case TK_POW: return NODE_POW;
		case TK_MOD: return NODE_MOD;
		case TK_NEQ: return NODE_NEQ;
		case TK_EQ: return NODE_EQ;
		case TK_GT: return NODE_GT;
		case TK_GTEQ: return NODE_GTEQ;
		case TK_LT: return NODE_LT;
		case TK_LTEQ: return NODE_LTEQ;
		case TK_SHL: return NODE_BIT_SHL;
		case TK_SHR: return NODE_BIT_SHR;
		case TK_BIT_XOR: return NODE_BIT_XOR;
		case TK_BIT_OR: return NODE_BIT_OR;
		case TK_BIT_AND: return NODE_BIT_AND;
		default: return NODE_NONE;
	}
}


TreeId parse_subexpr(Parser *fs, BooleanJumps *expr, int rank, int flags) {
	int oper,prio;
	TreeId x,y;
	FileToken tk;

	x=parse_unary(fs,expr,flags|EXPR_ALLOW_POSTFIX);
	if (x==NO_NODE) goto esc;

	retry:
	oper=word2tok(fs->this_token);
	prio=get_token_prec(oper);
	if (prio<=rank) goto esc;
	if (fs->then_token.type==TK_ASSIGN) goto esc;
	tk=poll_token(fs);
	y=parse_subexpr(fs,0,prio,flags);
	if (y==NO_NODE) goto esc;
	x=node_xy(fs,tk.line,tok2node(oper),NT_ANY,x,y);
	goto retry;

	esc:
	return x;
}


/* Named functions aren't a thing for this
language... at least for now... */
TreeId parse_function(Parser *fs) {
	FileToken tk;
	int fj,arity;
	FileFunction fn = {0};

	tk=take_token(fs,TK_FUN);
	/* emit a jump instruction to skip this function's code */
	fj=emit_jump(fs,tk.line,-1);
	begin_function(fs,&fn,tk.line);
	arity=1;

	/* start loading parameter list, each parameter also
	becomes a local register */
	take_token(fs,TK_PAREN_LEFT);
	if (!test_token(fs,TK_PAREN_RIGHT)) do {
		FileToken name;

		name=take_token(fs,TK_WORD);
		new_entity(fs, name.line, ENTITY_PARAMETER|ENTITY_ASSIGNED
		, name.text, reg_alloc(fs));

		arity += 1;
	} while (pick_token(fs,TK_COMMA));

	if (!test_token(fs,TK_PAREN_RIGHT)) {
		parser_dialog(fs,0,"did you miss a ','?");
	}
	take_token(fs,TK_PAREN_RIGHT);

	/* '?' are now optional */
	pick_token(fs,TK_QMARK);

	if (test_token(fs,TK_CURLY_LEFT)) {
		take_token(fs,TK_CURLY_LEFT);
		while (parse_stat(fs)) {
			tk=fs->this_token;
		}
		/* By default we emit a 'leave this' instruction,
		because this tends to be more convenient... */
		/* todo: only emit the yield if this block
		wasn't terminated by another leave instruction,
		otherwise this is wasteful */
		emit_yield(fs,tk.line,node_this(fs,tk.line));
		take_token(fs,TK_CURLY_RIGHT);
	} else {
		emit_yield(fs,tk.line,parse_expr(fs,0,0));
	}

	close_function(fs);

	/* now finally patch the jump-over jump
	to resume control flow... */
	patch_jump(fs,fj);


	/* create a new prototype and add this function
	to the list of prototypes */
	elf_Prototype fp = {0};
	/* todo: implement this */
	// fp.parent  = fn.enclosing
	fp.arity	  = arity;
	fp.nlocals = fn.nlocals;
	fp.nvalues = ARRAY_LENGTH(fn.enclosure);
	fp.bytes   = fn.bytes;
	fp.nbytes  = fs->M->nbytes - fn.bytes;

	int f = elf_add_function(fs->M,fp);

	/* Now iterate over all the enclosed or captured
	entities, this are the locals that we accessed
	from outside our function, these are registers
	so we can create register nodes directly. */
	TreeId *z = 0;
	FOR_ARRAY(i,fn.enclosure) {
		FileEntity entity = fs->entities[fn.enclosure[i]];
		ARRAY_ADD(z,node_local(fs,entity.line,entity.args));
	}

	return node_new_closure(fs,tk.line,f,z);
}


void parse_assign(Parser *fs, TreeId lexpr) {
	if (lexpr==NO_NODE) {
		return;
	}

	TreeId x,y;
	int mem;
	FileToken tk,op;

	tk=fs->tk;
	mem=get_mem_state(fs);
	x=desugar_range_expr(fs,lexpr,0);
	x=emit_preload(fs,x);
	if (pick_token(fs,TK_ASSIGN)) {
		y=parse_expr(fs,0,0);
		emit_store(fs,tk.line,x,y);
	} else if (pick_token(fs,TK_NIL_ASSIGN)) {
		BooleanJumps js = {0};
		y=parse_expr(fs,0,0);
		emit_jump_if_not_nil(fs,tk.line,&js,x);
		emit_store(fs,tk.line,x,y);
		patch_jumps(fs,js.f);
	} else {
		if (get_token_prec(tk.type) > 0) {
			op=poll_token(fs);
			take_token(fs,TK_ASSIGN);
			/* todo: optimization! */
			y=parse_expr(fs,0,0);
			y=node_xy(fs,op.line,tok2node(op.type),get_node_type(fs,x),x,y);
			emit_store(fs,op.line,x,y);
		} else {
			emit_eval(fs,0,-1,0,lexpr);
		}
	}
	desugar_range_expr_epilogue(fs,lexpr);
	set_mem_state(fs,mem);
}


TreeId parse_table(Parser *fs) {
	TreeId *args,table,key,field,store,value;
	FileToken token;
	int index;

	token=take_token(fs,TK_CURLY_LEFT);
	table=node_new_table(fs,token.line,0);
	args=0;
	index=0;
	token=fs->this_token;

	for (;(token.type!=TK_NONE)&&(token.type!=TK_CURLY_RIGHT);token=fs->this_token) {
		value=NO_NODE;
		if ((token.type==TK_WORD)&&(fs->then_token.type==TK_ASSIGN)) {
			token=poll_token(fs);
			key=node_string(fs,token.line,token.text);
		} else {
			key=value=parse_expr(fs,0,0);
		}
		token=fs->this_token;
		if (pick_token(fs,TK_ASSIGN)) {
			value=parse_expr(fs,0,0);
		} else {
			key=node_integer(fs,token.line,index++);
		}

		check_expr(fs,token.line,key);
		check_expr(fs,token.line,value);
		token=fs->this_token;
		field=node_field(fs,token.line,table,key);
		store=node_store(fs,token.line,field,value);
		ARRAY_ADD(args,store);
		if (pick_token(fs,TK_COMMA)) {
			continue;
		}
	}
	take_token(fs,TK_CURLY_RIGHT);
	fs->nodes[table].z = args;
	return table;
}


TreeId parse_expr(Parser *fs, BooleanJumps *expr, int flags) {
	switch (fs->this_token.type) {
		case TK_NONE:
		case TK_FOR: case TK_WHILE: case TK_LASTLY:
		case TK_COMMA:
		case TK_PAREN_RIGHT: case TK_CURLY_RIGHT: case TK_SQUARE_RIGHT: {
			return NO_NODE;
		}
	}
	return parse_subexpr(fs,expr,0,flags);
}


/* todo: add support for:
specifing which for loop you're reffering to. */
#if 0
elf_StackId target_value_register = NO_SLOT;
if (!term_eol_token(fs)) {
	TreeId value = parse_expr(fs,0);
	if (value != NO_NODE) {
		target_value_register = get_node_register(fs,NODE(value));
		if (target_value_register < 0) {
			parser_dialog(fs,get_node_line(fs,value),"invalid value");
		}
	}
}
#endif


TreeId parse_unary(Parser *fs, BooleanJumps *expr, elf_Bool flags) {
	TreeId v = NO_NODE;
	FileToken tk = fs->this_token;
	TreeId x;
	switch (tk.type) {
		case TK_M_INDEX: case TK_M_ARRAY: case TK_M_VALUE: {
			poll_token(fs);
			int reg;

			if (tk.type==TK_M_ARRAY) reg=SPECIAL_REGISTER_ARRAY; else
			if (tk.type==TK_M_VALUE) reg=SPECIAL_REGISTER_VALUE; else reg=SPECIAL_REGISTER_INDEX;

			v=node_local(fs,tk.line,reg);
		} break;
		/* todo: make this an intrinsic instruction! */
		case TK_M_INT: case TK_M_NUM: {
			char *name;
			TreeId fn, *z=0;

			poll_token(fs);
			x=parse_unary(fs,0,flags|EXPR_ALLOW_POSTFIX);
			name=tk.type==TK_M_INT?"ntoi":"iton";
			fn=node_global_name(fs,tk.line,name);
			ARRAY_ADD(z,x);
			v=node_call(fs,tk.line,fn,z);
		} break;
		case TK_M_REGISTER: {
			poll_token(fs);
			tk=take_token(fs,TK_WORD);
			x=get_entity(fs,tk.text,0);
			if (x!=NO_ENTITY) {
				v=node_integer(fs,tk.line,fs->entities[x].args);
			} else parser_dialog(fs,tk.line,"'%s': invalid entity (must be a local)",tk.text);
		} break;
		//
		// 'load' ( <file-name> )
		//
		//  elf.loadfile(<file-name>)
		//
		case TK_LOAD: {
			poll_token(fs);
			TreeId *call_args = parse_call_args(fs);

			TreeId load_file_func = node_global_name(fs,tk.line,"elf.loadfile");
			TreeId call_load_file = node_call(fs,tk.line,load_file_func,call_args);
			v = call_load_file;
		} break;
		//
		// 'new' <meta-table> ( <argument-list> )
		//
		// leave elf.set_object_metatable({},Vector2):__new(x,y)
		//
		case TK_NEW: {
			poll_token(fs);
			TreeId meta_field_name, get_meta_field, call_new
			, meta_table, *call_args, table;

			meta_table=parse_unary(fs,0,0);
			call_args=parse_call_args(fs);

			/* so if the user does something like new Thing {}
			or new Thing({}) the table that was passed in can
			be used as supposed to creating a new one */
			if ((ARRAY_LENGTH(call_args) == 1) && (get_node_kind(fs,call_args[0]) == NODE_TABLE)) {
				table = call_args[0];
			} else {
				/* If the user however, doesn't do this, then we
				create a new table for him */
				table = node_new_table(fs,tk.line,0);
			}

			table=node_call_set_metatable(fs,tk.line,table,meta_table);

			meta_field_name=node_string(fs,tk.line,"__new");
			get_meta_field=node_metafield(fs,tk.line,table,meta_field_name);
			call_new=node_call(fs,tk.line,get_meta_field,call_args);

			v = call_new;
		} break;
		/* empty ranges '..' are interpreted as 0..limit
		of whatever expression. */
		case TK_DOT_DOT: {
			poll_token(fs);
			v=node_xy(fs,tk.line,NODE_RANGE,NT_ANY,NO_NODE,NO_NODE);
		} break;
		/* todo: this is temporary */
		case TK_DOT: case TK_ELF: {
			char dir[MAX_PATH] = {};
			if (pick_token(fs,TK_ELF)) {
				strcat(dir,"elf");
				/* remind the user elf is a reserved keyword */
				if (!test_token_inline(fs,TK_DOT)) {
					parser_dialog(fs,tk.line,"incomplete symbol, expected '.' on the same line as 'elf'. Did you mean to use 'elf'? This is a reserved keyword and it refers to the elf directory.");
				}
			}

			take_token(fs,TK_DOT);
			do {
				get_token_inline(fs,TK_WORD);
				strcat(dir,".");
				strcat(dir,fs->last_token.text);
			} while (pick_token_inline(fs,TK_DOT));
			int x;
			x=elf_get_global(fs->M,elf_alloc_string(fs->R,dir));
			v=node_global(fs,tk.line,x);
		} break;
		case TK_WORD: {
			poll_token(fs);
			v=find_name(fs,tk.line,flags,tk.text);
		} break;
		case TK_SUB: {
			poll_token(fs);
			v=parse_subexpr(fs,0,10000,flags);
			v=node_xy(fs,tk.line,NODE_SUB,NT_INT,node_integer(fs,tk.line,0),v);
		} break;
		case TK_ADD: {
			poll_token(fs);
			v=parse_subexpr(fs,0,10000,flags);
		} break;
		case TK_CURLY_LEFT: {
			v=parse_table(fs);
		} break;
		case TK_PAREN_LEFT: {
			poll_token(fs);
			v=parse_expr(fs,0,EXPR_ALLOW_POSTFIX);
			take_token(fs,TK_PAREN_RIGHT);
			/* allow for empty () */
			if (v!=NO_NODE) {
				v=node_group(fs,tk.line,v);
				ASSERT(v != NO_NODE);
			}
		} break;
		case TK_FUN: {
			v=parse_function(fs);
		} break;
		case TK_NIL: {
			poll_token(fs);
			v=node_nil(fs,tk.line);
		} break;
		case TK_TRUE: case TK_FALSE: {
			poll_token(fs);
			v=node_integer(fs,tk.line,tk.type==TK_TRUE);
		} break;
		case TK_LETTER: case TK_INTEGER: {
			poll_token(fs);
			v=node_integer(fs,tk.line,tk.integer);
		} break;
		case TK_NUMBER: {
			poll_token(fs);
			v=node_number(fs,tk.line,tk.number);
		} break;
		case TK_STRING: {
			poll_token(fs);
			v=node_string(fs,tk.line,tk.text);
		} break;
		case TK_DEFAULT: {
			parser_dialog(fs,tk.line,"syntax error: default expressions can only be top level");
			elf_fail(fs->R,0,"syntax error: default expressions can only be top level");
		} break;
		default: {
			parser_dialog(fs,tk.line,"'%s': unexpected token", elf_token_intel[tk.type].name);
			elf_fail(fs->R,0,"syntax error: unexpected token");
		} break;
	}

	if (~flags & EXPR_ALLOW_POSTFIX) {
		goto esc;
	}

	/* ensure we don't parse a postfix past a line */
	while (!term_eol_token(fs)) {
		tk = fs->tk;

		switch (tk.type) {
			case TK_DOT: {
				poll_token(fs);
				// table.(x,y) -> (table.x, table.y)
				if (pick_token(fs,TK_PAREN_LEFT)) {
					TreeId *z = {0};
					do {
						FileToken n = take_token(fs,TK_WORD);
						TreeId x,y;
						y=node_string(fs,n.line,n.text);
						x=node_field(fs,tk.line,v,y);
						ARRAY_ADD(z,x);
					} while (pick_token(fs,TK_COMMA));
					v = node_multi(fs,tk.line,z);
					take_token(fs,TK_PAREN_RIGHT);
				} else
				// table.{x,y}
				if (pick_token(fs,TK_CURLY_LEFT)) {
					NO_CODE;
				} else {
					FileToken name;
					TreeId field;
					name=take_token(fs,TK_WORD);
					field=node_string(fs,name.line,name.text);
					v=node_field(fs,tk.line,v,field);
				}
			} break;
			/* todo: make this nil safe, so [0,0] shouldn't
			fail if item at 0 is nil  */
			case TK_SQUARE_LEFT: {
				take_token(fs,TK_SQUARE_LEFT);
				TreeId *z;
				TreeId index;
				do {
					index=parse_expr(fs,0,0);
					if (index==NO_NODE) break;
					/* registry[location.(y,x)] ->
					registry[location.y,location.x] */
					if (get_node_kind(fs,index)==NODE_MULTI) {
						z=get_node(fs,index).z;
						FOR_ARRAY(i,z) {
							v=node_index(fs,tk.line,v,z[i]);
						}
					} else if (get_node_kind(fs,index)==NODE_RANGE) {
						v=node_ranged_index(fs,tk.line,v,index);
					} else {
						v=node_index(fs,tk.line,v,index);
					}

					/* todo: this is silly, this is just an
					inner multi expressions, make multi
					expressions be regular 'comma' expressions
					instead */
				} while(pick_token(fs,TK_COMMA));
				take_token(fs,TK_SQUARE_RIGHT);
			} break;

			case TK_COLON: {
				FileToken n;
				TreeId y;
				poll_token(fs);
				n=take_token(fs,TK_WORD);
				y=node_string(fs,n.line,n.text);
				v=node_metafield(fs,tk.line,v,y);
			} break;
			case TK_CURLY_LEFT:
			case TK_PAREN_LEFT: {
				TreeId *z=parse_call_args(fs);
				v=node_call(fs,tk.line,v,z);
			} break;
			default: goto esc;
		}
	}

	esc:
	return v;
}

TreeId* parse_expr_list(Parser *fs) {
	TreeId y,*yz,*z=0;
	do {
		y=parse_expr(fs,0,0);
		if(get_node_kind(fs,y)==NODE_MULTI) {
			yz=get_node(fs,y).z;
			FOR_ARRAY(i,yz) ARRAY_ADD(z,yz[i]);
		} else {
			ARRAY_ADD(z,y);
		}
	} while (pick_token(fs,TK_COMMA));
	return z;
}


void parse_for_loop(Parser *fs) {
	FileToken tk,name;
	TreeId y,*z,value,array,index,lo,hi;
	int block,block_head=NO_BYTE,block_tail=NO_BYTE;
	int value_register,array_register,index_register;

	tk=take_token(fs,TK_FOR);
	name=take_token(fs,TK_WORD);
	take_token(fs,TK_ASSIGN);
	z=parse_expr_list(fs);
	take_token(fs,TK_QMARK);

	block=begin_block(fs,BLOCK_LOOP);
	value_register=reg_alloc(fs);
	value=node_local(fs,name.line,value_register);

	/* todo: remove REFERENCED, instead allow the user to not have to specify the name */
	new_entity(fs,name.line,ENTITY_REFERENCED|ENTITY_ASSIGNED|ENTITY_FORLOOP
	, name.text,value_register);

	FOR_ARRAY(i,z){
		y=z[i];
		array=NO_NODE;
		array_register=NO_SLOT;
		index=NO_NODE;
		index_register=NO_SLOT;

		if (get_node_kind(fs,y)==NODE_RANGE_INDEX) {
			array=get_node(fs,y).x;
			array_register=emit_load(fs,array);

			y=get_node(fs,y).y;
			ASSERT(get_node_kind(fs,y)==NODE_RANGE);
		}
		if (get_node_kind(fs,y)==NODE_RANGE) {
			lo=get_node(fs,y).x;
			hi=get_node(fs,y).y;
			if (array==NO_NODE) {
				index=value;
				// for ... ? { }
				if (lo==NO_NODE)lo=node_integer(fs,tk.line,0);
				if (hi==NO_NODE)hi=node_integer(fs,tk.line,-1);
			} else {
				// for array[...] ? { }
				if (lo==NO_NODE)lo=node_integer(fs,tk.line,0);
				if (hi==NO_NODE)hi=node_call_metafield(fs,tk.line,array,0,"length");
				/* todo: we're allocating this here, and never freeing it! */
				index=node_local(fs,tk.line,reg_alloc(fs));
			}

			begin_range_loop(fs,tk.line,index,lo,hi);

			get_block(fs,block)->loop.value_register=value_register;
			get_block(fs,block)->loop.array_register=array_register;

			if (array!=NO_NODE) {
				TreeId *z = {0};
				ARRAY_ADD(z,index);
				emit_store(fs,name.line,value,node_call_metafield(fs,tk.line,array,z,"idx"));
			}
			/* todo: could be neater */
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail) {
					emit_byte(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
				}
			}
			close_range_loop(fs,tk.line);
		} else {
			emit_store(fs,tk.line,value,y);
			if (i==0) {
				block_head=get_instr_cursor(fs);
				parse_stat(fs);
				block_tail=get_instr_cursor(fs);
			} else {
				FOR_RANGE(j,block_head,block_tail){
					emit_byte(fs,elf_get_instr_line(fs->M,j),get_byte(fs,j));
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
	close_block(fs);
}


int parse_stat(Parser *parser) {
	TreeId x;

	FileToken tk = parser->this_token;
	FileBlock *bl = get_block(parser,-1);
	int mem = get_mem_state(parser);

	switch (tk.type) {
		case TK_NONE: case TK_CURLY_RIGHT:
		case TK_THEN: case TK_ELSE: case TK_ELIF: {
			return 0;
		}
	}

	if (bl->flags & BLOCK_ENDED) {
		parser_dialog(parser,tk.line,"warning: unreachable statement");
	}

	switch (tk.type) {
		case TK_LASTLY: case TK_FINALLY: {
			poll_token(parser);
			if (tk.type == TK_FINALLY) {
				parser_dialog(parser,tk.line,"warning: please consider using 'lastly' instead, 'finally' could change semantics in the future");
			}
			begin_delay_block(parser,tk.line);
			parse_stat(parser);
			close_delay_block(parser,tk.line);
			ASSERT(get_mem_state(parser)==mem);
		} break;
		case TK_IF: case TK_IFF: {
			BranchJumps s = {0};
			poll_token(parser);
			x=parse_expr(parser,0,0);
			take_token(parser,TK_QMARK);
			begin_block(parser,0);
			begin_if(parser,tk.line,&s,x,tk.type==TK_IFF?L_IFF:L_IF);
			parse_stat(parser);
			while (!test_token(parser,TK_NONE)) {
				if (pick_token(parser,TK_ELIF)) {
					begin_block(parser,0);
					x=parse_expr(parser,0,0);
					take_token(parser,TK_QMARK);
					add_elif_clause(parser,parser->last_token.line,&s,x);
					parse_stat(parser);
					close_block(parser);
				} else if (pick_token(parser,TK_THEN)) {
					begin_block(parser,0);
					add_then_clause(parser,parser->last_token.line,&s);
					parse_stat(parser);
					close_block(parser);
				} else if (pick_token(parser,TK_ELSE)) {
					begin_block(parser,0);
					add_else_clause(parser,parser->last_token.line,&s);
					parse_stat(parser);
					close_block(parser);
				} else break;
			}
			close_if(parser,parser->last_token.line,&s);
			close_block(parser);
			ASSERT(get_mem_state(parser)==mem);
		} break;
		case TK_LET: {
			poll_token(parser);
			do {
				if (test_token(parser,TK_LET)) {
					parser_dialog(parser,parser->last_token.line,"invalid declaration, expected next declarator's name after ',' instead got 'let'");
					parser_dialog(parser,parser->this_token.line,"invalid declaration, 'let' after comma");
					elf_fail(parser->R,0,"syntax error: invalid declaration");
				}

				FileToken name;
				int reg;

				name=take_token(parser,TK_WORD);
				reg=reg_alloc(parser);
				/* this could be better, like instead we should
				evaluate the rhs and then point the entity
				to the register of where the result is, otherwise
				if we allocate a register now, it won't be
				available for the code generator to use */
				new_entity(parser,name.line,0,name.text,reg);
				parse_assign(parser,node_local(parser,name.line,reg));
			} while (pick_token(parser,TK_COMMA));
		} break;
		case TK_LEAVE: {

			poll_token(parser);
			x = parse_expr(parser,0,0);

			#if 0
			if (parser->default_register != NO_SLOT) {
				emit_store(parser,tk.line,node_local(parser,NO_LINE,parser->default_register),x);
			} else {
			#endif

			emit_yield(parser,tk.line,x);

			#if 0
			}
			#endif

			ASSERT(get_mem_state(parser)==mem);
		} break;
		case TK_BREAK: case TK_CONTINUE: {
			int reg = NO_NODE;
			poll_token(parser);
			if (!term_eol_token(parser)) {
				TreeId value = parse_unary(parser,0,0);
				if (value == NO_NODE) {
					if ((reg=get_node_register(parser,NODE(value)))<0) {
						parser_dialog(parser,get_node_line(parser,value),"invalid value");
					}
				}
			}
			if (tk.type==TK_CONTINUE) emit_continue(parser,tk.line,reg);
			else emit_break(parser,tk.line,reg);
		} break;
		case TK_WHILE: {
			poll_token(parser);
			begin_block(parser,BLOCK_LOOP);
			x=parse_expr(parser,0,0);
			take_token(parser,TK_QMARK);
			begin_while_loop(parser,x);
			parse_stat(parser);
			close_while_loop(parser);
			ASSERT(get_mem_state(parser)==mem);
			close_block(parser);
		} break;
		case TK_DO: {
			poll_token(parser);
			begin_block(parser,BLOCK_LOOP);
			begin_do_while_loop(parser,tk.line);
			parse_stat(parser);
			take_token(parser,TK_WHILE);
			x=parse_expr(parser,0,0);
			close_do_while_loop(parser,tk.line,x);
			ASSERT(get_mem_state(parser)==mem);
			close_block(parser);
		} break;
		case TK_CURLY_LEFT: {
			begin_block(parser,0);
			poll_token(parser);
			while (!term_token(parser,TK_CURLY_RIGHT)) {
				parse_stat(parser);
			}
			take_token(parser,TK_CURLY_RIGHT);
			close_block(parser);
		} break;
		case TK_FOR: {
			parse_for_loop(parser);
		} break;
		default: {
			#if 0
			EntityId entity;
			int reg;
			if (parser->nblocks>1 && parser->then_token.type==TK_ASSIGN){
				poll_token(parser);
				entity=get_entity(parser,tk.text,0);
				if (entity==-1){
					parser_dialog(parser,tk.line,"new implicit entity: %s", tk.text);
					reg=reg_alloc(parser);
					entity=new_entity(parser,tk.line,0,tk.text,reg);
				} else {
					reg=parser->entities[entity].args;
				}
				x=node_local(parser,tk.line,reg);
			} else {
				x=parse_expr(parser,0,0);
			}
#endif
			x=parse_expr(parser,0,0);
			if (x==NO_NODE) {
				parser_dialog(parser,tk.line,"invalid statement");
			}
			parse_assign(parser,x);
		} break;
	}
	return 1;
}
