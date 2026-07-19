//
// See Copyright Notice In elf.h
//

static const char *ast_type_name(AstType type)
{
	static const char *names[] =
	{
#define AST_XPAND(ENUM, NAME) #ENUM,
		AST_XDEF(AST_XPAND)
#undef AST_XPAND
	};

	if ((u32)type >= AST_COUNT_) {
		return "AST_UNKNOWN";
	}
	return names[type];
}

static AstContext create_ast_context(elf_Arena *arena)
{
	u32 stack_size = 4096;

	AstContext ast = {};
	ast.arena = arena;
	ast.stack = elf_arena_push_zero(arena, sizeof(*ast.stack) * stack_size);
	ast.stack_size = stack_size;
	ast.stack_index = 0;
	return ast;
}

static AstRef create_ast(Parser *par, SourceSite site, AstType kind)
{
	Ast *tree = elf_arena_push_zero(par->ast.arena, sizeof(*tree));
	tree->site = site;
	tree->kind = kind;
	return tree;
}

static AstRef create_nullary_ast(Parser *par, SourceSite site, AstType kind)
{
	AstRef tree = create_ast(par,site,kind);
	return tree;
}

static AstRef create_unary_ast(Parser *par, SourceSite site, AstType kind, AstRef x)
{
	AstRef tree = create_ast(par, site, kind);
	tree->unary = x;
	return tree;
}

static AstRef create_binary_ast(Parser *par, SourceSite site, AstType kind, AstRef x, AstRef y)
{
	AstRef tree = create_ast(par, site, kind);
	tree->binary.x = x;
	tree->binary.y = y;
	return tree;
}

static AstRef create_nil_ast(Parser *par, SourceSite site)
{
	return create_nullary_ast(par, site, AST_NIL_LITERAL);
}

static AstRef create_int_ast(Parser *par, SourceSite site, i64 i)
{
	AstRef value;
	value = create_nullary_ast(par, site, AST_INTEGER_LITERAL);
	value->integer_value = i;
	return value;
}

static AstRef create_num_ast(Parser *par, SourceSite site, f64 n)
{
	AstRef value;
	value=create_nullary_ast(par, site, AST_NUMBER_LITERAL);
	value->number_value=n;
	return value;
}

static AstRef create_atom_ast(Parser *par, SourceSite site, elf_String *atom)
{
	AstRef value = create_nullary_ast(par, site, AST_STRING_LITERAL);
	value->atom = atom;
	return value;
}

static AstRef create_table_ast(Parser *par, SourceSite site, AstRef *args, u32 nargs)
{
	AstRef tree = create_ast(par, site, AST_TABLE);
	tree->table.args = args;
	tree->table.nargs = nargs;
	return tree;
}

static AstRef create_table_entry_ast(Parser *par, SourceSite site, AstRef key, AstRef value)
{
	AstRef tree = create_ast(par, site, AST_TABLE_ENTRY);
	tree->table_entry.key = key;
	tree->table_entry.value = value;
	return tree;
}

static AstRef create_tuple_ast(Parser *par, SourceSite site, AstRef *args, u32 nargs)
{
	ASSERT(nargs >= 1);
	AstRef tree = create_ast(par, site, AST_TUPLE);
	tree->tuple.args = args;
	tree->tuple.nargs = nargs;
	return tree;
}

static AstRef create_ellipsis_ast(Parser *par, SourceSite site)
{
	AstRef tree = create_ast(par, site, AST_ELLIPSIS);
	return tree;
}

static AstRef create_length_ast(Parser *par, SourceSite site, AstRef x)
{
	return create_unary_ast(par, site, AST_LENGTH_INTRINSIC, x);
}

static AstRef create_get_mem_ast(Parser *par, SourceSite site, AstRef x)
{
	return create_unary_ast(par, site, AST_GET_MEM, x);
}

static AstRef create_index_ast(Parser *par, SourceSite site, AstRef x, AstRef y)
{
	return create_binary_ast(par, site, AST_INDEX, x, y);
}

