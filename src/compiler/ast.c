//
// See Copyright Notice In elf.h
//


// Todo, put this somewhere proper!

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static char *tree2s[] =
{
#define AST_XPAND(ENUM, NAME) #ENUM,
	AST_XDEF(AST_XPAND)
#undef AST_XPAND
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef create_ast(Parser *par, Source site, AstType kind)
{
	Ast *tree = elf_arena_push_zero(par->arena, sizeof(*tree));
	tree->site = site;
	tree->kind = kind;
	return tree;
}

static AstRef tree_nullary(Parser *par, Source site, AstType kind)
{
	AstRef tree = create_ast(par,site,kind);
	return tree;
}

static AstRef elf_new_unary_tree(Parser *par, Source site, AstType kind, AstRef x)
{
	AstRef tree = create_ast(par, site, kind);
	tree->tree_unary_expr = x;
	return tree;
}

static AstRef create_binary_expr_ast(Parser *par, Source site, AstType kind, AstRef x, AstRef y)
{
	AstRef tree = create_ast(par, site, kind);
	tree->ast_binary_expr.x = x;
	tree->ast_binary_expr.y = y;
	return tree;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static AstRef tree_nil(Parser *par, Source site)
{
	return tree_nullary(par, site, AST_NIL_LITERAL);
}

static AstRef tree_int(Parser *par, Source site, i64 i)
{
	AstRef v;
	v = tree_nullary(par, site, AST_INTEGER_LITERAL);
	v->expr_int = i;
	return v;
}

static AstRef tree_num(Parser *par, Source site, f64 n)
{
	AstRef v;
	v=tree_nullary(par, site, AST_NUMBER_LITERAL);
	v->expr_num=n;
	return v;
}

static AstRef create_str_ast(Parser *par, Source site, char *s)
{
	AstRef v = tree_nullary(par, site, AST_STRING_LITERAL);
	v->expr_str = s;
	return v;
}

static AstRef elf_new_table_expr_tree(Parser *par, Source site, AstRef *args, u32 nargs)
{
	AstRef tree = create_ast(par, site, AST_TABLE);
	tree->tree_table_expr.args = args;
	tree->tree_table_expr.nargs = nargs;
	return tree;
}

static AstRef elf_new_table_entry_tree(Parser *par, Source site, AstRef key, AstRef value)
{
	AstRef tree = create_ast(par, site, AST_TABLE_ENTRY);
	tree->tree_table_entry.key = key;
	tree->tree_table_entry.value = value;
	return tree;
}



static AstRef create_tuple_ast(Parser *par, Source site, AstRef *args, u32 nargs)
{
	ASSERT(nargs >= 1);
	AstRef tree = create_ast(par, site, AST_TUPLE);
	tree->ast_tuple_expr.args = args;
	tree->ast_tuple_expr.nargs = nargs;
	return tree;
}


static AstRef elf_new_ellipsis_tree(Parser *par, Source site)
{
	AstRef tree = create_ast(par, site, AST_ELLIPSIS);
	return tree;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// static AstRef tree_memory2(Parser *par, Source site, AstRef x, int rem)
// {
// 	ASSERT(rem >= 0);

// 	AstRef v = elf_new_unary_tree(par, site, TREE_MEMORY, x);
// 	v->tree_memory.mem =  -1;
// 	v->tree_memory.rem = rem;
// 	return v;
// }

// static AstRef tree_memory(Parser *par, Source site, AstRef x)
// {
// 	return tree_memory2(par, site, x, 1);
// }

// static inline AstRef tree_proxy(Parser *par, Source site, AstRef x)
// {
// 	return elf_new_unary_tree(par, site, TREE_PROXY, x);
// }

// static inline AstRef tree_proxy1(Parser *par, AstRef x)
// {
// 	return elf_new_unary_tree(par, x->site, TREE_PROXY, x);
// }

static AstRef tree_global(Parser *par, Source site, int x)
{
	AstRef v = tree_nullary(par, site, TREE_GLOBAL);
	v->expr_global = x;
	return v;
}

// todo: we should already have the GCStr by now...
static AstRef tree_global_symbol(Parser *par, Source site, const char *name)
{
	V v;
	to_str(&v, new_string_from_data(par->inter, name));
	Index x = elf_table_ensure(par->R, par->inter->globals, v);

	ASSERT(x != -1);
	return tree_global(par, site, x);
}

static AstRef tree_enforce(Parser *par, Source site, AstRef x, TypeRule rule)
{
	AstRef v = elf_new_unary_tree(par, site, TREE_ENFORCE, x);
	v->rule = rule;
	return v;
}

//	static AstRef tree_store(Parser *par, Source site, AstRef x, AstRef y)
//	{
//		ASSERT(istree(x));
//		ASSERT(istree(y));
//		ASSERT(y->kind != AST_RANGE_INDEX);
//		return create_binary_expr_ast(par, site, TREE_STORE, x, y);
//	}

//	static AstRef tree_add(Parser *par, Source site, AstRef x, AstRef y)
//	{
//		return create_binary_expr_ast(par, site, AST_ADD, x, y);
//	}

//	static AstRef tree_add_int(Parser *par, Source site, AstRef x, Int y)
//	{
//		return tree_add(par, site, x, tree_int(par, site, y));
//	}

// todo: intrinsic!
//	static AstRef tree_add_int_store(Parser *par, Source site, AstRef x, Int y)
//	{
//		AstRef v = tree_add_int(par, site, x, y);
//		return tree_store(par, site, x, v);
//	}

static AstRef tree_length(Parser *par, Source site, AstRef x)
{
	return elf_new_unary_tree(par, site, AST_LENGTH_INTRINSIC, x);
}

// Note, offset is an additional constant offset ...
static AstRef tree_index(Parser *par, Source site, AstRef x, AstRef y)
{
	return create_binary_expr_ast(par,site,AST_INDEX,x,y);
}

static AstRef elf_new_field_expr_tree(Parser *par, Source site, AstRef x, AstRef y)
{
	return create_binary_expr_ast(par, site, AST_FIELD, x, y);
}

static AstRef tree_ranged_index(Parser *par, Source site, AstRef x, AstRef y)
{
	return create_binary_expr_ast(par,site,AST_RANGE_INDEX,x,y);
}

static AstRef tree_less_than(Parser *par, Source site, AstRef x, AstRef y)
{
	return create_binary_expr_ast(par,site,AST_LESS_THAN,x,y);
}

static AstRef tree_eq_nil(Parser *par, Source site, AstRef x)
{
	return create_binary_expr_ast(par,site,AST_EQ,x,tree_nil(par,site));
}

static AstRef tree_meta_field(Parser *par, Source site, AstRef x, AstRef y)
{
	return create_binary_expr_ast(par,site,AST_META_FIELD,x,y);
}

static AstRef tree_closure_value(Parser *par, Source site, int x)
{
	AstRef v = tree_nullary(par, site, TREE_UPVALUE);
	v->expr_cvalue = x;
	return v;
}

static AstRef elf_new_if_stat_tree(Parser *par, Source site, AstRef pred, AstRef true_clause, AstRef else_clause)
{
	AstRef v = create_ast(par, site, AST_IF);
	v->tree_if_stat.pred = pred;
	v->tree_if_stat.true_clause = true_clause;
	v->tree_if_stat.else_clause = else_clause;
	return v;
}


static AstRef create_ident_ast(Parser *par, Source site, const char *ident)
{
	AstRef tree = create_ast(par, site, AST_IDENT);
	tree->ast_ident_expr = (char *) ident;
	return tree;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AstRef create_assign_ast(Parser *par, Source site, AstRef x, AstRef y)
{
	AstRef tree = create_ast(par, site, AST_ASSIGN);
	tree->ast_binary_expr.x = x;
	tree->ast_binary_expr.y = y;
	return tree;
}

static AstRef create_call_ast(Parser *par, Source site, AstRef expr, AstRef *args, u32 nargs)
{
	AstRef tree = create_ast(par, site, AST_CALL);
	tree->ast_call_expr.expr = expr;
	tree->ast_call_expr.args = args;
	tree->ast_call_expr.nargs = nargs;
	return tree;
}

static AstRef elf_new_comma_expr_tree(Parser *par, Source site, AstRef x, AstRef y)
{
	AstRef tree = create_binary_expr_ast(par, site, AST_COMMA_EXPR, x, y);
	return tree;
}

static AstRef elf_new_semi_colon_expr_tree(Parser *par, Source site, AstRef x, AstRef y)
{
	AstRef tree = create_binary_expr_ast(par, site, AST_SEMI_COLON_EXPR, x, y);
	return tree;
}


// Todo, remove (char *) usage from here!??
static AstRef ELF_NewMetaCallTree(Parser *par, Source site, AstRef expr, char *name, AstRef *args, u32 nargs)
{
	expr = tree_meta_field(par, site, expr, create_str_ast(par, site, name));
	return create_call_ast(par, site, expr, args, nargs);
}

// Todo, remove this from here!
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

static AstRef tree_callcoreapi(Parser *par, Source site, int id, AstRef *args, u32 nargs)
{
	AstRef expr = create_ident_ast(par, site, coreapi2s[id]);
	AstRef tree = create_call_ast(par, site, expr, args, nargs);
	return tree;
}

static AstRef tree_callcoreapi2(Parser *par, Source site, int id, AstRef x, AstRef y)
{
	AstRef args[] = {x, y};
	AstRef v = tree_callcoreapi(par, site, id, args, 2);
	return v;
}

static AstRef tree_callcoreapi3(Parser *par, Source site, int id, AstRef x, AstRef y, AstRef z)
{
	AstRef args[] = {x, y, z};
	AstRef v = tree_callcoreapi(par, site, id, args, 3);
	return v;
}

static AstRef elf_new_return_stat_tree(Parser *par, Source site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_RETURN);
	tree->tree_return_stat.expr = expr;
	return tree;
}

static AstRef tree_goto(Parser *par, Source site)
{
	return tree_nullary(par,site,TREE_GOTO);
}

static AstRef create_block_ast(Parser *par, Source site, AstRef *stats, u32 nstats)
{
	AstRef tree = create_ast(par, site, AST_BLOCK_STAT);
	tree->ast_block_stat.stats = stats;
	tree->ast_block_stat.nstats = nstats;
	return tree;
}

static AstRef create_file_ast(Parser *par, Source site, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FILE);
	tree->ast_file.body = body;
	return tree;
}

static AstRef elf_new_while_stat_tree(Parser *par, Source site, AstRef pred, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_WHILE);
	tree->tree_while_stat.pred = pred;
	tree->tree_while_stat.body = body;
	return tree;
}

static AstRef elf_new_for_stat_tree(Parser *par, Source site, AstRef decl, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FOR);
	tree->tree_for_stat.decl = decl;
	tree->tree_for_stat.body = body;
	return tree;
}

