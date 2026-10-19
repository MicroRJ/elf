//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "compiler.h"
#include "entity.h"
#include "ir.h"
#include "ast.h"
#include "ir_emitter.h"
#include "token.h"

typedef struct IrEmitter IrEmitter;
typedef struct IrDefer IrDefer;

struct IrDefer
{
	IrDefer *previous;
	Ast      stat;
};

typedef struct IrLoopFrame IrLoopFrame;
struct IrLoopFrame
{
	IrLoopFrame *parent;
	Ir continue_label;
	Ir break_label;
	IrDefer *defer_start;
};

typedef struct IrEmitterFrame IrEmitterFrame;
typedef struct IrCapture IrCapture;

struct IrCapture
{
	IrCapture *next;
	Entity    *entity;
	Ir         target;
};

// represents a lexical function as we descend
struct IrEmitterFrame
{
	IrEmitterFrame  *parent;
	Ast              tree;
	IrDefer         *defer;
	IrDefer         *defer_scope_start;
	IrLoopFrame     *loop;
	IrCapture       *first_capture;
	IrCapture       *last_capture;
	u32              capture_count;
};

struct IrEmitter
{
	Compiler        *compiler;
	IrModule        *module;

	u32              ir_stack_size;
	u32              ir_stack_index;
	Ir              *ir_stack;
	IrEmitterFrame *function;
};


#include "ir.c"

typedef struct
{
	IrEmitter *emitter;
	u32        start;
	u32        count;
}
IrQueue;

static void ir_emit_stat(IrQueue *block, Ast stat);
static Ir ir_emit_expr(IrEmitter *emitter, Ast expr);
static Ir ir_emit_stat_as_block(IrEmitter *emitter, Ast stat);

static void ir_report_error(IrEmitter *emitter, ErrorCode error, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	compiler_reportv(emitter->compiler, error, ERROR_ERROR, ERROR_PHASE_LOWERING, site, format, args);
	va_end(args);
}

static IrQueue ir_begin_queue(IrEmitter *emitter)
{
	IrQueue block = {};
	block.emitter = emitter;
	block.start = emitter->ir_stack_index;
	block.count = 0;
	return block;
}

static void ir_push(IrQueue *stack, Ir ir)
{
	IrEmitter *emitter = stack->emitter;
	ASSERT(emitter);
	ASSERT(emitter->ir_stack_index == stack->start + stack->count);
	ASSERT(emitter->ir_stack_index < emitter->ir_stack_size);
	emitter->ir_stack[emitter->ir_stack_index++] = ir;
	stack->count += 1;
}

static IrArray ir_end_queue(IrQueue *frame)
{
	IrEmitter *emitter = frame->emitter;
	ASSERT(emitter);
	ASSERT(emitter->ir_stack_index == frame->start + frame->count);

	emitter->ir_stack_index -= frame->count;

	IrArray items = {};
	items.count = frame->count;
	items.items = arena_push_copy(emitter->module->arena, sizeof(*items.items) * items.count, _Alignof(Ir), emitter->ir_stack + emitter->ir_stack_index);

	ASSERT(emitter->ir_stack_index == frame->start);
	frame->count = 0;
	return items;
}

static IrDefer *ir_begin_defer_scope(IrEmitter *emitter)
{
	ASSERT(emitter->function);
	IrDefer *previous_defer_start = emitter->function->defer_scope_start;
	emitter->function->defer_scope_start = emitter->function->defer;
	return previous_defer_start;
}

static void ir_close_defer_scope(IrEmitter *emitter, IrDefer *previous_defer_start)
{
	ASSERT(emitter->function);
	emitter->function->defer = emitter->function->defer_scope_start;
	emitter->function->defer_scope_start = previous_defer_start;
}

static void push_defer_stat(IrEmitter *emitter, Ast stat)
{
	ASSERT(emitter->function);
	IrDefer *defer = arena_push_zero(emitter->module->arena, sizeof(*defer), _Alignof(IrDefer));
	defer->previous = emitter->function->defer;
	defer->stat = stat;
	emitter->function->defer = defer;
}

static void emit_defer_range(IrEmitter *emitter, IrQueue *items, IrDefer *start)
{
	ASSERT(emitter->function);
	for (IrDefer *defer = emitter->function->defer; defer != start; defer = defer->previous)
	{
		ASSERT(defer);
		ir_emit_stat(items, defer->stat);
	}
}

static u32 capture_entity_in_function(IrEmitter *emitter, IrEmitterFrame *function_ctx, Entity *entity, SourceSite site)
{
	u32 capture_index = 0;
	for (IrCapture *capture = function_ctx->first_capture; capture; capture = capture->next)
	{
		if (capture->entity == entity) {
			return capture_index;
		}
		capture_index += 1;
	}

	ASSERT(function_ctx->capture_count < 255);
	ASSERT(capture_index == function_ctx->capture_count);

	Ir target = 0;
	IrEmitterFrame *parent = function_ctx->parent;

	if (parent && entity->parent != parent->tree)
	{
		u32 parent_capture = capture_entity_in_function(emitter, parent, entity, site);
		target = ir_new_capture(emitter, site, parent_capture);
	}
	else
	{
		target = ir_new_load(emitter, site, entity->ir);
	}

	IrCapture *capture = arena_push_zero(emitter->module->arena, sizeof(*capture), _Alignof(IrCapture));
	capture->entity = entity;
	capture->target = target;
	if (function_ctx->last_capture) {
		function_ctx->last_capture->next = capture;
	}
	else {
		function_ctx->first_capture = capture;
	}
	function_ctx->last_capture = capture;
	function_ctx->capture_count += 1;
	entity->tags |= ENTITY_TAG_REFERENCED;
	entity->users ++;
	return capture_index;
}

static Ir load_entity_ir(IrEmitter *emitter, SourceSite site, Entity *entity)
{
	ASSERT(entity);
	ASSERT(emitter->function);
	entity->tags |= ENTITY_TAG_REFERENCED;
	entity->users ++;
	if (entity->kind == ENTITY_GLOBAL) {
		return ir_new_global(emitter, site, entity->name);
	}
	ASSERT(entity->ir);
	if (entity->parent != emitter->function->tree)
	{
		u32 capture_index = capture_entity_in_function(emitter, emitter->function, entity, site);
		return ir_new_capture(emitter, site, capture_index);
	}

	return ir_new_load(emitter, site, entity->ir);
}

