//
// See Copyright Notice In elf.h
//

static const char *ir_kind_name(IRKind kind)
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

static IR create_ir(LowerContext *ctx, SourceSite site, IRKind kind)
{
	IR_Node *ir = arena_push_zero(ctx->arena, sizeof(*ir));
	ir->kind = kind;
	ir->site = site;
	return ir;
}

static IR create_nullary_ir(LowerContext *ctx, SourceSite site, IRKind kind)
{
	IR ir = create_ir(ctx, site, kind);
	return ir;
}

static IR create_unary_ir(LowerContext *ctx, SourceSite site, IRKind kind, IR x)
{
	IR ir = create_ir(ctx, site, kind);
	ir->ir_unary = x;
	return ir;
}

static IR create_binary_ir(LowerContext *ctx, SourceSite site, IRKind kind, IR x, IR y)
{
	IR ir = create_ir(ctx, site, kind);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static IR create_nil_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_NIL);
}

static IR create_int_ir(LowerContext *ctx, SourceSite site, i64 i)
{
	IR ir = create_nullary_ir(ctx, site, IR_INTEGER);
	ir->ir_int = i;
	return ir;
}

static IR create_num_ir(LowerContext *ctx, SourceSite site, f64 n)
{
	IR ir = create_nullary_ir(ctx, site, IR_NUMBER);
	ir->ir_num = n;
	return ir;
}

static IR create_atom_ir(LowerContext *ctx, SourceSite site, elf_Atom *atom)
{
	IR ir = create_nullary_ir(ctx, site, IR_ATOM);
	ir->atom = atom;
	return ir;
}

static IR create_load_global_ir(LowerContext *ctx, SourceSite site, u32 slot)
{
	IR ir = create_ir(ctx, site, IR_LOAD_GLOBAL);
	ir->ir_global = slot;
	return ir;
}

static IR create_function_ir(LowerContext *ctx, SourceSite site, u32 function_index)
{
	IR ir = create_ir(ctx, site, IR_FUNCTION);
	ir->ir_function.index = function_index;
	return ir;
}

static IR create_recurse_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_RECURSE);
}

static IR create_capture_ir(LowerContext *ctx, SourceSite site, u32 capture_index)
{
	IR ir = create_ir(ctx, site, IR_CAPTURE);
	ir->ir_capture = capture_index;
	return ir;
}

static IR create_if_ir(LowerContext *ctx, SourceSite site, IR pred, IR true_clause, IR else_clause)
{
	IR ir = create_ir(ctx, site, IR_IF);
	ir->ir_if.pred = pred;
	ir->ir_if.true_clause = true_clause;
	ir->ir_if.else_clause = else_clause;
	return ir;
}

static IR create_label_ir(LowerContext *ctx, SourceSite site, u32 label)
{
	IR ir = create_ir(ctx, site, IR_LABEL);
	ir->ir_label = label;
	return ir;
}

static IR create_jump_ir(LowerContext *ctx, SourceSite site, u32 label)
{
	IR ir = create_ir(ctx, site, IR_JUMP);
	ir->ir_label = label;
	return ir;
}

static IR create_jump_if_false_ir(LowerContext *ctx, SourceSite site, IR pred, u32 label)
{
	IR ir = create_ir(ctx, site, IR_JUMP_IF_FALSE);
	ir->ir_jump_if_false.pred = pred;
	ir->ir_jump_if_false.label = label;
	return ir;
}

static IR create_block_ir(LowerContext *ctx, SourceSite site, IR_Array stats)
{
	IR ir = create_ir(ctx, site, IR_BLOCK);
	ir->ir_block.stats = stats;
	return ir;
}

static IR create_expr_block_ir(LowerContext *ctx, SourceSite site, IR_Array stats, IR value)
{
	IR ir = create_ir(ctx, site, IR_EXPR_BLOCK);
	ir->ir_expr_block.stats = stats;
	ir->ir_expr_block.value = value;
	return ir;
}

static IR create_local_ir(LowerContext *ctx, SourceSite site, IR expr)
{
	IR ir = create_ir(ctx, site, IR_LOCAL);
	ir->ir_local.expr = expr;
	ir->ir_local.slot = NO_MEMORY;
	return ir;
}