static AstRef elf_new_defer_stat_tree(Parser *par, Source site, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_DEFER_STAT);
	tree->tree_defer_stat.body = body;
	return tree;
}

static AstRef elf_new_decl_stat_tree(Parser *par, Source site, u32 tags, AstRef name, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_DECL_STAT);
	tree->ast_decl_stat.name = name;
	tree->ast_decl_stat.expr = expr;
	return tree;
}

static AstRef elf_new_break_stat_tree(Parser *par, Source site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_BREAK);
	tree->tree_break_stat.expr = expr;
	return tree;
}

static AstRef elf_new_continue_stat_tree(Parser *par, Source site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_CONTINUE);
	tree->tree_continue_stat.expr = expr;
	return tree;
}

static AstRef create_function_ast(Parser *par, Source site, AstRef *params, u32 nparams, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FUNCTION);
	tree->ast_function.params = params;
	tree->ast_function.nparams = nparams;
	tree->ast_function.body = body;
	return tree;
}

static AstRef create_param_ast(Parser *par, Source site, AstRef name, AstRef type, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_FUNCTION_PARAM);
	tree->ast_function_param.name = name;
	tree->ast_function_param.type = type;
	tree->ast_function_param.expr = expr;
	return tree;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	u32 indent;
}
Printer;

