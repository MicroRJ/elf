//
// See Copyright Notice In elf.h
//

// README: IR node constructors are named ir_new_<kind>.

static Ir ir_alloc(IrFunctionEmitter *ctx, SourceSite site, IrKind kind)
{
	IrNode *ir = elf_arena_push_zero(ctx->module->arena, sizeof(*ir));
	ir->kind = kind;
	ir->site = site;
	return ir;
}

static Ir ir_new_nullary(IrFunctionEmitter *ctx, SourceSite site, IrKind kind)
{
	Ir ir = ir_alloc(ctx, site, kind);
	return ir;
}

static Ir ir_new_unary(IrFunctionEmitter *ctx, SourceSite site, IrKind kind, Ir x)
{
	Ir ir = ir_alloc(ctx, site, kind);
	ir->ir_unary = x;
	return ir;
}

static Ir ir_new_binary(IrFunctionEmitter *ctx, SourceSite site, IrKind kind, Ir x, Ir y)
{
	Ir ir = ir_alloc(ctx, site, kind);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static Ir ir_new_nil(IrFunctionEmitter *ctx, SourceSite site)
{
	return ir_new_nullary(ctx, site, IR_NIL);
}

static Ir ir_new_int(IrFunctionEmitter *ctx, SourceSite site, i64 i)
{
	Ir ir = ir_new_nullary(ctx, site, IR_INTEGER);
	ir->ir_int = i;
	return ir;
}

static Ir ir_new_num(IrFunctionEmitter *ctx, SourceSite site, f64 n)
{
	Ir ir = ir_new_nullary(ctx, site, IR_NUMBER);
	ir->ir_num = n;
	return ir;
}

static Ir ir_new_atom(IrFunctionEmitter *ctx, SourceSite site, Atom *atom)
{
	Ir ir = ir_new_nullary(ctx, site, IR_ATOM);
	ir->atom = atom;
	return ir;
}

static Ir ir_new_load_global(IrFunctionEmitter *ctx, SourceSite site, u32 slot)
{
	Ir ir = ir_alloc(ctx, site, IR_LOAD_GLOBAL);
	ir->ir_global = slot;
	return ir;
}

static Ir ir_new_function(IrFunctionEmitter *ctx, SourceSite site, u32 function_index)
{
	ASSERT(function_index < ctx->module->compiler->function_count);
	ASSERT(function_index < ctx->module->function_count);
	ASSERT(ctx->module->functions[function_index].is_static);
	Ir ir = ir_alloc(ctx, site, IR_FUNCTION);
	ir->ir_function.index = function_index;
	return ir;
}

static Ir ir_new_closure(IrFunctionEmitter *ctx, SourceSite site, u32 function_index, IrArray captures)
{
	ASSERT(function_index < ctx->module->function_count);
	ASSERT(captures.count == ctx->module->functions[function_index].capture_count);
	Ir ir = ir_alloc(ctx, site, IR_CLOSURE);
	ir->ir_closure.index = function_index;
	ir->ir_closure.captures = captures;
	return ir;
}

static Ir ir_new_recurse(IrFunctionEmitter *ctx, SourceSite site)
{
	return ir_new_nullary(ctx, site, IR_RECURSE);
}

static Ir ir_new_capture(IrFunctionEmitter *ctx, SourceSite site, u32 capture_index)
{
	Ir ir = ir_alloc(ctx, site, IR_CAPTURE);
	ir->ir_capture = capture_index;
	return ir;
}

static Ir ir_new_if(IrFunctionEmitter *ctx, SourceSite site, Ir pred, Ir true_clause, Ir else_clause)
{
	Ir ir = ir_alloc(ctx, site, IR_IF);
	ir->ir_if.pred = pred;
	ir->ir_if.true_clause = true_clause;
	ir->ir_if.else_clause = else_clause;
	return ir;
}

static Ir ir_new_label(IrFunctionEmitter *ctx, SourceSite site)
{
	Ir ir = ir_alloc(ctx, site, IR_LABEL);
	return ir;
}

static Ir ir_new_jump(IrFunctionEmitter *ctx, SourceSite site, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);

	Ir ir = ir_alloc(ctx, site, IR_JUMP);
	ir->ir_jump.label = label;
	return ir;
}

static Ir ir_new_jump_if_false(IrFunctionEmitter *ctx, SourceSite site, Ir pred, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);

	Ir ir = ir_alloc(ctx, site, IR_JUMP_IF_FALSE);
	ir->ir_jump_if_false.pred = pred;
	ir->ir_jump_if_false.label = label;
	return ir;
}

