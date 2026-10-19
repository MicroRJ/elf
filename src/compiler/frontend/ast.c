//
// See Copyright Notice In elf.h
//

// AST constructors only allocate and initialize the returned tree.
// README: use ast_new_ *** _expr naming convention
// README: use ast_new_ *** _stat naming convention

Ast_T ast_error_sentinel =
{
	.kind = AST_ERROR,
	.status = COM_STATUS_REPORTED,
};

Ast_T ast_error_oom_sentinel =
{
	.kind = AST_ERROR,
	.status = COM_STATUS_OOM,
};

static b32 ast_list_push(Ast_List *list, Arena *arena, Ast tree)
{
	ASSERT(list);
	ASSERT(arena);
	Ast_Node *node = arena_push_zero(arena, sizeof(*node), _Alignof(Ast_Node));
	if (!node) {
		return false;
	}
	node->tree = tree;
	if (list->last) {
		list->last->next = node;
	}
	else {
		list->first = node;
	}
	list->last = node;
	list->count += 1;
	return true;
}

static Ast_Array ast_list_compact(Ast_List *list, Arena *arena)
{
	ASSERT(list);
	ASSERT(arena);
	Ast_Array result = {.count = list->count};
	if (!result.count) {
		return result;
	}
	result.items = arena_push(arena, sizeof(*result.items) * result.count, _Alignof(Ast));
	if (!result.items) {
		return (Ast_Array) {};
	}
	u32 index = 0;
	for (Ast_Node *node = list->first; node; node = node->next)
	{
		ASSERT(index < result.count);
		result.items[index++] = node->tree;
	}
	ASSERT(index == result.count);
	return result;
}

static Ast ast_new(Parser *par, SourceSite site, Ast_Kind kind)
{
	ASSERT(par->source);
	Ast_T *tree = arena_push_zero(par->result_arena, sizeof(*tree), _Alignof(Ast_T));
	if (!tree) {
		return ERROR_OOM_AST;
	}
	tree->site = site;
	tree->kind = kind;
	return tree;
}

static Ast ast_new_nullary_expr(Parser *par, SourceSite site, Ast_Kind kind)
{
	Ast tree = ast_new(par,site,kind);
	return tree;
}

static Ast ast_new_unary_expr(Parser *par, SourceSite site, Ast_Kind kind, Ast x)
{
	if (ast_is_error(x)) {
		return x;
	}
	Ast tree = ast_new(par, site, kind);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->unary = x;
	return tree;
}

static Ast ast_new_binary_expr(Parser *par, SourceSite site, Ast_Kind kind, Ast x, Ast y)
{
	if (ast_is_error(x)) {
		return x;
	}
	if (ast_is_error(y)) {
		return y;
	}
	Ast tree = ast_new(par, site, kind);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->binary.x = x;
	tree->binary.y = y;
	return tree;
}

static Ast ast_new_nil_literal(Parser *par, SourceSite site)
{
	return ast_new_nullary_expr(par, site, AST_NIL_LITERAL);
}

static Ast ast_new_integer_literal(Parser *par, SourceSite site, i64 i)
{
	Ast value;
	value = ast_new_nullary_expr(par, site, AST_INTEGER_LITERAL);
	if (ast_is_error(value)) {
		return value;
	}
	value->integer_value = i;
	return value;
}

static Ast ast_new_number_literal(Parser *par, SourceSite site, f64 n)
{
	Ast value = ast_new_nullary_expr(par, site, AST_NUMBER_LITERAL);
	if (ast_is_error(value)) {
		return value;
	}
	value->number_value=n;
	return value;
}

static Ast ast_new_string_literal(Parser *par, SourceSite site, Atom *atom)
{
	Ast value = ast_new_nullary_expr(par, site, AST_STRING_LITERAL);
	if (ast_is_error(value)) {
		return value;
	}
	value->atom = atom;
	return value;
}

static Ast create_interpolated_string_ast(Parser *par, SourceSite site, Ast_Array parts)
{
	ASSERT(parts.count >= 3);
	ASSERT((parts.count & 1) != 0);
	Ast value = ast_new(par, site, AST_INTERPOLATED_STRING);
	if (ast_is_error(value)) {
		return value;
	}
	value->interpolated_string = parts;
	return value;
}

static Ast ast_new_import(Parser *par, SourceSite site, Atom *path)
{
	Ast value = ast_new_nullary_expr(par, site, AST_IMPORT);
	if (ast_is_error(value)) {
		return value;
	}
	value->import.path = path;
	return value;
}

static Ast create_table_ast(Parser *par, SourceSite site, Ast_Array args)
{
	Ast tree = ast_new(par, site, AST_TABLE);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->table = args;
	return tree;
}