#define PRINT(pr, s, ...) do { printf(s, __VA_ARGS__); } while(0)

// for (u32 i=0; i<pr->indent;++i) printf("  ");
void elf_print_new_line(Printer *pr)
{
	PRINT(pr, "\n");
}

void elf_print_indent(Printer *pr, i32 indent)
{
	pr->indent += indent;
}


u32 elf_tree_binary_precedence(AstType kind)
{
	switch (kind)
	{
		case AST_POW:             return 12;

		case AST_MUL:             return 10;
		case AST_DIV:             return 10;
		case AST_MOD:             return 10;

		case AST_ADD:             return 10;
		case AST_SUB:             return 10;

		case AST_SHIFT_LEFT:      return 9;
		case AST_SHIFT_RIGHT:     return 9;

		case AST_LESS_THAN:       return 8;
		case AST_LESS_THAN_EQ:    return 8;
		case AST_GREATER_THAN:    return 8;
		case AST_GREATER_THAN_EQ: return 8;

		case AST_EQ:              return 7;
		case AST_NOT_EQ:          return 7;

		case AST_BITWISE_AND:     return 6;
		case AST_BITWISE_OR:      return 5;
		case AST_BITWISE_XOR:     return 4;

		case AST_AND:             return 3;
		case AST_OR:              return 2;
		case AST_NIL_AND:         return 3;
		case AST_NIL_OR:          return 2;
		case AST_ELLIPSIS:        return 1;

		default:                  return 0;
	}

	return 0;
}