static b32 mark_entity_assigned(IrEmitter *emitter, Ast destination)
{
	if (!destination || destination->kind != AST_IDENT) {
		return true;
	}
	Entity *entity = destination->entity;
	ASSERT(entity);
	if (entity->tags & ENTITY_TAG_CONSTANT)
	{
		ir_report_error(emitter, ERROR_LOWERING, destination->site
		, "'%s' is constant and cannot be assigned", atom_data(destination->atom));
		return false;
	}
	entity->tags |= ENTITY_TAG_ASSIGNED;
	return true;
}

static void push_loop(IrEmitter *emitter, IrLoopFrame *loop)
{
	ASSERT(emitter->function);
	loop->parent = emitter->function->loop;
	emitter->function->loop = loop;
}

static void pop_loop(IrEmitter *emitter, IrLoopFrame *loop)
{
	ASSERT(emitter->function);
	ASSERT(emitter->function->loop == loop);
	emitter->function->loop = loop->parent;
}

static IrLoopFrame current_loop(IrEmitter *emitter, SourceSite site)
{
	ASSERT(emitter->function);
	if (!emitter->function->loop)
	{
		ir_report_error(emitter, ERROR_LOWERING, site, "break and continue must be inside a loop");
		IrLoopFrame loop = {};
		return loop;
	}
	return *emitter->function->loop;
}

static TypeRule type_rule_from_atom(Atom *atom)
{
	switch (atom->id)
	{
#define TYPE_NAME_RULE(NAME, TEXT, RULE) case ATOM_TYPE_NAME_##NAME: return RULE;
		TYPE_NAME_DEFINITIONS(TYPE_NAME_RULE)
#undef TYPE_NAME_RULE
		default: return TRULE_NONE;
	}
}

static IrFunction *ir_reserve_function(IrEmitter *emitter, SourceSite site, b32 variadic, u32 param_count);
static void ir_lower_function_body(IrEmitter *emitter, IrFunction *definition, Ast tree, Ast_Array ast_params, Ast body);
static IrFunction *ir_lower_function(IrEmitter *emitter, Ast tree, SourceSite site, b32 variadic, Ast_Array ast_params, Ast body);

static IrKind ir_kind_from_ast_kind(Ast_Kind kind)
{
	switch (kind)
	{
		case AST_AND:              return IR_AND;
		case AST_OR:               return IR_OR;
		case AST_NIL_OR:           return IR_NIL_OR;
		case AST_ADD:              return IR_ADD;
		case AST_SUB:              return IR_SUB;
		case AST_MUL:              return IR_MUL;
		case AST_DIV:              return IR_DIV;
		case AST_POW:              return IR_POW;
		case AST_MOD:              return IR_MOD;
		case AST_EQ:               return IR_EQ;
		case AST_NOT_EQ:           return IR_NOT_EQ;
		case AST_LESS_THAN:        return IR_LESS_THAN;
		case AST_LESS_THAN_EQ:     return IR_LESS_THAN_EQ;
		case AST_SHIFT_LEFT:       return IR_SHIFT_LEFT;
		case AST_SHIFT_RIGHT:      return IR_SHIFT_RIGHT;
		case AST_BITWISE_AND:      return IR_BITWISE_AND;
		case AST_BITWISE_OR:       return IR_BITWISE_OR;
		case AST_BITWISE_XOR:      return IR_BITWISE_XOR;
		case AST_BITWISE_NOT:      return IR_BITWISE_NOT;
		default: ;
	}
	return IR_NONE;
}

static IrKind ir_kind_from_compound_assign_ast_kind(Ast_Kind kind)
{
	switch (kind)
	{
		case AST_ADD_ASSIGN: return IR_ADD;
		case AST_SUB_ASSIGN: return IR_SUB;
		case AST_MUL_ASSIGN: return IR_MUL;
		case AST_DIV_ASSIGN: return IR_DIV;
		case AST_MOD_ASSIGN: return IR_MOD;
		case AST_XOR_ASSIGN: return IR_BITWISE_XOR;
		case AST_SHL_ASSIGN: return IR_SHIFT_LEFT;
		case AST_SHR_ASSIGN: return IR_SHIFT_RIGHT;
		default:             return IR_NONE;
	}
}

static b32 ast_is_call(Ast expr)
{
	return expr && expr->kind == AST_CALL;
}