static Ast create_array_ast(Parser *par, SourceSite site, Ast_Array items)
{
	Ast tree = ast_new(par, site, AST_ARRAY);
	if (ast_is_error(tree))
	{
		return tree;
	}
	tree->array = items;
	return tree;
}

static Ast create_table_entry_ast(Parser *par, SourceSite site, Ast key, Ast value)
{
	if (ast_is_error(key)) {
		return key;
	}
	if (ast_is_error(value)) {
		return value;
	}
	Ast tree = ast_new(par, site, AST_TABLE_ENTRY);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->table_entry.key = key;
	tree->table_entry.value = value;
	return tree;
}

static Ast create_tuple_ast(Parser *par, SourceSite site, Ast_Array args)
{
	ASSERT(args.count >= 1);
	Ast tree = ast_new(par, site, AST_TUPLE);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->tuple = args;
	return tree;
}

static Ast create_ellipsis_ast(Parser *par, SourceSite site)
{
	Ast tree = ast_new(par, site, AST_ELLIPSIS);
	return tree;
}

static Ast ast_new_measure_intrin(Parser *par, SourceSite site, Ast x)
{
	return ast_new_unary_expr(par, site, AST_MEASURE, x);
}

static Ast create_get_mem_ast(Parser *par, SourceSite site, Ast x)
{
	return ast_new_unary_expr(par, site, AST_GET_MEM, x);
}

static Ast create_index_ast(Parser *par, SourceSite site, Ast x, Ast y)
{
	return ast_new_binary_expr(par, site, AST_INDEX, x, y);
}

static Ast create_field_ast(Parser *par, SourceSite site, Ast x, Ast y)
{
	return ast_new_binary_expr(par, site, AST_FIELD, x, y);
}

static Ast create_range_index_ast(Parser *par, SourceSite site, Ast x, Ast y)
{
	return ast_new_binary_expr(par, site, AST_RANGE_INDEX, x, y);
}

static Ast create_meta_field_ast(Parser *par, SourceSite site, Ast x, Ast y)
{
	return ast_new_binary_expr(par,site,AST_META_FIELD,x,y);
}

static Ast create_if_ast(Parser *par, SourceSite site, Ast pred, Ast true_clause, Ast else_clause)
{
	if (ast_is_error(pred)) {
		return pred;
	}
	if (ast_is_error(true_clause)) {
		return true_clause;
	}
	if (ast_is_error(else_clause)) {
		return else_clause;
	}
	Ast value = ast_new(par, site, AST_IF);
	if (ast_is_error(value)) {
		return value;
	}
	value->if_stat.pred = pred;
	value->if_stat.true_clause = true_clause;
	value->if_stat.else_clause = else_clause;
	return value;
}

static Ast create_ident_ast(Parser *par, SourceSite site, Atom *atom)
{
	Ast tree = ast_new(par, site, AST_IDENT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->atom = atom;
	return tree;
}

static Ast create_assign_ast(Parser *par, SourceSite site, Ast x, Ast y)
{
	if (ast_is_error(x)) {
		return x;
	}
	if (ast_is_error(y)) {
		return y;
	}
	Ast tree = ast_new(par, site, AST_ASSIGN);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->binary.x = x;
	tree->binary.y = y;
	return tree;
}

static Ast create_call_ast(Parser *par, SourceSite site, Ast expr, Ast_Array args)
{
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_CALL);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->call.expr = expr;
	tree->call.args = args;
	return tree;
}

static Ast create_named_argument_ast(Parser *par, SourceSite site, Ast name, Ast expr)
{
	if (ast_is_error(name)) {
		return name;
	}
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_NAMED_ARGUMENT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->named_argument.name = name;
	tree->named_argument.expr = expr;
	return tree;
}

static Ast create_return_ast(Parser *par, SourceSite site, Ast expr)
{
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_RETURN);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->return_stat.expr = expr;
	return tree;
}