static AstRef create_field_ast(Parser *par, SourceSite site, AstRef x, AstRef y)
{
	return create_binary_ast(par, site, AST_FIELD, x, y);
}

static AstRef create_range_index_ast(Parser *par, SourceSite site, AstRef x, AstRef y)
{
	return create_binary_ast(par, site, AST_RANGE_INDEX, x, y);
}

static AstRef create_meta_field_ast(Parser *par, SourceSite site, AstRef x, AstRef y)
{
	return create_binary_ast(par,site,AST_META_FIELD,x,y);
}

static AstRef create_if_ast(Parser *par, SourceSite site, AstRef pred, AstRef true_clause, AstRef else_clause)
{
	AstRef value = create_ast(par, site, AST_IF);
	value->if_stat.pred = pred;
	value->if_stat.true_clause = true_clause;
	value->if_stat.else_clause = else_clause;
	return value;
}

static AstRef create_ident_ast(Parser *par, SourceSite site, elf_String *atom)
{
	AstRef tree = create_ast(par, site, AST_IDENT);
	tree->atom = atom;
	return tree;
}

// Todo, remove
static AstRef create_dotted_ident_ast(Parser *par, SourceSite site, const char *text)
{
	const char *segment = text;
	const char *cursor = text;
	AstRef expr = 0;
	for (;; ++ cursor)
	{
		if (*cursor == '.' || *cursor == 0)
		{
			u32 segment_size = (u32)(cursor - segment);
			ASSERT(segment_size != 0);

			elf_String *atom = elf_atom_from_data_size(par->state, segment, segment_size);
			AstRef part = create_ident_ast(par, site, atom);
			if (expr) {
				expr = create_field_ast(par, site, expr, part);
			}
			else {
				expr = part;
			}

			if (*cursor == 0) {
				break;
			}
			segment = cursor + 1;
		}
	}
	return expr;
}

static AstRef create_assign_ast(Parser *par, SourceSite site, AstRef x, AstRef y)
{
	AstRef tree = create_ast(par, site, AST_ASSIGN);
	tree->binary.x = x;
	tree->binary.y = y;
	return tree;
}

static AstRef create_call_ast(Parser *par, SourceSite site, AstRef expr, AstRef *args, u32 nargs)
{
	AstRef tree = create_ast(par, site, AST_CALL);
	tree->call.expr = expr;
	tree->call.args = args;
	tree->call.nargs = nargs;
	return tree;
}

static AstRef create_for_steps_ast(Parser *par, SourceSite site, AstRef *args, u32 nargs)
{
	AstRef tree = create_ast(par, site, AST_FOR_STEPS);
	tree->for_steps.args = args;
	tree->for_steps.nargs = nargs;
	return tree;
}

static AstRef create_return_ast(Parser *par, SourceSite site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_RETURN);
	tree->return_stat.expr = expr;
	return tree;
}

static AstRef create_block_ast(Parser *par, SourceSite site, AstRef *stats, u32 nstats)
{
	AstRef tree = create_ast(par, site, AST_BLOCK_STAT);
	tree->block.stats = stats;
	tree->block.nstats = nstats;
	return tree;
}

static AstRef create_file_ast(Parser *par, SourceSite site, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FILE);
	tree->file.body = body;
	return tree;
}

static AstRef create_while_ast(Parser *par, SourceSite site, AstRef pred, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_WHILE);
	tree->while_stat.pred = pred;
	tree->while_stat.body = body;
	return tree;
}

static AstRef create_for_ast(Parser *par, SourceSite site, AstRef decl, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FOR);
	tree->for_stat.decl = decl;
	tree->for_stat.body = body;
	return tree;
}

static AstRef create_defer_ast(Parser *par, SourceSite site, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_DEFER_STAT);
	tree->defer_stat.body = body;
	return tree;
}

static AstRef create_decl_ast(Parser *par, SourceSite site, u32 tags, AstRef name, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_DECL_STAT);
	tree->decl.name = name;
	tree->decl.expr = expr;
	return tree;
}