static Ir ir_emit_expr(IrEmitter *emitter, Ast expr)
{
	if (!expr || ast_is_error(expr)) {
		return ERROR_IR;
	}

	Ir ir = 0;

	Entity *entity = expr->entity;
	if (entity)
	{
		StaticValue value = entity->value;

		switch (value.type) {
			case STATIC_VALUE_FUNCTION:
			{
				if (!entity->value.definition)
				{
					Ast function = entity->value.function;
					ASSERT(function && function->kind == AST_FUNCTION);
					entity->value.definition = ir_reserve_function(emitter, function->site, function->function.variadic, function->function.params.count);
					ir_lower_function_body(emitter, entity->value.definition, function, function->function.params, function->function.body);
				}
				ir = ir_new_function(emitter, expr->site, entity->value.definition);
				return ir;
			}
			break;

			case STATIC_VALUE_INTEGER:
			{
				ir = ir_new_int(emitter, expr->site, value.integer);
				return ir;
			}
			break;

			default:;
		}
	}

	switch (expr->kind)
	{
		case AST_INTEGER_LITERAL:
		{
			ir = ir_new_int(emitter, expr->site, expr->integer_value);
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			ir = ir_new_num(emitter, expr->site, expr->number_value);
		}
		break;

		case AST_STRING_LITERAL:
		{
			ir = ir_new_atom(emitter, expr->site, expr->atom);
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			Ast *parts = expr->interpolated_string.items;
			u32 nparts = expr->interpolated_string.count;
			if (nparts < 3 || (nparts & 1) == 0)
			{
				ir_report_error(emitter, ERROR_INTERNAL, expr->site,
					"interpolated string must contain alternating text and expression parts");
				ir = ERROR_IR;
				break;
			}

			ir = 0;
			for (u32 i = 0; i < nparts; ++ i)
			{
				Ast part = parts[i];
				if (!part || ((i & 1) == 0 && part->kind != AST_STRING_LITERAL))
				{
					ir_report_error(emitter, ERROR_INTERNAL, expr->site, "invalid interpolated string part at index %u", i);
					ir = ERROR_IR;
					break;
				}

				Ir part_ir = ir_emit_expr(emitter, part);
				if (!part_ir || part_ir == ERROR_IR)
				{
					ir = ERROR_IR;
					break;
				}

				ir = ir ? ir_new_binary(emitter, part->site, IR_ADD, ir, part_ir) : part_ir;
			}
		}
		break;

		case AST_NIL_LITERAL:
		{
			ir = ir_new_nil(emitter, expr->site);
		}
		break;

		case AST_IDENT:
		{
			Entity *entity = expr->entity;
			ASSERT(entity);
			ir = load_entity_ir(emitter, expr->site, entity);
		}
		break;

		case AST_META_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = ir_emit_expr(emitter, x);
			Ir y_ir = ir_emit_expr(emitter, y);

			ir = ir_new_binary(emitter, expr->site, IR_META_FIELD, x_ir, y_ir);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = ir_emit_expr(emitter, x);
			Ir y_ir = ir_emit_expr(emitter, y);

			ir = ir_new_index(emitter, expr->site, x_ir, y_ir);
		}
		break;

		case AST_MEASURE:
		{
			Ir expr_ir = ir_emit_expr(emitter, expr->unary);
			ir = ir_new_measure(emitter, expr->site, expr_ir);
		}
		break;

		case AST_GET_MEM:
		{
			Ir expr_ir = ir_emit_expr(emitter, expr->unary);
			if (!expr_ir || expr_ir->kind != IR_LOAD)
			{
				ir_report_error(emitter, ERROR_LOWERING, expr->site
				, "#get_mem expects a local or parameter expression");
				ir = ERROR_IR;
			}
			else
			{
				ir = ir_new_get_mem(emitter, expr->site, expr_ir);
			}
		}
		break;

		case AST_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;
			Ir x_ir = ir_emit_expr(emitter, x);
			Ir y_ir = ir_emit_expr(emitter, y);
			ir = ir_new_binary(emitter, expr->site, IR_FIELD, x_ir, y_ir);
		}
		break;

		case AST_CALL:
		{
			Ir ir_expr = ir_emit_expr(emitter, expr->call.expr);
			b32 failed = !ir_expr || ir_expr == ERROR_IR;
			u32 alloc_arguments = expr->call.args.count;

			if (!failed && ir_expr->kind == IR_FUNCTION)
			{
				IrFunction *ir_func = ir_expr->ir_function;
				ASSERT(ir_func);
				ASSERT(ir_func->ast);
				ASSERT(ir_func->ast->kind == AST_FUNCTION);
				ASSERT(ir_func->param_count >= IMPLICIT_PARAM_COUNT);
				ASSERT(ir_func->ast->function.params.count == ir_func->param_count - IMPLICIT_PARAM_COUNT);
				alloc_arguments = MAX(ir_func->ast->function.params.count, alloc_arguments);
			}

			Ir *arguments = arena_push_zero(emitter->compiler->arena, sizeof(* arguments) * alloc_arguments, _Alignof(Ir));

			for (u32 i = 0; i < expr->call.args.count; ++i)
			{
				Ast argument = expr->call.args.items[i];
				ASSERT(argument);
				u32 argument_slot = i;
				Ast argument_expr = argument;
				if (ir_expr && ir_expr != ERROR_IR && ir_expr->kind == IR_FUNCTION)
				{
					argument_slot = argument->param_slot;
				}
				if (argument->kind == AST_NAMED_ARGUMENT)
				{
					ASSERT(ir_expr && ir_expr != ERROR_IR && ir_expr->kind == IR_FUNCTION);
					ASSERT(argument->named_argument.expr);
					argument_expr = argument->named_argument.expr;
				}
				ASSERT(argument_slot < alloc_arguments);
				ASSERT(!arguments[argument_slot]);
				arguments[argument_slot] = ir_emit_expr(emitter, argument_expr);
				failed |= !arguments[argument_slot] || arguments[argument_slot] == ERROR_IR;
			}

			if (!failed && ir_expr->kind == IR_FUNCTION)
			{
				Ast tree_function = ir_expr->ir_function->ast;
				for (u32 i = 0; i < tree_function->function.params.count; ++ i)
				{
					Ast tree_parameter = tree_function->function.params.items[i];
					ASSERT(tree_parameter);
					ASSERT(tree_parameter->kind == AST_FUNCTION_PARAM);
					ASSERT(tree_parameter->param.name);
					if (!arguments[i] && tree_parameter->param.expr)
					{
						arguments[i] = ir_emit_expr(emitter, tree_parameter->param.expr);
						failed |= !arguments[i] || arguments[i] == ERROR_IR;
					}
				}
			}

			for (u32 i = 0; i < alloc_arguments; ++i)
			{
				if (!arguments[i])
				{
					arguments[i] = ir_new_nil(emitter, expr->site);
				}
			}
			IrArray ir_args = {arguments, alloc_arguments};

			ir = failed ? ERROR_IR : ir_new_call(emitter, expr->site, ir_expr, ir_args);
		}
		break;

		case AST_FUNCTION:
		{
			IrFunction *definition = ir_lower_function(emitter, expr, expr->site, expr->function.variadic, expr->function.params, expr->function.body);
			ir = ir_new_function(emitter, expr->site, definition);
		}
		break;

		case AST_RECURSE:
		{
			ASSERT(emitter->function);
			ir = ir_new_recurse(emitter, expr->site);
		}
		break;

		case AST_TABLE:
		{
			Ast *args = expr->table.items;
			u32 nargs = expr->table.count;

			IrQueue table_items = ir_begin_queue(emitter);
			Ir table_local = ir_new_local(emitter, expr->site, ir_new_table(emitter, expr->site));
			ir_push(&table_items, table_local);

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast entry = ast_ensure_kind(args[i], AST_TABLE_ENTRY);
				if (ast_is_error(entry))
				{
					ir_report_error(emitter, ERROR_INTERNAL, args[i]->site, "expected table entry");
					ir_push(&table_items, ERROR_IR);
					continue;
				}

				Ast key = entry->table_entry.key;
				Ast value = entry->table_entry.value;
				Ir table = ir_new_load(emitter, entry->site, table_local);
				Ir ir_value = ir_emit_expr(emitter, value);
				if (!key)
				{
					ir_report_error(emitter, ERROR_INTERNAL, entry->site, "expected keyed table entry");
					ir_push(&table_items, ERROR_IR);
					continue;
				}
				Ir ir_key = ir_emit_expr(emitter, key);
				Ir field = ir_new_binary(emitter, entry->site, IR_FIELD, table, ir_key);
				ir_push(&table_items, ir_new_store(emitter, entry->site, field, ir_value));
			}

			Ir value = ir_new_load(emitter, expr->site, table_local);
			IrArray stats = ir_end_queue(&table_items);
			ir = ir_new_expr_block(emitter, expr->site, stats, value);
		}
		break;

		case AST_ARRAY:
		{
			IrQueue array_items = ir_begin_queue(emitter);
			Ir array_local = ir_new_local(emitter, expr->site, ir_new_array(emitter, expr->site));
			ir_push(&array_items, array_local);

			for (u32 i = 0; i < expr->array.count; ++i)
			{
				Ir array = ir_new_load(emitter, expr->site, array_local);
				Ir value = ir_emit_expr(emitter, expr->array.items[i]);
				ir_push(&array_items, ir_new_array_add(emitter, expr->array.items[i]->site, array, value));
			}

			Ir value = ir_new_load(emitter, expr->site, array_local);
			IrArray stats = ir_end_queue(&array_items);
			ir = ir_new_expr_block(emitter, expr->site, stats, value);
		}
		break;

		case AST_TABLE_ENTRY:
		{
			ir_report_error(emitter, ERROR_INTERNAL, expr->site, "table entries are only valid inside table literals");
			ir = ERROR_IR;
		}
		break;

		case AST_GREATER_THAN_EQ:
		{
			Ir x = ir_emit_expr(emitter, expr->binary.x);
			Ir y = ir_emit_expr(emitter, expr->binary.y);
			ir = ir_new_binary(emitter, expr->site, IR_LESS_THAN_EQ, y, x);
		}
		break;

		case AST_GREATER_THAN:
		{
			Ir x = ir_emit_expr(emitter, expr->binary.x);
			Ir y = ir_emit_expr(emitter, expr->binary.y);
			ir = ir_new_binary(emitter, expr->site, IR_LESS_THAN, y, x);
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
		case AST_NIL_OR:
		case AST_SHIFT_LEFT:
		case AST_SHIFT_RIGHT:
		case AST_BITWISE_AND:
		case AST_BITWISE_OR:
		case AST_BITWISE_XOR:
		{
			Ir x = ir_emit_expr(emitter, expr->binary.x);
			Ir y = ir_emit_expr(emitter, expr->binary.y);
			IrKind ir_kind = ir_kind_from_ast_kind(expr->kind);
			ir = ir_new_binary(emitter, expr->site, ir_kind, x, y);
		}
		break;

		case AST_BITWISE_NOT:
		{
			Ir value = ir_emit_expr(emitter, expr->unary);
			ir = ir_new_unary(emitter, expr->site, IR_BITWISE_NOT, value);
		}
		break;
		default:
		{
			ir_report_error(emitter, ERROR_INTERNAL, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			ir = ERROR_IR;
		}
		break;
	}

	return ir;
}