static Ir ir_new_block(IrFunctionEmitter *ctx, SourceSite site, IrArray stats)
{
	Ir ir = ir_alloc(ctx, site, IR_BLOCK);
	ir->ir_block.stats = stats;
	return ir;
}

static Ir ir_new_expr_block(IrFunctionEmitter *ctx, SourceSite site, IrArray stats, Ir value)
{
	Ir ir = ir_alloc(ctx, site, IR_EXPR_BLOCK);
	ir->ir_expr_block.stats = stats;
	ir->ir_expr_block.value = value;
	return ir;
}

static Ir ir_new_local(IrFunctionEmitter *ctx, SourceSite site, Ir expr)
{
	Ir ir = ir_alloc(ctx, site, IR_LOCAL);
	ir->ir_local.expr = expr;
	ir->ir_local.slot = NO_MEMORY;
	return ir;
}

static Ir ir_new_multi_local(IrFunctionEmitter *ctx, SourceSite site, Ir expr, IrArray locals)
{
	Ir ir = ir_alloc(ctx, site, IR_MULTI_LOCAL);
	ir->ir_multi_local.expr = expr;
	ir->ir_multi_local.locals = locals;
	return ir;
}

static Ir ir_new_load_local(IrFunctionEmitter *ctx, SourceSite site, Ir local)
{
	ASSERT(local);
	ASSERT(local->kind == IR_LOCAL);

	Ir ir = ir_alloc(ctx, site, IR_LOAD_LOCAL);
	ir->ir_load_local.local = local;
	return ir;
}