static AstRef create_break_ast(Parser *par, SourceSite site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_BREAK);
	tree->break_stat.expr = expr;
	return tree;
}

static AstRef create_continue_ast(Parser *par, SourceSite site, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_CONTINUE);
	tree->continue_stat.expr = expr;
	return tree;
}

static AstRef create_function_ast(Parser *par, SourceSite site, AstRef *params, u32 nparams, AstRef variadic, AstRef body)
{
	AstRef tree = create_ast(par, site, AST_FUNCTION);
	tree->function.params = params;
	tree->function.nparams = nparams;
	tree->function.variadic = variadic;
	tree->function.body = body;
	return tree;
}

static AstRef create_param_ast(Parser *par, SourceSite site, AstRef name, AstRef type, AstRef expr)
{
	AstRef tree = create_ast(par, site, AST_FUNCTION_PARAM);
	tree->param.name = name;
	tree->param.type = type;
	tree->param.expr = expr;
	return tree;
}

// Todo, this is all deprecated!
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFCOREAPI(_) \
_(ASSERT,   "elf.assert"   , 0,  2,  0) \
_(FORMAT,   "elf.format"   , 0,  1,  2) \
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

static AstRef create_core_api_call_ast(Parser *par, SourceSite site, int id, AstRef *args, u32 nargs)
{
	AstRef expr = create_dotted_ident_ast(par, site, coreapi2s[id]);
	AstRef tree = create_call_ast(par, site, expr, args, nargs);
	return tree;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	u32 indent;
}
Printer;

#define PRINT(pr, s, ...) do { printf(s, __VA_ARGS__); } while(0)

static void print_ast(Printer *pr, AstRef tree);

static void elf_print_new_line(Printer *pr)
{
	PRINT(pr, "\n");
}

static void elf_print_indent(Printer *pr, i32 indent)
{
	pr->indent += indent;
}

static void print_ast_list(Printer *pr, AstRef *items, u32 count)
{
	for (u32 i = 0; i < count; ++ i)
	{
		if (i != 0) {
			PRINT(pr, ", ");
		}
		print_ast(pr, items[i]);
	}
}

static const char *ast_binary_operator(AstType kind)
{
	switch (kind)
	{
		case AST_AND:             return "&&";
		case AST_OR:              return "||";
		case AST_ADD:             return "+";
		case AST_SUB:             return "-";
		case AST_MUL:             return "*";
		case AST_DIV:             return "/";
		case AST_POW:             return "**";
		case AST_MOD:             return "%";
		case AST_EQ:              return "==";
		case AST_NOT_EQ:          return "!=";
		case AST_LESS_THAN:       return "<";
		case AST_LESS_THAN_EQ:    return "<=";
		case AST_GREATER_THAN:    return ">";
		case AST_GREATER_THAN_EQ: return ">=";
		case AST_NIL_AND:         return "!!";
		case AST_NIL_OR:          return "??";
		case AST_SHIFT_LEFT:      return "<<";
		case AST_SHIFT_RIGHT:     return ">>";
		case AST_BITWISE_AND:     return "&";
		case AST_BITWISE_OR:      return "|";
		case AST_BITWISE_XOR:     return "^";
		case AST_ADD_ASSIGN:      return "+=";
		case AST_SUB_ASSIGN:      return "-=";
		case AST_MUL_ASSIGN:      return "*=";
		case AST_DIV_ASSIGN:      return "/=";
		case AST_MOD_ASSIGN:      return "%=";
		case AST_XOR_ASSIGN:      return "^=";
		case AST_SHL_ASSIGN:      return "<<=";
		case AST_SHR_ASSIGN:      return ">>=";
		case AST_NIL_ASSIGN:      return "?=";
		default:                  return 0;
	}
}

static void print_binary_ast(Printer *pr, AstRef tree, const char *op)
{
	PRINT(pr, "(");
	print_ast(pr, tree->binary.x);
	PRINT(pr, " %s ", op);
	print_ast(pr, tree->binary.y);
	PRINT(pr, ")");
}

