//
// See Copyright Notice In elf.h
//

static const char *ir_kind_name(IrKind kind)
{
	static const char *names[] =
	{
#define IR_XPAND(ENUM, NAME) #ENUM,
		IR_XDEF(IR_XPAND)
#undef IR_XPAND
	};

	if ((u32)kind >= IR_COUNT_) {
		return "IR_UNKNOWN";
	}
	return names[kind];
}

static Ir create_ir(LowerContext *ctx, SourceSite site, IrKind kind)
{
	IrNode *ir = arena_push_zero(ctx->arena, sizeof(*ir));
	ir->kind = kind;
	ir->site = site;
	return ir;
}

static Ir create_nullary_ir(LowerContext *ctx, SourceSite site, IrKind kind)
{
	Ir ir = create_ir(ctx, site, kind);
	return ir;
}

static Ir create_unary_ir(LowerContext *ctx, SourceSite site, IrKind kind, Ir x)
{
	Ir ir = create_ir(ctx, site, kind);
	ir->ir_unary = x;
	return ir;
}

static Ir create_binary_ir(LowerContext *ctx, SourceSite site, IrKind kind, Ir x, Ir y)
{
	Ir ir = create_ir(ctx, site, kind);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static Ir create_nil_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_NIL);
}

static Ir create_int_ir(LowerContext *ctx, SourceSite site, i64 i)
{
	Ir ir = create_nullary_ir(ctx, site, IR_INTEGER);
	ir->ir_int = i;
	return ir;
}

static Ir create_num_ir(LowerContext *ctx, SourceSite site, f64 n)
{
	Ir ir = create_nullary_ir(ctx, site, IR_NUMBER);
	ir->ir_num = n;
	return ir;
}

static Ir create_atom_ir(LowerContext *ctx, SourceSite site, elf_String *atom)
{
	Ir ir = create_nullary_ir(ctx, site, IR_ATOM);
	ir->atom = atom;
	return ir;
}

static Ir create_load_global_ir(LowerContext *ctx, SourceSite site, u32 slot)
{
	Ir ir = create_ir(ctx, site, IR_LOAD_GLOBAL);
	ir->ir_global = slot;
	return ir;
}

static Ir create_function_ir(LowerContext *ctx, SourceSite site, u32 function_index)
{
	Ir ir = create_ir(ctx, site, IR_FUNCTION);
	ir->ir_function.index = function_index;
	return ir;
}

static Ir create_recurse_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_RECURSE);
}

static Ir create_capture_ir(LowerContext *ctx, SourceSite site, u32 capture_index)
{
	Ir ir = create_ir(ctx, site, IR_CAPTURE);
	ir->ir_capture = capture_index;
	return ir;
}

static Ir create_if_ir(LowerContext *ctx, SourceSite site, Ir pred, Ir true_clause, Ir else_clause)
{
	Ir ir = create_ir(ctx, site, IR_IF);
	ir->ir_if.pred = pred;
	ir->ir_if.true_clause = true_clause;
	ir->ir_if.else_clause = else_clause;
	return ir;
}

static Ir create_label_ir(LowerContext *ctx, SourceSite site)
{
	Ir ir = create_ir(ctx, site, IR_LABEL);
	return ir;
}

static Ir create_jump_ir(LowerContext *ctx, SourceSite site, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);

	Ir ir = create_ir(ctx, site, IR_JUMP);
	ir->ir_jump.label = label;
	return ir;
}

static Ir create_jump_if_false_ir(LowerContext *ctx, SourceSite site, Ir pred, Ir label)
{
	ASSERT(label);
	ASSERT(label->kind == IR_LABEL);

	Ir ir = create_ir(ctx, site, IR_JUMP_IF_FALSE);
	ir->ir_jump_if_false.pred = pred;
	ir->ir_jump_if_false.label = label;
	return ir;
}

static Ir create_block_ir(LowerContext *ctx, SourceSite site, IrArray stats)
{
	Ir ir = create_ir(ctx, site, IR_BLOCK);
	ir->ir_block.stats = stats;
	return ir;
}

static Ir create_expr_block_ir(LowerContext *ctx, SourceSite site, IrArray stats, Ir value)
{
	Ir ir = create_ir(ctx, site, IR_EXPR_BLOCK);
	ir->ir_expr_block.stats = stats;
	ir->ir_expr_block.value = value;
	return ir;
}

static Ir create_local_ir(LowerContext *ctx, SourceSite site, Ir expr)
{
	Ir ir = create_ir(ctx, site, IR_LOCAL);
	ir->ir_local.expr = expr;
	ir->ir_local.slot = NO_MEMORY;
	return ir;
}

static Ir create_load_local_ir(LowerContext *ctx, SourceSite site, Ir local)
{
	ASSERT(local);
	ASSERT(local->kind == IR_LOCAL);

	Ir ir = create_ir(ctx, site, IR_LOAD_LOCAL);
	ir->ir_load_local.local = local;
	return ir;
}

static Ir create_store_ir(LowerContext *ctx, SourceSite site, Ir x, Ir y)
{
	Ir ir = create_ir(ctx, site, IR_STORE);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static Ir create_array_add_ir(LowerContext *ctx, SourceSite site, Ir table, Ir value)
{
	return create_binary_ir(ctx, site, IR_ARRAY_ADD, table, value);
}

static Ir create_call_ir(LowerContext *ctx, SourceSite site, Ir expr, IrArray args)
{
	Ir ir = create_ir(ctx, site, IR_CALL);
	ir->ir_call.expr = expr;
	ir->ir_call.args = args;
	return ir;
}

static Ir create_table_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_TABLE);
}