static Ir ir_emit_stat_as_block(IrEmitter *emitter, Ast stat)
{
	if (!stat || ast_is_error(stat)) {
		return ERROR_IR;
	}

	IrQueue queue = ir_begin_queue(emitter);

	IrDefer *defer_scope = ir_begin_defer_scope(emitter);
	{
		if (stat->kind == AST_BLOCK_STAT)
		{
			for (u32 i = 0; i < stat->block.count; ++ i)
			{
				if (stat->block.items[i]->kind != AST_STATIC_DECL) {
					ir_emit_stat(&queue, stat->block.items[i]);
				}
			}
		}
		else
		{
			ir_emit_stat(&queue, stat);
		}
	}
	emit_defer_range(emitter, &queue, emitter->function->defer_scope_start);
	ir_close_defer_scope(emitter, defer_scope);

	ASSERT(emitter->ir_stack_index == queue.start + queue.count);
	IrArray instr = ir_end_queue(&queue);
	Ir ir = ir_new_block(emitter, stat->site, instr);
	return ir;
}

static Ir ir_emit_lvalue_once(IrEmitter *emitter, IrQueue *items, Ast expr)
{
	Ir ir = 0;

	switch (expr->kind)
	{
		case AST_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = ir_emit_expr(emitter, x);
			Ir receiver_memory = ir_new_local(emitter, x->site, receiver);
			ir_push(items, receiver_memory);
			Ir receiver_load = ir_new_load(emitter, x->site, receiver_memory);

			Ir key_load = ir_emit_expr(emitter, y);
			if (y->kind != AST_STRING_LITERAL)
			{
				Ir key_memory = ir_new_local(emitter, y->site, key_load);
				ir_push(items, key_memory);
				key_load = ir_new_load(emitter, y->site, key_memory);
			}

			ir = ir_new_binary(emitter, expr->site, IR_FIELD, receiver_load, key_load);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = ir_emit_expr(emitter, x);
			Ir receiver_memory = ir_new_local(emitter, x->site, receiver);
			ir_push(items, receiver_memory);
			Ir receiver_load = ir_new_load(emitter, x->site, receiver_memory);

			Ir key = ir_emit_expr(emitter, y);
			Ir key_memory = ir_new_local(emitter, y->site, key);
			ir_push(items, key_memory);
			Ir key_load = ir_new_load(emitter, y->site, key_memory);

			ir = ir_new_index(emitter, expr->site, receiver_load, key_load);
		}
		break;

		default:
		{
			ir = ir_emit_expr(emitter, expr);
		}
		break;
	}

	return ir;
}

static u32 lower_local_decl_tuples_to_ir(IrEmitter *emitter, IrQueue *items, Ast name_tuple, Ast expr_tuple, u32 entity_tags)
{
	u32 nvars = name_tuple->tuple.count;

	for (u32 i = 0; i < expr_tuple->tuple.count; ++ i)
	{
		Ast expr = expr_tuple->tuple.items[i];
		if (expr && (expr->kind == AST_RANGE || expr->kind == AST_RANGE_INDEX))
		{
			const char *message = "range expressions cannot be used as declaration values; use a range for loop";
			if (entity_tags & ENTITY_TAG_FORLOOP) {
				message = "range for loops require one range expression";
			}

			ir_report_error(emitter, ERROR_LOWERING, expr->site, message);
			ir_push(items, ERROR_IR);
			return 1;
		}
	}

	if (nvars > 1 && expr_tuple->tuple.count == 1 && ast_is_call(expr_tuple->tuple.items[0]))
	{
		Ir expr = ir_emit_expr(emitter, expr_tuple->tuple.items[0]);
		Ir *locals = arena_push(emitter->module->arena, sizeof(*locals) * nvars, _Alignof(Ir *));
		for (u32 i = 0; i < nvars; ++i)
		{
			Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
			if (ast_is_error(name))
			{
				ir_report_error(emitter, ERROR_INTERNAL, name_tuple->site, "expected declaration name");
				ir_push(items, ERROR_IR);
				return 1;
			}

			locals[i] = ir_new_local(emitter, name->site, 0);

			ASSERT(name->entity);
			ASSERT(!name->entity->ir);
			name->entity->ir = locals[i];
		}
		ir_push(items, ir_new_multi_local(emitter, expr_tuple->site, expr, (IrArray){ .items = locals, .count = nvars }));
		return nvars;
	}

	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			ir_report_error(emitter, ERROR_INTERNAL, name_tuple->site, "expected declaration name");
			ir_push(items, ERROR_IR);
			return 1;
		}

		Ir value = 0;
		if (i < expr_tuple->tuple.count)
		{
			Ast expr = expr_tuple->tuple.items[i];
			value = ir_emit_expr(emitter, expr);
		}
		else
		{
			value = ir_new_nil(emitter, name->site);
		}

		Ir local = ir_new_local(emitter, name->site, value);
		ir_push(items, local);
		ASSERT(name->entity);
		ASSERT(!name->entity->ir);
		name->entity->ir = local;
	}

	return nvars;
}