static Ast create_block_ast(Parser *par, SourceSite site, Ast_Array stats)
{
	Ast tree = ast_new(par, site, AST_BLOCK_STAT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->block = stats;
	return tree;
}

static Ast create_file_ast(Parser *par, SourceSite site)
{
	Ast tree = ast_new(par, site, AST_FILE);
	return tree;
}

static Ast create_while_ast(Parser *par, SourceSite site, Ast pred, Ast body)
{
	if (ast_is_error(pred)) {
		return pred;
	}
	if (ast_is_error(body)) {
		return body;
	}
	Ast tree = ast_new(par, site, AST_WHILE);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->while_stat.pred = pred;
	tree->while_stat.body = body;
	return tree;
}

static Ast create_for_ast(Parser *par, SourceSite site, Ast init, Ast pred, Ast step, Ast body)
{
	if (ast_is_error(init)) {
		return init;
	}
	if (ast_is_error(pred)) {
		return pred;
	}
	if (ast_is_error(step)) {
		return step;
	}
	if (ast_is_error(body)) {
		return body;
	}
	Ast tree = ast_new(par, site, AST_FOR);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->for_stat.init = init;
	tree->for_stat.pred = pred;
	tree->for_stat.step = step;
	tree->for_stat.body = body;
	return tree;
}

static Ast create_for_range_ast(Parser *par, SourceSite site, Ast decl, Ast body)
{
	if (ast_is_error(decl)) {
		return decl;
	}
	if (ast_is_error(body)) {
		return body;
	}
	Ast tree = ast_new(par, site, AST_FOR_RANGE);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->for_range_stat.decl = decl;
	tree->for_range_stat.body = body;
	return tree;
}

static Ast create_defer_ast(Parser *par, SourceSite site, Ast body)
{
	if (ast_is_error(body)) {
		return body;
	}
	Ast tree = ast_new(par, site, AST_DEFER_STAT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->defer_stat.body = body;
	return tree;
}

static Ast create_decl_ast(Parser *par, SourceSite site, b32 constant, Ast name, Ast expr)
{
	if (ast_is_error(name)) {
		return name;
	}
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_DECL_STAT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->decl.constant = constant;
	tree->decl.name = name;
	tree->decl.expr = expr;
	return tree;
}

static Ast create_static_decl_ast(Parser *par, SourceSite site, Ast name, Ast expr, b32 exported)
{
	if (ast_is_error(name)) {
		return name;
	}
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_STATIC_DECL);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->static_decl.exported = exported;
	tree->static_decl.name = name;
	tree->static_decl.expr = expr;
	return tree;
}

static Ast create_break_ast(Parser *par, SourceSite site, Ast expr)
{
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_BREAK);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->break_stat.expr = expr;
	return tree;
}

static Ast create_continue_ast(Parser *par, SourceSite site, Ast expr)
{
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_CONTINUE);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->continue_stat.expr = expr;
	return tree;
}

static Ast create_function_ast(Parser *par, SourceSite site, Ast parent)
{
	ASSERT(parent);
	ASSERT(parent->kind == AST_FILE || parent->kind == AST_FUNCTION);
	Ast tree = ast_new(par, site, AST_FUNCTION);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->function.parent = parent;
	return tree;
}

static Ast create_param_ast(Parser *par, SourceSite site, Ast name, Ast type, Ast expr)
{
	if (ast_is_error(name)) {
		return name;
	}
	if (ast_is_error(type)) {
		return type;
	}
	if (ast_is_error(expr)) {
		return expr;
	}
	Ast tree = ast_new(par, site, AST_DECL_STAT);
	if (ast_is_error(tree)) {
		return tree;
	}
	tree->decl.name = name;
	tree->decl.type = type;
	tree->decl.expr = expr;
	return tree;
}

typedef struct
{
	u32 indent;
}
Printer;

#define PRINT(pr, s, ...) do { printf(s, __VA_ARGS__); } while(0)

static void print_ast(Printer *pr, Ast tree);

static void elf_print_new_line(Printer *pr)
{
	PRINT(pr, "\n");
}

static void elf_print_indent(Printer *pr, i32 indent)
{
	pr->indent += indent;
}

static void print_ast_list(Printer *pr, Ast *items, u32 count)
{
	for (u32 i = 0; i < count; ++ i)
	{
		if (i != 0) {
			PRINT(pr, ", ");
		}
		print_ast(pr, items[i]);
	}
}

static const char *ast_binary_operator(Ast_Kind kind)
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

static void print_binary_ast(Printer *pr, Ast tree, const char *op)
{
	PRINT(pr, "(");
	print_ast(pr, tree->binary.x);
	PRINT(pr, " %s ", op);
	print_ast(pr, tree->binary.y);
	PRINT(pr, ")");
}