static Ir ir_new_store(IrFunctionEmitter *ctx, SourceSite site, Ir x, Ir y)
{
	Ir ir = ir_alloc(ctx, site, IR_STORE);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static Ir ir_new_array_add(IrFunctionEmitter *ctx, SourceSite site, Ir array, Ir value)
{
	return ir_new_binary(ctx, site, IR_ARRAY_ADD, array, value);
}

static Ir ir_new_call(IrFunctionEmitter *ctx, SourceSite site, IrCallType type, Ir expr, IrArray args)
{
	ASSERT(type == IR_CALL_DYNAMIC || type == IR_CALL_DIRECT);
	ASSERT(type != IR_CALL_DIRECT || (expr && expr->kind == IR_FUNCTION));
	Ir ir = ir_alloc(ctx, site, IR_CALL);
	ir->ir_call.type = type;
	ir->ir_call.expr = expr;
	ir->ir_call.args = args;
	return ir;
}

static Ir ir_new_table(IrFunctionEmitter *ctx, SourceSite site)
{
	return ir_new_nullary(ctx, site, IR_TABLE);
}

static Ir ir_new_array(IrFunctionEmitter *ctx, SourceSite site)
{
	return ir_new_nullary(ctx, site, IR_ARRAY);
}

static Ir ir_new_index(IrFunctionEmitter *ctx, SourceSite site, Ir x, Ir y)
{
	return ir_new_binary(ctx, site, IR_INDEX, x, y);
}

static Ir ir_new_measure(IrFunctionEmitter *ctx, SourceSite site, Ir expr)
{
	return ir_new_unary(ctx, site, IR_MEASURE, expr);
}

static Ir ir_new_get_mem(IrFunctionEmitter *ctx, SourceSite site, Ir expr)
{
	return ir_new_unary(ctx, site, IR_GET_MEM, expr);
}

static Ir ir_new_return(IrFunctionEmitter *ctx, SourceSite site, Ir expr)
{
	Ir ir = ir_alloc(ctx, site, IR_RETURN);
	ir->ir_return.expr = expr;
	return ir;
}

typedef struct
{
	u32 indent;
}
IrPrinter;

#define IR_PRINT(pr, text, ...) do { printf(text, __VA_ARGS__); } while (0)

static void ir_print_new_line(IrPrinter *pr)
{
	IR_PRINT(pr, "\n");
}

static void ir_print_indent(IrPrinter *pr, i32 indent)
{
	pr->indent += indent;
}

static void print_ir(IrPrinter *pr, Ir ir)
{
	if (!ir) {
		IR_PRINT(pr, "(null)");
		return;
	}
	switch (ir->kind)
	{
		case IR_NONE:
		{
			IR_PRINT(pr, "(none)");
		}
		break;
		case IR_ERROR:
		{
			IR_PRINT(pr, "(error)");
		}
		break;
		case IR_ATOM:
		{
			const char *data = atom_data(ir->atom);
			IR_PRINT(pr, "\"%s\"", data);
		}
		break;
		case IR_INTEGER:
		{
			i64 integer = ir->ir_int;
			IR_PRINT(pr, "%lli", integer);
		}
		break;
		case IR_NUMBER:
		{
			f64 number = ir->ir_num;
			IR_PRINT(pr, "%f", number);
		}
		break;

		case IR_LABEL:
		{
			IR_PRINT(pr, "label %p", ir);
		}
		break;

		case IR_JUMP:
		{
			IR_PRINT(pr, "jump %p", ir->ir_jump.label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			IR_PRINT(pr, "jump_if_false ");
			print_ir(pr, ir->ir_jump_if_false.pred);
			IR_PRINT(pr, ", %p", ir->ir_jump_if_false.label);
		}
		break;

		case IR_IF:
		{
			Ir pred = ir->ir_if.pred;
			Ir true_clause = ir->ir_if.true_clause;
			Ir else_clause = ir->ir_if.else_clause;
			IR_PRINT(pr, "if ");
			print_ir(pr, pred);
			IR_PRINT(pr, "?");
			print_ir(pr, true_clause);
			if (else_clause) {
				IR_PRINT(pr, "else ");
				print_ir(pr, else_clause);
			}
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = ir->ir_block.stats;
			IR_PRINT(pr, "{");
			ir_print_new_line(pr );
			ir_print_indent(pr, +1);
			for (u32 i = 0; i < stats.count; ++ i)
			{
				print_ir(pr, stats.items[i]);
				ir_print_new_line(pr);
			}
			ir_print_indent(pr, -1);
			IR_PRINT(pr, "}");
		}
		break;

		case IR_EXPR_BLOCK:
		{
			IrArray stats = ir->ir_expr_block.stats;
			IR_PRINT(pr, "expr ");
			IR_PRINT(pr, "{");
			ir_print_new_line(pr);
			ir_print_indent(pr, +1);
			for (u32 i = 0; i < stats.count; ++ i)
			{
				print_ir(pr, stats.items[i]);
				ir_print_new_line(pr);
			}
			ir_print_indent(pr, -1);
			IR_PRINT(pr, "} => ");
			print_ir(pr, ir->ir_expr_block.value);
		}
		break;

		case IR_FIELD:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			print_ir(pr, x);
			IR_PRINT(pr, ".");
			print_ir(pr, y);
		}
		break;

		case IR_CALL:
		{
			Ir expr = ir->ir_call.expr;
			IrArray args = ir->ir_call.args;
			print_ir(pr, expr);
			IR_PRINT(pr, "(");
			for (u32 i = 0; i < args.count; ++ i)
			{
				if (i != 0) IR_PRINT(pr, ", ");
				print_ir(pr, args.items[i]);
			}
			IR_PRINT(pr, ")");
		}
		break;

		case IR_FUNCTION:
		{
			IR_PRINT(pr, "function[%u]", ir->ir_function.index);
		}
		break;

		case IR_CLOSURE:
		{
			IR_PRINT(pr, "closure[%u]", ir->ir_closure.index);
		}
		break;

		case IR_TABLE:
		{
			IR_PRINT(pr, "table");
		}
		break;

		case IR_ARRAY:
		{
			IR_PRINT(pr, "array");
		}
		break;

		case IR_ARRAY_ADD:
		{
			print_ir(pr, ir->ir_binary.x);
			IR_PRINT(pr, "[] = ");
			print_ir(pr, ir->ir_binary.y);
		}
		break;

		case IR_STORE:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			print_ir(pr, x);
			IR_PRINT(pr, " = ");
			print_ir(pr, y);
		}
		break;

		case IR_LESS_THAN:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			IR_PRINT(pr, "(");
			print_ir(pr, x);
			IR_PRINT(pr, " < ");
			print_ir(pr, y);
			IR_PRINT(pr, ")");
		}
		break;

		case IR_ADD:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			IR_PRINT(pr, "(");
			print_ir(pr, x);
			IR_PRINT(pr, " + ");
			print_ir(pr, y);
			IR_PRINT(pr, ")");
		}
		break;

		case IR_MUL:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			IR_PRINT(pr, "(");
			print_ir(pr, x);
			IR_PRINT(pr, " * ");
			print_ir(pr, y);
			IR_PRINT(pr, ")");
		}
		break;

		default:
		{
			IR_PRINT(pr, "(%s)", ir_kind_name(ir->kind));
		}
		break;
	}
}