static Ir lower_ast_for_range_expr_to_ir(IrEmitter *emitter, Ast stat, Ast name_tuple,
	Ast range_expr, u32 entity_tags)
{
	Ast collection_expr = 0;
	Ast range = range_expr;
	if (range_expr && range_expr->kind == AST_RANGE_INDEX)
	{
		collection_expr = range_expr->binary.x;
		range = range_expr->binary.y;
	}

	if (!range || range->kind != AST_RANGE)
	{
		ir_report_error(emitter, ERROR_LOWERING, stat->site, "range for loops require a range expression");
		return ERROR_IR;
	}

	u32 nvars = name_tuple->tuple.count;
	Ast first_name = ast_ensure_kind(name_tuple->tuple.items[0], AST_IDENT);
	if (ast_is_error(first_name))
	{
		ir_report_error(emitter, ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
		return ERROR_IR;
	}

	Ir collection_local = 0;
	if (collection_expr)
	{
		Ir collection = ir_emit_expr(emitter, collection_expr);
		collection_local = ir_new_local(emitter, collection_expr->site, collection);
	}

	Ast range_start_expr = range->binary.x;
	Ast range_end_expr = range->binary.y;
	SourceSite range_start_site = range_start_expr ? range_start_expr->site : range->site;
	SourceSite range_end_site = range_end_expr ? range_end_expr->site : range->site;

	Ir range_start = range_start_expr
		? ir_emit_expr(emitter, range_start_expr)
		: ir_new_int(emitter, range_start_site, 0);
	Ir range_end = 0;
	if (range_end_expr) {
		range_end = ir_emit_expr(emitter, range_end_expr);
	}
	else if (collection_local)
	{
		Ir collection = ir_new_load(emitter, range_end_site, collection_local);
		range_end = ir_new_measure(emitter, range_end_site, collection);
	}
	else
	{
		ir_report_error(emitter, ERROR_LOWERING, range->site,
			"an omitted range end requires an indexed collection");
		return ERROR_IR;
	}

	Ir iterator_local = ir_new_local(emitter, range_start_site, range_start);
	Ir range_end_local = ir_new_local(emitter, range_end_site, range_end);

	Ir *var_locals = arena_push(emitter->module->arena, sizeof(*var_locals) * nvars, _Alignof(Ir));
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			ir_report_error(emitter, ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
			return ERROR_IR;
		}

		Ir local = ir_new_local(emitter, name->site, 0);
		var_locals[i] = local;
		ASSERT(name->entity);
		ASSERT(!name->entity->ir);
		name->entity->ir = local;
	}

	Ir start_label = ir_new_label(emitter, stat->site);
	Ir continue_label = ir_new_label(emitter, first_name->site);
	Ir break_label = ir_new_label(emitter, stat->site);

	IrLoopFrame loop_frame = {};
	loop_frame.continue_label = continue_label;
	loop_frame.break_label = break_label;
	loop_frame.defer_start = emitter->function->defer;
	push_loop(emitter, &loop_frame);
	Ir body = ir_emit_stat_as_block(emitter, stat->for_range_stat.body);
	pop_loop(emitter, &loop_frame);

	Ir load_iterator_local = ir_new_load(emitter, first_name->site, iterator_local);
	Ir load_range_end_local = ir_new_load(emitter, range_end_site, range_end_local);
	Ir iterator_less_than_range_end = ir_new_binary(emitter, range->site, IR_LESS_THAN, load_iterator_local, load_range_end_local);
	Ir exit_jump = ir_new_jump_if_false(emitter, range->site, iterator_less_than_range_end, break_label);

	IrQueue loop_items = ir_begin_queue(emitter);
	ir_push(&loop_items, start_label);
	ir_push(&loop_items, exit_jump);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = name_tuple->tuple.items[i];
		Ir iterator_load = ir_new_load(emitter, name->site, iterator_local);
		Ir value = iterator_load;
		if (i != 0)
		{
			Ir offset = ir_new_int(emitter, name->site, i);
			value = ir_new_binary(emitter, name->site, IR_ADD, iterator_load, offset);
		}

		if (collection_local)
		{
			Ir collection = ir_new_load(emitter, name->site, collection_local);
			value = ir_new_index(emitter, name->site, collection, value);
		}

		ir_push(&loop_items, ir_new_store(emitter, name->site, var_locals[i], value));
	}

	Ir iterator_load_for_add = ir_new_load(emitter, first_name->site, iterator_local);
	Ir stride = ir_new_int(emitter, first_name->site, nvars);
	Ir next_value = ir_new_binary(emitter, first_name->site, IR_ADD, iterator_load_for_add, stride);
	Ir increment = ir_new_store(emitter, first_name->site, iterator_local, next_value);

	ir_push(&loop_items, body);
	ir_push(&loop_items, continue_label);
	ir_push(&loop_items, increment);
	ir_push(&loop_items, ir_new_jump(emitter, first_name->site, start_label));
	ir_push(&loop_items, break_label);
	IrArray loop_stats = ir_end_queue(&loop_items);
	Ir loop = ir_new_block(emitter, stat->site, loop_stats);

	IrQueue block_items = ir_begin_queue(emitter);
	if (collection_local) {
		ir_push(&block_items, collection_local);
	}
	ir_push(&block_items, iterator_local);
	ir_push(&block_items, range_end_local);
	for (u32 i = 0; i < nvars; ++ i)
	{
		ir_push(&block_items, var_locals[i]);
	}
	ir_push(&block_items, loop);

	IrArray stats = ir_end_queue(&block_items);
	Ir ir = ir_new_block(emitter, stat->site, stats);

	return ir;
}