static void print_ast(Printer *pr, Ast tree)
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
		case AST_INTERPOLATED_STRING:
		{
			PRINT(pr, "f[");
			print_ast_list(pr, tree->interpolated_string.items, tree->interpolated_string.count);
			PRINT(pr, "]");
		}
		break;
		case AST_IMPORT:
		{
			PRINT(pr, "import \"%s\"", atom_data(tree->import.path));
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

			Ast body = tree->file.body;


			print_ast(pr, body);
		}
		break;

		case AST_FUNCTION:
		{

			Ast *params    = tree->function.params.items;
			u32  nparams   = tree->function.params.count;
			b32  variadic  = tree->function.variadic;
			Ast  body      = tree->function.body;


			PRINT(pr, "fun(");
			for (u32 i = 0; i < nparams; ++i)
			{
				Ast param = params[i];
				ASSERT(param);
				ASSERT(param->kind == AST_DECL_STAT);
				if (i) {
					PRINT(pr, ", ");
				}
				print_ast(pr, param->decl.name);
				if (param->decl.type)
				{
					PRINT(pr, ": ");
					print_ast(pr, param->decl.type);
				}
				if (param->decl.expr)
				{
					PRINT(pr, " = ");
					print_ast(pr, param->decl.expr);
				}
			}
			if (variadic)
			{
				if (nparams) {
					PRINT(pr, ", ");
				}
				PRINT(pr, "...");
			}
			PRINT(pr, ")");
			print_ast(pr, body);
		}
		break;

		case AST_DEFER_STAT:
		{

			Ast  body = tree->defer_stat.body;


			PRINT(pr, "defer ");
			print_ast(pr, body);
		}
		break;

		case AST_WHILE:
		{

			Ast pred = tree->while_stat.pred;
			Ast body = tree->while_stat.body;


			PRINT(pr, "while ");
			print_ast(pr, pred);
			PRINT(pr, " ? ");
			print_ast(pr, body);
		}
		break;

		case AST_FOR:
		{
			PRINT(pr, "for ");
			print_ast(pr, tree->for_stat.init);
			PRINT(pr, "; ");
			print_ast(pr, tree->for_stat.pred);
			PRINT(pr, "; ");
			print_ast(pr, tree->for_stat.step);
			PRINT(pr, " ? ");
			print_ast(pr, tree->for_stat.body);
		}
		break;
		case AST_FOR_RANGE:
		{
			PRINT(pr, "for ");
			print_ast(pr, tree->for_range_stat.decl);
			PRINT(pr, " ? ");
			print_ast(pr, tree->for_range_stat.body);
		}
		break;

		case AST_IF:
		{

			Ast pred = tree->if_stat.pred;
			Ast true_clause = tree->if_stat.true_clause;
			Ast else_clause = tree->if_stat.else_clause;


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

			Ast *stats = tree->block.items;
			u32 nstats = tree->block.count;


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

			Ast x = tree->binary.x;
			Ast y = tree->binary.y;


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
			Ast expr = tree->call.expr;
			Ast *args = tree->call.args.items;
			u32 nargs = tree->call.args.count;
			print_ast(pr, expr);
			PRINT(pr, "(");
			print_ast_list(pr, args, nargs);
			PRINT(pr, ")");
		}
		break;
		case AST_NAMED_ARGUMENT:
		{
			print_ast(pr, tree->named_argument.name);
			PRINT(pr, " = ");
			print_ast(pr, tree->named_argument.expr);
		}
		break;

		case AST_DECL_STAT:
		{

			Ast name = tree->decl.name;
			Ast expr = tree->decl.expr;


			print_ast(pr, name);
			PRINT(pr, tree->decl.constant ? " ::= " : " := ");
			print_ast(pr, expr);
		}
		break;

		case AST_STATIC_DECL:
		{
			if (tree->static_decl.exported) {
				PRINT(pr, "export ");
			}
			print_ast(pr, tree->static_decl.name);
			PRINT(pr, " :: ");
			print_ast(pr, tree->static_decl.expr);
		}
		break;

		case AST_ASSIGN:
		{

			Ast x = tree->binary.x;
			Ast y = tree->binary.y;


			print_ast(pr, x);
			PRINT(pr, " = ");
			print_ast(pr, y);
		}
		break;
		case AST_RANGE:
		{

			Ast x = tree->binary.x;
			Ast y = tree->binary.y;


			PRINT(pr, "(");
			if (x) {
				print_ast(pr, x);
			}
			PRINT(pr, "...");
			if (y) {
				print_ast(pr, y);
			}
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

			Ast *args = tree->tuple.items;
			u32 nargs = tree->tuple.count;


			PRINT(pr, "(");
			print_ast_list(pr, args, nargs);
			PRINT(pr, ")");
		}
		break;
		case AST_TABLE:
		{
			PRINT(pr, "{");
			print_ast_list(pr, tree->table.items, tree->table.count);
			PRINT(pr, "}");
		}
		break;
		case AST_ARRAY:
		{
			PRINT(pr, "[");
			print_ast_list(pr, tree->array.items, tree->array.count);
			PRINT(pr, "]");
		}
		break;
		case AST_TABLE_ENTRY:
		{
			print_ast(pr, tree->table_entry.key);
			PRINT(pr, ": ");
			print_ast(pr, tree->table_entry.value);
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
		case AST_MEASURE:
		{
			PRINT(pr, "*");
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