static Ir create_index_ir(LowerContext *ctx, SourceSite site, Ir x, Ir y)
{
	return create_binary_ir(ctx, site, IR_INDEX, x, y);
}

static Ir create_length_intrinsic_ir(LowerContext *ctx, SourceSite site, Ir expr)
{
	return create_unary_ir(ctx, site, IR_LENGTH_INTRINSIC, expr);
}

static Ir create_get_mem_ir(LowerContext *ctx, SourceSite site, Ir expr)
{
	return create_unary_ir(ctx, site, IR_GET_MEM, expr);
}

static Ir create_return_ir(LowerContext *ctx, SourceSite site, Ir expr)
{
	Ir ir = create_ir(ctx, site, IR_RETURN);
	ir->ir_return.expr = expr;
	return ir;
}

static void print_ir(Printer *pr, Ir ir)
{
	if (!ir) {
		PRINT(pr, "(null)");
		return;
	}
	switch (ir->kind)
	{
		case IR_NONE:
		{
			PRINT(pr, "(none)");
		}
		break;
		case IR_ERROR:
		{
			PRINT(pr, "(error)");
		}
		break;
		case IR_ATOM:
		{
			const char *data = elf_atom_data(ir->atom);
			PRINT(pr, "\"%s\"", data);
		}
		break;
		case IR_INTEGER:
		{
			i64 integer = ir->ir_int;
			PRINT(pr, "%lli", integer);
		}
		break;
		case IR_NUMBER:
		{
			f64 number = ir->ir_num;
			PRINT(pr, "%f", number);
		}
		break;

		case IR_LABEL:
		{
			PRINT(pr, "label %p", ir);
		}
		break;

		case IR_JUMP:
		{
			PRINT(pr, "jump %p", ir->ir_jump.label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			PRINT(pr, "jump_if_false ");
			print_ir(pr, ir->ir_jump_if_false.pred);
			PRINT(pr, ", %p", ir->ir_jump_if_false.label);
		}
		break;

		case IR_IF:
		{
			Ir pred = ir->ir_if.pred;
			Ir true_clause = ir->ir_if.true_clause;
			Ir else_clause = ir->ir_if.else_clause;
			PRINT(pr, "if ");
			print_ir(pr, pred);
			PRINT(pr, "?");
			print_ir(pr, true_clause);
			if (else_clause) {
				PRINT(pr, "else ");
				print_ir(pr, else_clause);
			}
		}
		break;

		case IR_BLOCK:
		{
			IrArray stats = ir->ir_block.stats;
			PRINT(pr, "{");
			elf_print_new_line(pr );
			elf_print_indent(pr, +1);
			for (u32 i = 0; i < stats.count; ++ i)
			{
				print_ir(pr, stats.items[i]);
				elf_print_new_line(pr);
			}
			elf_print_indent(pr, -1);
			PRINT(pr, "}");
		}
		break;

		case IR_EXPR_BLOCK:
		{
			IrArray stats = ir->ir_expr_block.stats;
			PRINT(pr, "expr ");
			PRINT(pr, "{");
			elf_print_new_line(pr);
			elf_print_indent(pr, +1);
			for (u32 i = 0; i < stats.count; ++ i)
			{
				print_ir(pr, stats.items[i]);
				elf_print_new_line(pr);
			}
			elf_print_indent(pr, -1);
			PRINT(pr, "} => ");
			print_ir(pr, ir->ir_expr_block.value);
		}
		break;

		case IR_FIELD:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			print_ir(pr, x);
			PRINT(pr, ".");
			print_ir(pr, y);
		}
		break;

		case IR_CALL:
		{
			Ir expr = ir->ir_call.expr;
			IrArray args = ir->ir_call.args;
			print_ir(pr, expr);
			PRINT(pr, "(");
			for (u32 i = 0; i < args.count; ++ i)
			{
				if (i != 0) PRINT(pr, ", ");
				print_ir(pr, args.items[i]);
			}
			PRINT(pr, ")");
		}
		break;

		case IR_FUNCTION:
		{
			PRINT(pr, "function[%u]", ir->ir_function.index);
		}
		break;

		case IR_TABLE:
		{
			PRINT(pr, "table");
		}
		break;

		case IR_ARRAY_ADD:
		{
			print_ir(pr, ir->ir_binary.x);
			PRINT(pr, "[] = ");
			print_ir(pr, ir->ir_binary.y);
		}
		break;

		case IR_STORE:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			print_ir(pr, x);
			PRINT(pr, " = ");
			print_ir(pr, y);
		}
		break;

		case IR_LESS_THAN:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			PRINT(pr, "(");
			print_ir(pr, x);
			PRINT(pr, " < ");
			print_ir(pr, y);
			PRINT(pr, ")");
		}
		break;

		case IR_ADD:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			PRINT(pr, "(");
			print_ir(pr, x);
			PRINT(pr, " + ");
			print_ir(pr, y);
			PRINT(pr, ")");
		}
		break;

		case IR_MUL:
		{
			Ir x = ir->ir_binary.x;
			Ir y = ir->ir_binary.y;
			PRINT(pr, "(");
			print_ir(pr, x);
			PRINT(pr, " * ");
			print_ir(pr, y);
			PRINT(pr, ")");
		}
		break;

		default:
		{
			PRINT(pr, "(%s)", ir_kind_name(ir->kind));
		}
		break;
	}
}