static Ir lower_ast_for_range_to_ir(IrEmitter *emitter, Ast stat)
{
	Ast decl = ast_ensure_kind(stat->for_range_stat.decl, AST_DECL_STAT);
	if (ast_is_error(decl))
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	Ast name_tuple = ast_ensure_kind(decl->decl.name, AST_TUPLE);
	Ast expr_tuple = ast_ensure_kind(decl->decl.expr, AST_TUPLE);
	if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	if (name_tuple->tuple.count == 0 || expr_tuple->tuple.count != 1)
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid range for-loop arity");
		return ERROR_IR;
	}

	Ast range = expr_tuple->tuple.items[0];
	if (!range || (range->kind != AST_RANGE && range->kind != AST_RANGE_INDEX))
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid range for-loop expression");
		return ERROR_IR;
	}

	u32 entity_tags = decl->decl.constant ? ENTITY_TAG_CONSTANT : 0;
	return lower_ast_for_range_expr_to_ir(emitter, stat, name_tuple, range, entity_tags);
}

static Ir lower_ast_for_to_ir(IrEmitter *emitter, Ast stat)
{
	Ast init = ast_ensure_kind(stat->for_stat.init, AST_DECL_STAT);
	if (ast_is_error(init))
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid C-style for-loop initializer");
		return ERROR_IR;
	}

	IrQueue block_items = ir_begin_queue(emitter);
	ir_emit_stat(&block_items, init);

	Ir condition_label = ir_new_label(emitter, stat->site);
	Ir continue_label = ir_new_label(emitter, stat->for_stat.step->site);
	Ir break_label = ir_new_label(emitter, stat->site);

	IrLoopFrame loop_frame = {};
	loop_frame.continue_label = continue_label;
	loop_frame.break_label = break_label;
	loop_frame.defer_start = emitter->function->defer;
	push_loop(emitter, &loop_frame);
	Ir body = ir_emit_stat_as_block(emitter, stat->for_stat.body);
	pop_loop(emitter, &loop_frame);

	Ir pred = ir_emit_expr(emitter, stat->for_stat.pred);
	Ir exit_jump = ir_new_jump_if_false(emitter, stat->for_stat.pred->site, pred, break_label);
	Ir step = ir_emit_stat_as_block(emitter, stat->for_stat.step);

	IrQueue loop_items = ir_begin_queue(emitter);
	ir_push(&loop_items, condition_label);
	ir_push(&loop_items, exit_jump);
	ir_push(&loop_items, body);
	ir_push(&loop_items, continue_label);
	ir_push(&loop_items, step);
	ir_push(&loop_items, ir_new_jump(emitter, stat->site, condition_label));
	ir_push(&loop_items, break_label);
	IrArray loop_stats = ir_end_queue(&loop_items);
	ir_push(&block_items, ir_new_block(emitter, stat->site, loop_stats));

	IrArray stats = ir_end_queue(&block_items);
	Ir ir = ir_new_block(emitter, stat->site, stats);
	return ir;
}

static Ir lower_compound_assign_to_ir(IrEmitter *emitter, IrQueue *items, Ast stat)
{
	Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
	Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
	if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
	{
		ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid compound assignment");
		return ERROR_IR;
	}

	if (dest_tuple->tuple.count != 1 || expr_tuple->tuple.count != 1)
	{
		ir_report_error(emitter, ERROR_INVALID_ASSIGNMENT, stat->site, "compound assignment requires one destination and one value");
		return ERROR_IR;
	}

	IrKind op = ir_kind_from_compound_assign_ast_kind(stat->kind);
	ASSERT(op != IR_NONE);

	Ast dest_ast = dest_tuple->tuple.items[0];
	Ast expr_ast = expr_tuple->tuple.items[0];
	if (!mark_entity_assigned(emitter, dest_ast)) return ERROR_IR;

	Ir dest_ir = ir_emit_lvalue_once(emitter, items, dest_ast);
	Ir right_ir = ir_emit_expr(emitter, expr_ast);
	Ir value_ir = ir_new_binary(emitter, stat->site, op, dest_ir, right_ir);
	Ir destination_ir = dest_ir;
	if (destination_ir->kind == IR_LOAD) {
		destination_ir = destination_ir->ir_load.local;
	}
	return ir_new_store(emitter, stat->site, destination_ir, value_ir);
}