static IR create_load_local_ir(LowerContext *ctx, SourceSite site, IR local)
{
	ASSERT(local);
	ASSERT(local->kind == IR_LOCAL);

	IR ir = create_ir(ctx, site, IR_LOAD_LOCAL);
	ir->ir_load_local.local = local;
	return ir;
}

static IR create_store_ir(LowerContext *ctx, SourceSite site, IR x, IR y)
{
	IR ir = create_ir(ctx, site, IR_STORE);
	ir->ir_binary.x = x;
	ir->ir_binary.y = y;
	return ir;
}

static IR create_array_add_ir(LowerContext *ctx, SourceSite site, IR table, IR value)
{
	return create_binary_ir(ctx, site, IR_ARRAY_ADD, table, value);
}

static IR create_call_ir(LowerContext *ctx, SourceSite site, IR expr, IR_Array args)
{
	IR ir = create_ir(ctx, site, IR_CALL);
	ir->ir_call.expr = expr;
	ir->ir_call.args = args;
	return ir;
}

static IR create_table_ir(LowerContext *ctx, SourceSite site)
{
	return create_nullary_ir(ctx, site, IR_TABLE);
}

static IR create_index_ir(LowerContext *ctx, SourceSite site, IR x, IR y)
{
	return create_binary_ir(ctx, site, IR_INDEX, x, y);
}

static IR create_length_intrinsic_ir(LowerContext *ctx, SourceSite site, IR expr)
{
	return create_unary_ir(ctx, site, IR_LENGTH_INTRINSIC, expr);
}

static IR create_get_mem_ir(LowerContext *ctx, SourceSite site, IR expr)
{
	return create_unary_ir(ctx, site, IR_GET_MEM, expr);
}

static IR create_return_ir(LowerContext *ctx, SourceSite site, IR expr)
{
	IR ir = create_ir(ctx, site, IR_RETURN);
	ir->ir_return.expr = expr;
	return ir;
}

static void print_ir(Printer *pr, IR ir)
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
			PRINT(pr, "label %u", ir->ir_label);
		}
		break;

		case IR_JUMP:
		{
			PRINT(pr, "jump %u", ir->ir_label);
		}
		break;

		case IR_JUMP_IF_FALSE:
		{
			PRINT(pr, "jump_if_false ");
			print_ir(pr, ir->ir_jump_if_false.pred);
			PRINT(pr, ", %u", ir->ir_jump_if_false.label);
		}
		break;

		case IR_IF:
		{
			IR pred = ir->ir_if.pred;
			IR true_clause = ir->ir_if.true_clause;
			IR else_clause = ir->ir_if.else_clause;
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
			IR_Array stats = ir->ir_block.stats;
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
			IR_Array stats = ir->ir_expr_block.stats;
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
			IR x = ir->ir_binary.x;
			IR y = ir->ir_binary.y;
			print_ir(pr, x);
			PRINT(pr, ".");
			print_ir(pr, y);
		}
		break;

		case IR_CALL:
		{
			IR expr = ir->ir_call.expr;
			IR_Array args = ir->ir_call.args;
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
			IR x = ir->ir_binary.x;
			IR y = ir->ir_binary.y;
			print_ir(pr, x);
			PRINT(pr, " = ");
			print_ir(pr, y);
		}
		break;

		case IR_LESS_THAN:
		{
			IR x = ir->ir_binary.x;
			IR y = ir->ir_binary.y;
			PRINT(pr, "(");
			print_ir(pr, x);
			PRINT(pr, " < ");
			print_ir(pr, y);
			PRINT(pr, ")");
		}
		break;

		case IR_ADD:
		{
			IR x = ir->ir_binary.x;
			IR y = ir->ir_binary.y;
			PRINT(pr, "(");
			print_ir(pr, x);
			PRINT(pr, " + ");
			print_ir(pr, y);
			PRINT(pr, ")");
		}
		break;

		case IR_MUL:
		{
			IR x = ir->ir_binary.x;
			IR y = ir->ir_binary.y;
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