void print_ast(Printer *pr, AstRef tree)
{
	if (!tree) {
		PRINT(pr, "(null)");
		return;
	}
	switch (tree->kind)
	{
		case AST_NONE:
		{
			PRINT(pr, "(none)");
		}
		break;
		case AST_IDENT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			char *ident = tree->ast_ident_expr;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "%s", ident);
		}
		break;
		case AST_STRING_LITERAL:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			char *data = tree->expr_str;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "\"%s\"", data);
		}
		break;
		case AST_INTEGER_LITERAL:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			i64 integer = tree->expr_int;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "%lli", integer);
		}
		break;
		case AST_NUMBER_LITERAL:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			f64 number = tree->expr_num;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "%f", number);
		}
		break;
		case AST_ELLIPSIS:
		{
			PRINT(pr, "...");
		}
		break;
		case AST_FILE:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef body = tree->ast_file.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, body);
		}
		break;

		case AST_FUNCTION:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef *params    = tree->ast_function.params;
			u32     nparams   = tree->ast_function.nparams;
			AstRef  body      = tree->ast_function.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "function(");
			for (u32 i = 0; i < nparams; ++ i)
			{
				if (i != 0) PRINT(pr, ", ");
				print_ast(pr, params[i]);
			}
			PRINT(pr, ")");
			print_ast(pr, body);
		}
		break;

		case AST_DEFER_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef  body = tree->tree_defer_stat.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "defer ");
			print_ast(pr, body);
		}
		break;

		case AST_WHILE:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef pred = tree->tree_while_stat.pred;
			AstRef body = tree->tree_while_stat.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "while ");
			print_ast(pr, pred);
			PRINT(pr, " ? ");
			print_ast(pr, body);
		}
		break;

		case AST_FOR:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef decl = tree->tree_for_stat.decl;
			AstRef body = tree->tree_for_stat.body;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "for ");
			print_ast(pr, decl);
			PRINT(pr, " ? ");
			print_ast(pr, body);
		}
		break;

		case AST_FUNCTION_PARAM:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef name = tree->ast_function_param.name;
			AstRef type = tree->ast_function_param.type;
			AstRef expr = tree->ast_function_param.expr;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, name);
			if (type) {
				PRINT(pr, ": ");
				print_ast(pr, type);
			}
			if (expr) {
				PRINT(pr, " = ");
				print_ast(pr, expr);
			}
		}
		break;

		case AST_IF:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef pred = tree->tree_if_stat.pred;
			AstRef true_clause = tree->tree_if_stat.true_clause;
			AstRef else_clause = tree->tree_if_stat.else_clause;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "if ");
			print_ast(pr, pred);
			PRINT(pr, "?");
			print_ast(pr, true_clause);
			if (else_clause) {
				PRINT(pr, "else ");
				print_ast(pr, true_clause);
			}
		}
		break;
		case AST_BLOCK_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef *stats = tree->ast_block_stat.stats;
			u32 nstats = tree->ast_block_stat.nstats;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "{");
			elf_print_new_line(pr );
			elf_print_indent(pr, +1);
			for (u32 i = 0; i < nstats; ++ i)
			{
				print_ast(pr, stats[i]);
				elf_print_new_line(pr);
			}
			elf_print_indent(pr, -1);
			PRINT(pr, "}");
		}
		break;

		case AST_FIELD:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, x);
			PRINT(pr, ".");
			print_ast(pr, y);
		}
		break;

		case AST_CALL:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef expr = tree->ast_call_expr.expr;
			AstRef *args = tree->ast_call_expr.args;
			u32 nargs = tree->ast_call_expr.nargs;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, expr);
			PRINT(pr, "(");
			for (u32 i = 0; i < nargs; ++ i)
			{
				if (i != 0) PRINT(pr, ", ");
				print_ast(pr, args[i]);
			}
			PRINT(pr, ")");
		}
		break;

		case AST_DECL_STAT:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef name = tree->ast_decl_stat.name;
			AstRef expr = tree->ast_decl_stat.expr;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, name);
			PRINT(pr, " := ");
			print_ast(pr, expr);
		}
		break;

		case AST_ASSIGN:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, x);
			PRINT(pr, " = ");
			print_ast(pr, y);
		}
		break;

		case AST_LESS_THAN:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "(");
			print_ast(pr, x);
			PRINT(pr, " < ");
			print_ast(pr, y);
			PRINT(pr, ")");
		}
		break;

		case AST_ADD:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "(");
			print_ast(pr, x);
			PRINT(pr, " + ");
			print_ast(pr, y);
			PRINT(pr, ")");
		}
		break;

		case AST_MUL:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "(");
			print_ast(pr, x);
			PRINT(pr, " * ");
			print_ast(pr, y);
			PRINT(pr, ")");
		}
		break;

		case AST_COMMA_EXPR:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "(");
			print_ast(pr, x);
			PRINT(pr, ", ");
			print_ast(pr, y);
			PRINT(pr, ")");
		}
		break;

		case AST_TUPLE:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef *args = tree->ast_tuple_expr.args;
			u32 nargs = tree->ast_tuple_expr.nargs;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			PRINT(pr, "(");
			for (u32 i = 0; i < nargs; ++ i)
			{
				if (i != 0) PRINT(pr, ", ");
				print_ast(pr, args[i]);
			}
			PRINT(pr, ")");
		}
		break;

		case AST_SEMI_COLON_EXPR:
		{
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			AstRef x = tree->ast_binary_expr.x;
			AstRef y = tree->ast_binary_expr.y;
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			print_ast(pr, x);
			PRINT(pr, "; ");
			print_ast(pr, y);
		}
		break;

		default:
		{
			PRINT(pr, "(unknown)");
		}
		break;
	}
}