static void print_ast(Printer *pr, AstRef tree)
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
			PRINT(pr, "%s", atom_data(tree->atom));
		}
		break;
		case AST_NIL_LITERAL:
		{
			PRINT(pr, "nil");
		}
		break;
		case AST_STRING_LITERAL:
		{
			PRINT(pr, "\"%s\"", atom_data(tree->atom));
		}
		break;
		case AST_INTEGER_LITERAL:
		{

			i64 integer = tree->integer_value;


			PRINT(pr, "%lli", integer);
		}
		break;
		case AST_NUMBER_LITERAL:
		{

			f64 number = tree->number_value;


			PRINT(pr, "%f", number);
		}
		break;
		case AST_ERROR:
		{
			PRINT(pr, "(error)");
		}
		break;
		case AST_ELLIPSIS:
		{
			PRINT(pr, "...");
		}
		break;
		case AST_GET_MEM:
		{
			PRINT(pr, "#get_mem(");
			print_ast(pr, tree->unary);
			PRINT(pr, ")");
		}
		break;
		case AST_FILE:
		{

			AstRef body = tree->file.body;


			print_ast(pr, body);
		}
		break;

		case AST_FUNCTION:
		{

			AstRef *params    = tree->function.params;
			u32     nparams   = tree->function.nparams;
			AstRef  variadic  = tree->function.variadic;
			AstRef  body      = tree->function.body;


			PRINT(pr, "fun(");
			print_ast_list(pr, params, nparams);
			if (variadic)
			{
				if (nparams) {
					PRINT(pr, ", ");
				}
				print_ast(pr, variadic);
			}
			PRINT(pr, ")");
			print_ast(pr, body);
		}
		break;

		case AST_DEFER_STAT:
		{

			AstRef  body = tree->defer_stat.body;


			PRINT(pr, "defer ");
			print_ast(pr, body);
		}
		break;

		case AST_WHILE:
		{

			AstRef pred = tree->while_stat.pred;
			AstRef body = tree->while_stat.body;


			PRINT(pr, "while ");
			print_ast(pr, pred);
			PRINT(pr, " ? ");
			print_ast(pr, body);
		}
		break;

		case AST_FOR:
		{

			AstRef decl = tree->for_stat.decl;
			AstRef body = tree->for_stat.body;


			PRINT(pr, "for ");
			print_ast(pr, decl);
			PRINT(pr, " ? ");
			print_ast(pr, body);
		}
		break;

		case AST_FUNCTION_PARAM:
		{

			AstRef name = tree->param.name;
			AstRef type = tree->param.type;
			AstRef expr = tree->param.expr;


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

			AstRef pred = tree->if_stat.pred;
			AstRef true_clause = tree->if_stat.true_clause;
			AstRef else_clause = tree->if_stat.else_clause;


			PRINT(pr, "if ");
			print_ast(pr, pred);
			PRINT(pr, "?");
			print_ast(pr, true_clause);
			if (else_clause) {
				PRINT(pr, "else ");
				print_ast(pr, else_clause);
			}
		}
		break;
		case AST_BLOCK_STAT:
		{

			AstRef *stats = tree->block.stats;
			u32 nstats = tree->block.nstats;


			PRINT(pr, "{");
			elf_print_new_line(pr);
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

			AstRef x = tree->binary.x;
			AstRef y = tree->binary.y;


			print_ast(pr, x);
			PRINT(pr, ".");
			print_ast(pr, y);
		}
		break;
		case AST_META_FIELD:
		{
			print_ast(pr, tree->binary.x);
			PRINT(pr, ":");
			print_ast(pr, tree->binary.y);
		}
		break;
		case AST_INDEX:
		case AST_RANGE_INDEX:
		{
			print_ast(pr, tree->binary.x);
			PRINT(pr, "[");
			print_ast(pr, tree->binary.y);
			PRINT(pr, "]");
		}
		break;

		case AST_CALL:
		{

			AstRef expr = tree->call.expr;
			AstRef *args = tree->call.args;
			u32 nargs = tree->call.nargs;


			print_ast(pr, expr);
			PRINT(pr, "(");
			print_ast_list(pr, args, nargs);
			PRINT(pr, ")");
		}
		break;

		case AST_DECL_STAT:
		{

			AstRef name = tree->decl.name;
			AstRef expr = tree->decl.expr;


			print_ast(pr, name);
			PRINT(pr, " := ");
			print_ast(pr, expr);
		}
		break;

		case AST_ASSIGN:
		{

			AstRef x = tree->binary.x;
			AstRef y = tree->binary.y;


			print_ast(pr, x);
			PRINT(pr, " = ");
			print_ast(pr, y);
		}
		break;
		case AST_RANGE:
		{

			AstRef x = tree->binary.x;
			AstRef y = tree->binary.y;


			PRINT(pr, "(");
			print_ast(pr, x);
			PRINT(pr, "...");
			print_ast(pr, y);
			PRINT(pr, ")");
		}
		break;

		case AST_COMMA_EXPR:
		{
			print_binary_ast(pr, tree, ",");
		}
		break;

		case AST_TUPLE:
		{

			AstRef *args = tree->tuple.args;
			u32 nargs = tree->tuple.nargs;


			PRINT(pr, "(");
			print_ast_list(pr, args, nargs);
			PRINT(pr, ")");
		}
		break;
		case AST_TABLE:
		{
			PRINT(pr, "{");
			print_ast_list(pr, tree->table.args, tree->table.nargs);
			PRINT(pr, "}");
		}
		break;
		case AST_TABLE_ENTRY:
		{
			print_ast(pr, tree->table_entry.key);
			PRINT(pr, ": ");
			print_ast(pr, tree->table_entry.value);
		}
		break;

		case AST_FOR_STEPS:
		{
			for (u32 i = 0; i < tree->for_steps.nargs; ++ i)
			{
				if (i != 0) {
					PRINT(pr, "; ");
				}
				print_ast(pr, tree->for_steps.args[i]);
			}
		}
		break;
		case AST_RETURN:
		{
			PRINT(pr, "ret");
			if (tree->return_stat.expr) {
				PRINT(pr, " ");
				print_ast(pr, tree->return_stat.expr);
			}
		}
		break;
		case AST_BREAK:
		{
			PRINT(pr, "break");
			if (tree->break_stat.expr) {
				PRINT(pr, " ");
				print_ast(pr, tree->break_stat.expr);
			}
		}
		break;
		case AST_CONTINUE:
		{
			PRINT(pr, "continue");
			if (tree->continue_stat.expr) {
				PRINT(pr, " ");
				print_ast(pr, tree->continue_stat.expr);
			}
		}
		break;
		case AST_BITWISE_NOT:
		{
			PRINT(pr, "~");
			print_ast(pr, tree->unary);
		}
		break;
		case AST_LENGTH_INTRINSIC:
		{
			PRINT(pr, "#");
			print_ast(pr, tree->unary);
		}
		break;
		case AST_AND:
		case AST_OR:
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_POW:
		case AST_MOD:
		case AST_EQ:
		case AST_NOT_EQ:
		case AST_LESS_THAN:
		case AST_LESS_THAN_EQ:
		case AST_GREATER_THAN:
		case AST_GREATER_THAN_EQ:
		case AST_NIL_AND:
		case AST_NIL_OR:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		case AST_BITWISE_XOR:
		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_MOD_ASSIGN:
		case AST_XOR_ASSIGN:
		case AST_SHL_ASSIGN:
		case AST_SHR_ASSIGN:
		case AST_NIL_ASSIGN:
		{
			print_binary_ast(pr, tree, ast_binary_operator(tree->kind));
		}
		break;

		default:
		{
			PRINT(pr, "(%s)", ast_type_name(tree->kind));
		}
		break;
	}
}