static void ir_emit_stat(IrQueue *items, Ast stat)
{
	IrEmitter *emitter = items->emitter;

	if (!stat || ast_is_error(stat))
	{
		ir_push(items, ERROR_IR);
		return;
	}

	switch (stat->kind)
	{
		case AST_IF:
		{
			Ast pred = stat->if_stat.pred;
			Ast true_clause = stat->if_stat.true_clause;
			Ast else_clause = stat->if_stat.else_clause;

			Ir pred_ir = ir_emit_expr(emitter, pred);
			Ir true_ir = ir_emit_stat_as_block(emitter, true_clause);
			Ir else_ir = 0;
			if (else_clause) {
				else_ir = ir_emit_stat_as_block(emitter, else_clause);
			}

			Ir ir = ir_new_if(emitter, stat->site, pred_ir, true_ir, else_ir);
			ir_push(items, ir);
		}
		break;

		case AST_WHILE:
		{
			Ast pred = stat->while_stat.pred;
			Ast body = stat->while_stat.body;

			Ir start_label = ir_new_label(emitter, stat->site);
			Ir break_label = ir_new_label(emitter, stat->site);

			IrLoopFrame loop_frame = {};
			loop_frame.continue_label = start_label;
			loop_frame.break_label = break_label;
			loop_frame.defer_start = emitter->function->defer;
			push_loop(emitter, &loop_frame);
			Ir body_ir = ir_emit_stat_as_block(emitter, body);
			pop_loop(emitter, &loop_frame);

			Ir pred_ir = ir_emit_expr(emitter, pred);
			Ir jump_if_false = ir_new_jump_if_false(emitter, pred->site, pred_ir, break_label);

			IrQueue loop_items = ir_begin_queue(emitter);
			ir_push(&loop_items, start_label);
			ir_push(&loop_items, jump_if_false);
			ir_push(&loop_items, body_ir);
			ir_push(&loop_items, ir_new_jump(emitter, stat->site, start_label));
			ir_push(&loop_items, break_label);

			IrArray loop_stats = ir_end_queue(&loop_items);
			Ir ir = ir_new_block(emitter, stat->site, loop_stats);
			ir_push(items, ir);
		}
		break;

		case AST_FOR:
		{
			Ir ir = lower_ast_for_to_ir(emitter, stat);
			ir_push(items, ir);
		}
		break;

		case AST_FOR_RANGE:
		{
			Ir ir = lower_ast_for_range_to_ir(emitter, stat);
			ir_push(items, ir);
		}
		break;

		case AST_BLOCK_STAT:
		{
			ir_push(items, ir_emit_stat_as_block(emitter, stat));
		}
		break;

		case AST_STATIC_DECL:
		{
			// NOTE(RJ): functions are lazily lowered
		}
		break;

		case AST_DECL_STAT:
		{
			Ast name_tuple = ast_ensure_kind(stat->decl.name, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->decl.expr, AST_TUPLE);
			if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
			{
				ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid declaration");
				ir_push(items, ERROR_IR);
				break;
			}

			u32 entity_tags = stat->decl.constant ? ENTITY_TAG_CONSTANT : 0;
			lower_local_decl_tuples_to_ir(emitter, items, name_tuple, expr_tuple, entity_tags);
		}
		break;

		case AST_ASSIGN:
		{
			Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid assignment");
				ir_push(items, ERROR_IR);
				break;
			}

			u32 nargs = dest_tuple->tuple.count;
			if (nargs == 1)
			{
				Ast dest_ast = dest_tuple->tuple.items[0];
				if (!mark_entity_assigned(emitter, dest_ast)) {
					ir_push(items, ERROR_IR);
					break;
				}
				Ast expr_ast = expr_tuple->tuple.count ? expr_tuple->tuple.items[0] : 0;

				Ir dest_ir = ir_emit_expr(emitter, dest_ast);
				Ir expr_ir = expr_ast ? ir_emit_expr(emitter, expr_ast) : ir_new_nil(emitter, dest_ast->site);
				if (dest_ir->kind == IR_LOAD) {
					dest_ir = dest_ir->ir_load.local;
				}
				Ir ir = ir_new_store(emitter, stat->site, dest_ir, expr_ir);
				ir_push(items, ir);
				break;
			}

			// TODO(RJ): we don't need multi-local! could be done with the same mechanism!
			Ir *values = arena_push(emitter->module->arena, sizeof(*values) * nargs, _Alignof(Ir));
			if (expr_tuple->tuple.count == 1 && ast_is_call(expr_tuple->tuple.items[0]))
			{
				Ast expr_ast = expr_tuple->tuple.items[0];
				Ir expr_ir = ir_emit_expr(emitter, expr_ast);
				for (u32 i = 0; i < nargs; ++i) {
					values[i] = ir_new_local(emitter, expr_ast->site, 0);
				}
				ir_push(items, ir_new_multi_local(emitter, expr_ast->site, expr_ir, (IrArray){ .items = values, .count = nargs }));
			}
			else
			{
				for (u32 i = 0; i < nargs; ++ i)
				{
					SourceSite site = stat->site;
					Ir expr_ir;
					if (i < expr_tuple->tuple.count) {
						Ast expr_ast = expr_tuple->tuple.items[i];
						site = expr_ast->site;
						expr_ir = ir_emit_expr(emitter, expr_ast);
					}
					else {
						expr_ir = ir_new_nil(emitter, site);
					}

					values[i] = ir_new_local(emitter, site, expr_ir);
					ir_push(items, values[i]);
				}
			}

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast dest_ast = dest_tuple->tuple.items[i];
				if (!mark_entity_assigned(emitter, dest_ast)) {
					ir_push(items, ERROR_IR);
					continue;
				}
				Ir dest_ir = ir_emit_expr(emitter, dest_ast);
				Ir value_ir = ir_new_load(emitter, values[i]->site, values[i]);
				if (dest_ir->kind == IR_LOAD) {
					dest_ir = dest_ir->ir_load.local;
				}
				Ir ir = ir_new_store(emitter, stat->site, dest_ir, value_ir);
				ir_push(items, ir);
			}
		}
		break;

		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_MOD_ASSIGN:
		case AST_XOR_ASSIGN:
		case AST_SHL_ASSIGN:
		case AST_SHR_ASSIGN:
		{
			Ir ir = lower_compound_assign_to_ir(emitter, items, stat);
			ir_push(items, ir);
		}
		break;

		case AST_NIL_ASSIGN:
		{
			Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				ir_report_error(emitter, ERROR_INTERNAL, stat->site, "invalid nil assignment");
				ir_push(items, ERROR_IR);
				break;
			}
			if (dest_tuple->tuple.count != expr_tuple->tuple.count)
			{
				ir_report_error(emitter, ERROR_INVALID_ASSIGNMENT, stat->site, "nil assignment requires one value for each destination");
				ir_push(items, ERROR_IR);
				break;
			}

			for (u32 i = 0; i < dest_tuple->tuple.count; ++ i)
			{
				Ast dest_ast = dest_tuple->tuple.items[i];
				Ast expr_ast = expr_tuple->tuple.items[i];
				if (!mark_entity_assigned(emitter, dest_ast)) {
					ir_push(items, ERROR_IR);
					continue;
				}

				Ir dest_ir = ir_emit_lvalue_once(emitter, items, dest_ast);
				Ir nil_ir = ir_new_nil(emitter, dest_ast->site);
				Ir pred_ir = ir_new_binary(emitter, stat->site, IR_EQ, dest_ir, nil_ir);
				Ir value_ir = ir_emit_expr(emitter, expr_ast);
				Ir destination_ir = dest_ir;
				if (destination_ir->kind == IR_LOAD) {
					destination_ir = destination_ir->ir_load.local;
				}
				Ir store_ir = ir_new_store(emitter, stat->site, destination_ir, value_ir);

				IrQueue true_items = ir_begin_queue(emitter);
				ir_push(&true_items, store_ir);
				IrArray true_stats = ir_end_queue(&true_items);
				Ir true_clause = ir_new_block(emitter, stat->site, true_stats);
				Ir ir = ir_new_if(emitter, stat->site, pred_ir, true_clause, 0);
				ir_push(items, ir);
			}
		}
		break;

		case AST_RETURN:
		{
			Ast expr = stat->return_stat.expr;
			Ir expr_ir = 0;
			if (expr)
			{
				if (expr->kind == AST_TUPLE && expr->tuple.count == 1) {
					expr = expr->tuple.items[0];
				}
				expr_ir = ir_emit_expr(emitter, expr);
			}
			if (expr_ir && emitter->function->defer)
			{
				Ir return_value = ir_new_local(emitter, stat->site, expr_ir);
				ir_push(items, return_value);
				expr_ir = ir_new_load(emitter, stat->site, return_value);
			}
			emit_defer_range(emitter, items, 0);
			Ir ir = ir_new_return(emitter, stat->site, expr_ir);
			ir_push(items, ir);
		}
		break;

		case AST_BREAK:
		{
			IrLoopFrame loop = current_loop(emitter, stat->site);
			emit_defer_range(emitter, items, loop.defer_start);
			Ir ir = ir_new_jump(emitter, stat->site, loop.break_label);
			ir_push(items, ir);
		}
		break;

		case AST_CONTINUE:
		{
			IrLoopFrame loop = current_loop(emitter, stat->site);
			emit_defer_range(emitter, items, loop.defer_start);
			Ir ir = ir_new_jump(emitter, stat->site, loop.continue_label);
			ir_push(items, ir);
		}
		break;

		case AST_DEFER_STAT:
		{
			push_defer_stat(emitter, stat->defer_stat.body);
		}
		break;

		case AST_TUPLE:
		{
			Ast *args = stat->tuple.items;
			u32 nargs = stat->tuple.count;

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ir ir = ir_emit_expr(emitter, args[i]);
				ir_push(items, ir);
			}
		}
		break;

		default:
		{
			Ir ir = ir_emit_expr(emitter, stat);
			ir_push(items, ir);
		}
		break;
	}
}

static IrFunction *ir_reserve_function(IrEmitter *emitter, SourceSite site, b32 variadic, u32 param_count)
{
	IrFunction *definition = arena_push_zero(emitter->module->arena, sizeof(*definition), _Alignof(IrFunction));
	definition->index = IR_FUNCTION_INDEX_INVALID;
	definition->site = site;
	definition->variadic = variadic;
	definition->param_count = IMPLICIT_PARAM_COUNT + param_count;
	if (emitter->module->last_function)
	{
		emitter->module->last_function->next = definition;
	}
	else
	{
		emitter->module->first_function = definition;
	}
	emitter->module->last_function = definition;
	emitter->module->function_count += 1;
	return definition;
}

static void ir_lower_function_body(IrEmitter *emitter, IrFunction *definition, Ast tree, Ast_Array ast_params, Ast body)
{
	ASSERT(definition);
	ASSERT(tree);
	ASSERT(!definition->ast || definition->ast == tree);
	ASSERT(definition->param_count == IMPLICIT_PARAM_COUNT + ast_params.count);
	definition->ast = tree;

	IrEmitterFrame function_ctx = {};
	function_ctx.parent = emitter->function;
	function_ctx.tree = tree;

	IrEmitterFrame *outer_function = emitter->function;
	emitter->function = &function_ctx;

	Ir *ir_params = arena_push_zero(emitter->module->arena, sizeof(*ir_params) * (IMPLICIT_PARAM_COUNT + ast_params.count), _Alignof(Ir));
	ir_params[IMPLICIT_PARAM_INDEX] = ir_new_local(emitter, definition->site, 0);
	definition->params = ir_params;

	for (u32 i = 0; i < ast_params.count; ++ i)
	{
		Ast param = ast_ensure_kind(ast_params.items[i], AST_FUNCTION_PARAM);
		if (ast_is_error(param)) {
			ir_report_error(emitter, ERROR_INTERNAL, param->site, "expected function parameter");
			continue;
		}

		Ast name = ast_ensure_kind(param->param.name, AST_IDENT);
		if (ast_is_error(name)) {
			ir_report_error(emitter, ERROR_INTERNAL, param->site, "expected function parameter name");
			continue;
		}

		Ir param_memory = ir_new_local(emitter, name->site, 0);
		ir_params[IMPLICIT_PARAM_COUNT + i] = param_memory;
		ASSERT(name->entity);
		name->entity->ir = param_memory;

		Ast type = param->param.type;
		if (type)
		{
			type = ast_ensure_kind(type, AST_IDENT);
			if (ast_is_error(type))
			{
				ir_report_error(emitter, ERROR_INTERNAL, param->site, "expected function parameter type name");
			}
			else
			{
				TypeRule rule = type_rule_from_atom(type->atom);
				if (rule == TRULE_NONE)
				{
					ir_report_error(emitter, ERROR_LOWERING, type->site, "'%s' is not a known type name", atom_data(type->atom));
				}
				else
				{
					param_memory->type_rule = rule;
				}
			}
		}
	}

	definition->body = ir_emit_stat_as_block(emitter, body);

	IrArray captures = {};
	captures.count = function_ctx.capture_count;
	if (captures.count) {
		captures.items = arena_push(emitter->module->arena, sizeof(*captures.items) * captures.count, _Alignof(Ir));
	}
	u32 capture_index = 0;
	for (IrCapture *capture = function_ctx.first_capture; capture; capture = capture->next)
	{
		ASSERT(capture_index < captures.count);
		captures.items[capture_index++] = capture->target;
	}
	ASSERT(capture_index == captures.count);


	emitter->function = outer_function;

	definition->captures = captures;
}

static IrFunction *ir_lower_function(IrEmitter *emitter, Ast tree, SourceSite site, b32 variadic, Ast_Array ast_params, Ast body)
{
	IrFunction *definition = ir_reserve_function(emitter, site, variadic, ast_params.count);
	ir_lower_function_body(emitter, definition, tree, ast_params, body);
	return definition;
}

IrModule *ir_emit_module(Compiler *compiler, Ast ast)
{
	ASSERT(compiler);
	ASSERT(compiler->arena);
	ASSERT(ast);
	ASSERT(ast->kind == AST_FILE);

	IrModule *module = arena_push_zero(compiler->arena, sizeof(*module), 8);
	module->arena = compiler->arena;

	IrEmitter *emitter = arena_push_zero(module->arena, sizeof(*emitter), _Alignof(IrEmitter));
	emitter->compiler = compiler;
	emitter->module = module;
	emitter->ir_stack_size = 4096;
	emitter->ir_stack = arena_push_zero(module->arena, sizeof(*emitter->ir_stack) * emitter->ir_stack_size, _Alignof(Ir));
	IrFunction *entry = ir_lower_function(emitter, ast, ast->site, 1, (Ast_Array){0}, ast->file.body);
	ASSERT(entry == module->first_function);
	ASSERT(!entry->captures.items && !entry->captures.count);
	return module;
}
