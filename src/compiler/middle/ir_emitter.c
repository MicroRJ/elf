//
// See Copyright Notice In elf.h
//

#include <dy.h>

#include "elf.h"
#include "base.h"
#include "atom_view.h"
#include "compiler.h"
#include "ir.h"
#include "ast.h"
#include "ir_emitter.h"
#include "token.h"

typedef struct IrEntity IrEntity;
typedef struct IrEmitter IrEmitter;

typedef enum
{
	ENTITY_INVALID = 0,
	ENTITY_LOCAL_DECLARATION,
}
IrEntityType;

typedef EntityTags IrEntityTags;

typedef u32 IrEntityId;

struct IrEntity
{
	IrEntityType  type;
	IrEntityTags  tags;
	u32           scope_start;
	Atom         *name;
	SourceSite    site;
	Ir            memory_ir;
};

typedef struct
{
	IrEntityId previous_scope_start;
	u32        previous_scope_depth;
}
EntityScope;

typedef struct
{
	u32 previous_defer_start;
}
DeferScope;

typedef struct
{
	Ir continue_label;
	Ir break_label;
	u32 defer_start;
}
LoopLabels;

typedef struct LwScopeFunction LwScopeFunction;
typedef struct LwCapture LwCapture;

struct LwCapture
{
	LwCapture *next;
	IrEntity  *entity;
	Ir         source;
};

struct LwScopeFunction
{
	LwScopeFunction *parent;
	IrFunDef        *function;
	LwCapture       *first_capture;
	LwCapture       *last_capture;
	u32              capture_count;
	IrEntityId       scope_start;
};

struct IrEmitter
{
	Compiler        *compiler;
	Checker         *checker;
	IrModule        *module;

	u32              ir_stack_size;
	u32              ir_stack_index;
	Ir              *ir_stack;

	Ast             *defer_stack;
	u32              defer_stack_size;
	u32              defer_count;
	u32              defer_scope_start;
	u32              function_defer_start;

	IrEntityId       scope_start;
	IrEntityId       scope_end;
	u32              scope_depth;

	LwScopeFunction *function;

	u32              loop_count;

	IrEntity         entities[MAX_ENTITIES];
	LoopLabels       loop_stack[64];
};


#include "ir.c"

typedef struct
{
	IrEmitter *ctx;
	u32           start;
	u32           count;
}
IrFrame;

static void ir_lower_stat(IrFrame *block, Ast stat);
static Ir ir_emit_expr(IrEmitter *ctx, Ast expr);
static Ir ir_lower_stat_as_block(IrEmitter *ctx, Ast stat);

static void report_lowering_error(IrEmitter *ctx, Com_Error error, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	compiler_reportv(ctx->compiler, error, COMPILER_ERROR, COMPILER_PHASE_LOWERING, site, format, args);
	va_end(args);
}

static void report_lowering_warning(IrEmitter *ctx, Com_Error error, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	compiler_reportv(ctx->compiler, error, COMPILER_WARNING, COMPILER_PHASE_LOWERING, site, format, args);
	va_end(args);
}

static IrFrame begin_ir_frame(IrEmitter *ctx)
{
	IrFrame block = {};
	block.ctx = ctx;
	block.start = ctx->ir_stack_index;
	block.count = 0;
	return block;
}

static void push_ir(IrFrame *stack, Ir ir)
{
	IrEmitter *ctx = stack->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == stack->start + stack->count);
	ASSERT(ctx->ir_stack_index < ctx->ir_stack_size);
	ctx->ir_stack[ctx->ir_stack_index++] = ir;
	stack->count += 1;
}

static IrArray close_ir_frame(IrFrame *frame)
{
	IrEmitter *ctx = frame->ctx;
	ASSERT(ctx);
	ASSERT(ctx->ir_stack_index == frame->start + frame->count);

	ctx->ir_stack_index -= frame->count;

	IrArray items = {};
	items.count = frame->count;
	items.items = elf_arena_push_copy(ctx->module->arena, sizeof(*items.items) * items.count, ctx->ir_stack + ctx->ir_stack_index);

	ASSERT(ctx->ir_stack_index == frame->start);
	frame->count = 0;
	return items;
}

static EntityScope get_entity_scope(IrEmitter *ctx)
{
	EntityScope scope = {};
	scope.previous_scope_start = ctx->scope_start;
	scope.previous_scope_depth = ctx->scope_depth;

	ctx->scope_start = ctx->scope_end;
	ctx->scope_depth += 1;
	return scope;
}

static void set_entity_scope(IrEmitter *ctx, EntityScope scope)
{
	IrEntityId current_start = ctx->scope_start;
	for (i32 i = ctx->scope_end - 1; i >= (i32)current_start; -- i)
	{
		if (~ctx->entities[i].tags & ENTITY_TAG_REFERENCED) {
			report_lowering_warning(ctx, COM_ERROR_UNREFERENCED_ENTITY, ctx->entities[i].site, "unreferenced entity");
		}
	}

	ASSERT(current_start <= ctx->scope_end);
	ASSERT(scope.previous_scope_start <= current_start);

	ctx->scope_end = current_start;
	ctx->scope_start = scope.previous_scope_start;
	ctx->scope_depth = scope.previous_scope_depth;
}

static DeferScope get_defer_scope(IrEmitter *ctx)
{
	DeferScope scope = {};
	scope.previous_defer_start = ctx->defer_scope_start;
	ctx->defer_scope_start = ctx->defer_count;
	return scope;
}

static void set_defer_scope(IrEmitter *ctx, DeferScope scope)
{
	ctx->defer_count = ctx->defer_scope_start;
	ctx->defer_scope_start = scope.previous_defer_start;
}

static void push_defer_stat(IrEmitter *ctx, Ast stat)
{
	ASSERT(ctx->defer_count < ctx->defer_stack_size);
	ctx->defer_stack[ctx->defer_count++] = stat;
}

static void emit_defer_range(IrEmitter *ctx, IrFrame *items, u32 start)
{
	ASSERT(start <= ctx->defer_count);
	for (u32 i = ctx->defer_count; i > start; --i)
	{
		Ast deferred = ctx->defer_stack[i - 1];
		ir_lower_stat(items, deferred);
	}
}

static IrEntity *entity_from_name(IrEmitter *ctx, Atom *name)
{
	for (i32 i = ctx->scope_end - 1; i >= 0; -- i)
	{
		IrEntity *en = & ctx->entities[i];
		if (en->name == name) {
			return en;
		}
	}
	return 0;
}

static IrEntity *declare_local_entity(IrEmitter *ctx, SourceSite site, u32 tags, Atom *name)
{
	IrEntity *en = entity_from_name(ctx, name);
	const char *name_data = atom_data(name);
	if (en)
	{
		if (en->scope_start == ctx->scope_start) {
			report_lowering_error(ctx, COM_ERROR_LOWERING, site, "'%s': is already declared", name_data);
		}
	}

	ASSERT(ctx->scope_end < MAX_ENTITIES);

	en = & ctx->entities[ctx->scope_end ++];
	zero_memory(en, sizeof(* en));
	en->type = ENTITY_LOCAL_DECLARATION;
	en->tags = tags;
	en->name = name;
	en->site = site;
	en->scope_start = ctx->scope_start;
	return en;
}

static u32 capture_entity_in_function(IrEmitter *ctx, LwScopeFunction *function_ctx, IrEntity *entity, SourceSite site)
{
	u32 capture_index = 0;
	for (LwCapture *capture = function_ctx->first_capture; capture; capture = capture->next)
	{
		if (capture->entity == entity) {
			return capture_index;
		}
		capture_index += 1;
	}

	ASSERT(function_ctx->capture_count < 255);
	ASSERT(capture_index == function_ctx->capture_count);

	Ir source = 0;
	LwScopeFunction *parent = function_ctx->parent;
	if (parent && entity->scope_start < parent->scope_start)
	{
		u32 parent_capture = capture_entity_in_function(ctx, parent, entity, site);
		source = ir_new_capture(ctx, site, parent_capture);
	}
	else
	{
		source = ir_new_load_local(ctx, site, entity->memory_ir);
	}

	LwCapture *capture = elf_arena_push_zero(ctx->module->arena, sizeof(*capture));
	capture->entity = entity;
	capture->source = source;
	if (function_ctx->last_capture) {
		function_ctx->last_capture->next = capture;
	}
	else {
		function_ctx->first_capture = capture;
	}
	function_ctx->last_capture = capture;
	function_ctx->capture_count += 1;
	entity->tags |= ENTITY_TAG_REFERENCED;
	return capture_index;
}

static Ir load_entity_ir(IrEmitter *ctx, SourceSite site, IrEntity *entity)
{
	ASSERT(entity->type == ENTITY_LOCAL_DECLARATION);
	entity->tags |= ENTITY_TAG_REFERENCED;
	if (ctx->function && entity->scope_start < ctx->function->scope_start)
	{
		u32 capture_index = capture_entity_in_function(ctx, ctx->function, entity, site);
		return ir_new_capture(ctx, site, capture_index);
	}

	return ir_new_load_local(ctx, site, entity->memory_ir);
}

static b32 mark_entity_assigned(IrEmitter *ctx, Ast destination)
{
	if (!destination || destination->kind != AST_IDENT) {
		return true;
	}
	IrEntity *entity = entity_from_name(ctx, destination->atom);
	if (!entity) {
		return true;
	}
	ASSERT(entity->type == ENTITY_LOCAL_DECLARATION);
	if (entity->tags & ENTITY_TAG_CONSTANT)
	{
		report_lowering_error(ctx, COM_ERROR_LOWERING, destination->site,
			"'%s' is constant and cannot be assigned", atom_data(destination->atom));
		return false;
	}
	entity->tags |= ENTITY_TAG_ASSIGNED;
	return true;
}

static void push_loop_labels(IrEmitter *ctx, LoopLabels labels)
{
	ASSERT(ctx->loop_count < ARRAY_COUNT(ctx->loop_stack));
	ctx->loop_stack[ctx->loop_count ++] = labels;
}

static void pop_loop_labels(IrEmitter *ctx)
{
	ASSERT(ctx->loop_count > 0);
	ctx->loop_count -= 1;
}

static LoopLabels current_loop_labels(IrEmitter *ctx, SourceSite site)
{
	if (ctx->loop_count == 0)
	{
		report_lowering_error(ctx, COM_ERROR_LOWERING, site, "break and continue must be inside a loop");
		LoopLabels labels = {};
		return labels;
	}
	return ctx->loop_stack[ctx->loop_count - 1];
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

static u32 ir_lower_function(IrEmitter *emitter, SourceSite site, b32 variadic, Ast_Array ast_params, Ast body);

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

static IrCallType call_type_from_callee(Ir callee)
{
	IrCallType type = IR_CALL_DYNAMIC;
	if (callee && callee->kind == IR_FUNCTION)
	{
		type = IR_CALL_DIRECT;
	}
	return type;
}

static b32 ast_is_call(Ast expr)
{
	return expr && expr->kind == AST_CALL;
}

static IrArray lower_call_arguments(IrEmitter *ctx, Ast call, b32 *failed)
{
	ASSERT(call && call->kind == AST_CALL);
	u32 argument_count = call->call.args.count;
	b32 has_carg = call->call.carg != 0;
	u32 capacity = argument_count + has_carg;
	IrArray result = {};
	if (capacity) {
		result.items = elf_arena_push(ctx->module->arena, sizeof(*result.items) * capacity);
	}
	for (u32 i = 0; i < argument_count; ++i)
	{
		Ast argument = call->call.args.items[i];
		if (argument->kind == AST_NAMED_ARGUMENT)
		{
			report_lowering_error(ctx, COM_ERROR_LOWERING, argument->site, "named arguments must be resolved before IR lowering");
			*failed = true;
		}
		else
		{
			Ir argument_ir = ir_emit_expr(ctx, argument);
			result.items[result.count++] = argument_ir;
			if (!argument_ir || argument_ir == ERROR_IR) {
				*failed = true;
			}
		}
	}
	if (has_carg)
	{
		Ir argument_ir = ir_emit_expr(ctx, call->call.carg);
		result.items[result.count++] = argument_ir;
		if (!argument_ir || argument_ir == ERROR_IR) {
			*failed = true;
		}
	}

	return result;
}

static Ir ir_emit_expr(IrEmitter *ctx, Ast expr)
{
	if (!expr || ast_is_error(expr)) {
		return ERROR_IR;
	}

	Ir ir = 0;

	const Entity *entity = ast_entity(expr);
	if (entity)
	{
		StaticValue value = entity->value;

		switch (value.type) {

			case STATIC_VALUE_INTEGER: {
				ir = ir_new_int(ctx, expr->site, value.integer);
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
			ir = ir_new_int(ctx, expr->site, expr->integer_value);
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			ir = ir_new_num(ctx, expr->site, expr->number_value);
		}
		break;

		case AST_STRING_LITERAL:
		{
			ir = ir_new_atom(ctx, expr->site, expr->atom);
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			Ast *parts = expr->interpolated_string.items;
			u32 nparts = expr->interpolated_string.count;
			if (nparts < 3 || (nparts & 1) == 0)
			{
				report_lowering_error(ctx, COM_ERROR_INTERNAL, expr->site,
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
					report_lowering_error(ctx, COM_ERROR_INTERNAL, expr->site, "invalid interpolated string part at index %u", i);
					ir = ERROR_IR;
					break;
				}

				Ir part_ir = ir_emit_expr(ctx, part);
				if (!part_ir || part_ir == ERROR_IR)
				{
					ir = ERROR_IR;
					break;
				}

				ir = ir ? ir_new_binary(ctx, part->site, IR_ADD, ir, part_ir) : part_ir;
			}
		}
		break;

		case AST_NIL_LITERAL:
		{
			ir = ir_new_nil(ctx, expr->site);
		}
		break;

		case AST_IDENT:
		{
			Atom *ident = expr->atom;
			IrEntity *en = entity_from_name(ctx, ident);
			if (en) {
				ir = load_entity_ir(ctx, expr->site, en);
			}
			else
			{
				ir = ir_new_global(ctx, expr->site, expr->atom);
			}
		}
		break;

		case AST_META_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = ir_emit_expr(ctx, x);
			Ir y_ir = ir_emit_expr(ctx, y);

			ir = ir_new_binary(ctx, expr->site, IR_META_FIELD, x_ir, y_ir);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir x_ir = ir_emit_expr(ctx, x);
			Ir y_ir = ir_emit_expr(ctx, y);

			ir = ir_new_index(ctx, expr->site, x_ir, y_ir);
		}
		break;

		case AST_MEASURE:
		{
			Ir expr_ir = ir_emit_expr(ctx, expr->unary);
			ir = ir_new_measure(ctx, expr->site, expr_ir);
		}
		break;

		case AST_GET_MEM:
		{
			Ir expr_ir = ir_emit_expr(ctx, expr->unary);
			if (!expr_ir || expr_ir->kind != IR_LOAD_LOCAL)
			{
				report_lowering_error(ctx, COM_ERROR_LOWERING, expr->site
				, "#get_mem expects a local or parameter expression");
				ir = ERROR_IR;
			}
			else
			{
				ir = ir_new_get_mem(ctx, expr->site, expr_ir);
			}
		}
		break;

		case AST_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;
			Ir x_ir = ir_emit_expr(ctx, x);
			Ir y_ir = ir_emit_expr(ctx, y);
			ir = ir_new_binary(ctx, expr->site, IR_FIELD, x_ir, y_ir);
		}
		break;

		case AST_CALL:
		{
			Ir ir_func = ir_emit_expr(ctx, expr->call.expr);
			IrCallType call_type = call_type_from_callee(ir_func);
			b32 failed = !ir_func || ir_func == ERROR_IR;
			IrArray ir_args = lower_call_arguments(ctx, expr, &failed);

			ir = failed ? ERROR_IR : ir_new_call(ctx, expr->site, call_type, ir_func, ir_args);
		}
		break;

		case AST_FUNCTION:
		{
			ASSERT(ctx->module->function_count < ctx->module->function_capacity);

			u32 index = ir_lower_function(ctx, expr->site, expr->function.variadic, expr->function.params, expr->function.body);
			ir = ir_new_closure(ctx, expr->site, index);
		}
		break;

		case AST_RECURSE:
		{
			ASSERT(ctx->function && ctx->function->function);
			ir = ir_new_recurse(ctx, expr->site);
		}
		break;

		case AST_TABLE:
		{
			Ast *args = expr->table.items;
			u32 nargs = expr->table.count;

			IrFrame table_items = begin_ir_frame(ctx);
			Ir table_local = ir_new_local(ctx, expr->site, ir_new_table(ctx, expr->site));
			push_ir(&table_items, table_local);

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast entry = ast_ensure_kind(args[i], AST_TABLE_ENTRY);
				if (ast_is_error(entry))
				{
					report_lowering_error(ctx, COM_ERROR_INTERNAL, args[i]->site, "expected table entry");
					push_ir(&table_items, ERROR_IR);
					continue;
				}

				Ast key = entry->table_entry.key;
				Ast value = entry->table_entry.value;
				Ir table = ir_new_load_local(ctx, entry->site, table_local);
				Ir ir_value = ir_emit_expr(ctx, value);
				if (!key)
				{
					report_lowering_error(ctx, COM_ERROR_INTERNAL, entry->site, "expected keyed table entry");
					push_ir(&table_items, ERROR_IR);
					continue;
				}
				Ir ir_key = ir_emit_expr(ctx, key);
				Ir field = ir_new_binary(ctx, entry->site, IR_FIELD, table, ir_key);
				push_ir(&table_items, ir_new_store(ctx, entry->site, field, ir_value));
			}

			Ir value = ir_new_load_local(ctx, expr->site, table_local);
			IrArray stats = close_ir_frame(&table_items);
			ir = ir_new_expr_block(ctx, expr->site, stats, value);
		}
		break;

		case AST_ARRAY:
		{
			IrFrame array_items = begin_ir_frame(ctx);
			Ir array_local = ir_new_local(ctx, expr->site, ir_new_array(ctx, expr->site));
			push_ir(&array_items, array_local);

			for (u32 i = 0; i < expr->array.count; ++i)
			{
				Ir array = ir_new_load_local(ctx, expr->site, array_local);
				Ir value = ir_emit_expr(ctx, expr->array.items[i]);
				push_ir(&array_items, ir_new_array_add(ctx, expr->array.items[i]->site, array, value));
			}

			Ir value = ir_new_load_local(ctx, expr->site, array_local);
			IrArray stats = close_ir_frame(&array_items);
			ir = ir_new_expr_block(ctx, expr->site, stats, value);
		}
		break;

		case AST_TABLE_ENTRY:
		{
			report_lowering_error(ctx, COM_ERROR_INTERNAL, expr->site, "table entries are only valid inside table literals");
			ir = ERROR_IR;
		}
		break;

		case AST_GREATER_THAN_EQ:
		{
			Ir x = ir_emit_expr(ctx, expr->binary.x);
			Ir y = ir_emit_expr(ctx, expr->binary.y);
			ir = ir_new_binary(ctx, expr->site, IR_LESS_THAN_EQ, y, x);
		}
		break;

		case AST_GREATER_THAN:
		{
			Ir x = ir_emit_expr(ctx, expr->binary.x);
			Ir y = ir_emit_expr(ctx, expr->binary.y);
			ir = ir_new_binary(ctx, expr->site, IR_LESS_THAN, y, x);
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
		case AST_BITWISE_NOT:
		{
			Ir x = ir_emit_expr(ctx, expr->binary.x);
			Ir y = ir_emit_expr(ctx, expr->binary.y);
			IrKind ir_kind = ir_kind_from_ast_kind(expr->kind);
			ir = ir_new_binary(ctx, expr->site, ir_kind, x, y);
		}
		break;
		default:
		{
			report_lowering_error(ctx, COM_ERROR_INTERNAL, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			ir = ERROR_IR;
		}
		break;
	}

	return ir;
}

static Ir ir_lower_stat_as_block(IrEmitter *ctx, Ast stat)
{
	if (!stat || ast_is_error(stat)) {
		return ERROR_IR;
	}

	IrFrame block_items = begin_ir_frame(ctx);
	if (stat->kind == AST_BLOCK_STAT)
	{
		EntityScope scope = get_entity_scope(ctx);
		DeferScope defer_scope = get_defer_scope(ctx);
		for (u32 i = 0; i < stat->block.count; ++ i)
		{
			if (stat->block.items[i]->kind != AST_STATIC_DECL) {
				ir_lower_stat(&block_items, stat->block.items[i]);
			}
		}
		emit_defer_range(ctx, &block_items, ctx->defer_scope_start);
		set_defer_scope(ctx, defer_scope);
		set_entity_scope(ctx, scope);
	}
	else
	{
		ir_lower_stat(&block_items, stat);
	}

	ASSERT(ctx->ir_stack_index == block_items.start + block_items.count);
	IrArray instr = close_ir_frame(&block_items);
	Ir ir = ir_new_block(ctx, stat->site, instr);
	return ir;
}

static Ir lower_ast_lvalue_to_ir_once(IrEmitter *ctx, IrFrame *items, Ast expr)
{
	Ir ir = 0;

	switch (expr->kind)
	{
		case AST_FIELD:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = ir_emit_expr(ctx, x);
			Ir receiver_memory = ir_new_local(ctx, x->site, receiver);
			push_ir(items, receiver_memory);
			Ir receiver_load = ir_new_load_local(ctx, x->site, receiver_memory);

			Ir key_load = ir_emit_expr(ctx, y);
			if (y->kind != AST_STRING_LITERAL)
			{
				Ir key_memory = ir_new_local(ctx, y->site, key_load);
				push_ir(items, key_memory);
				key_load = ir_new_load_local(ctx, y->site, key_memory);
			}

			ir = ir_new_binary(ctx, expr->site, IR_FIELD, receiver_load, key_load);
		}
		break;

		case AST_INDEX:
		{
			Ast x = expr->binary.x;
			Ast y = expr->binary.y;

			Ir receiver = ir_emit_expr(ctx, x);
			Ir receiver_memory = ir_new_local(ctx, x->site, receiver);
			push_ir(items, receiver_memory);
			Ir receiver_load = ir_new_load_local(ctx, x->site, receiver_memory);

			Ir key = ir_emit_expr(ctx, y);
			Ir key_memory = ir_new_local(ctx, y->site, key);
			push_ir(items, key_memory);
			Ir key_load = ir_new_load_local(ctx, y->site, key_memory);

			ir = ir_new_index(ctx, expr->site, receiver_load, key_load);
		}
		break;

		default:
		{
			ir = ir_emit_expr(ctx, expr);
		}
		break;
	}

	return ir;
}

static u32 lower_local_decl_tuples_to_ir(IrEmitter *ctx, IrFrame *items, Ast name_tuple, Ast expr_tuple, u32 entity_tags)
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

			report_lowering_error(ctx, COM_ERROR_LOWERING, expr->site, message);
			push_ir(items, ERROR_IR);
			return 1;
		}
	}

	if (nvars > 1 && expr_tuple->tuple.count == 1 && ast_is_call(expr_tuple->tuple.items[0]))
	{
		Ir expr = ir_emit_expr(ctx, expr_tuple->tuple.items[0]);
		Ir *locals = elf_arena_push(ctx->module->arena, sizeof(*locals) * nvars);
		for (u32 i = 0; i < nvars; ++i)
		{
			Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
			if (ast_is_error(name))
			{
				report_lowering_error(ctx, COM_ERROR_INTERNAL, name_tuple->site, "expected declaration name");
				push_ir(items, ERROR_IR);
				return 1;
			}
			Ir local = ir_new_local(ctx, name->site, 0);
			locals[i] = local;
			IrEntity *entity = declare_local_entity(ctx, name->site, entity_tags, name->atom);
			entity->memory_ir = local;
		}
		push_ir(items, ir_new_multi_local(ctx, expr_tuple->site, expr, (IrArray){ .items = locals, .count = nvars }));
		return nvars;
	}

	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, COM_ERROR_INTERNAL, name_tuple->site, "expected declaration name");
			push_ir(items, ERROR_IR);
			return 1;
		}

		Ir value = 0;
		if (i < expr_tuple->tuple.count)
		{
			Ast expr = expr_tuple->tuple.items[i];
			value = ir_emit_expr(ctx, expr);
		}
		else
		{
			value = ir_new_nil(ctx, name->site);
		}

		Ir local = ir_new_local(ctx, name->site, value);
		push_ir(items, local);

		IrEntity *entity = declare_local_entity(ctx, name->site, entity_tags, name->atom);
		entity->memory_ir = local;
	}

	return nvars;
}

static Ir lower_ast_for_range_expr_to_ir(IrEmitter *ctx, Ast stat, Ast name_tuple,
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
		report_lowering_error(ctx, COM_ERROR_LOWERING, stat->site, "range for loops require a range expression");
		return ERROR_IR;
	}

	EntityScope scope = get_entity_scope(ctx);

	u32 nvars = name_tuple->tuple.count;
	Ast first_name = ast_ensure_kind(name_tuple->tuple.items[0], AST_IDENT);
	if (ast_is_error(first_name))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
		return ERROR_IR;
	}

	Ir collection_local = 0;
	if (collection_expr)
	{
		Ir collection = ir_emit_expr(ctx, collection_expr);
		collection_local = ir_new_local(ctx, collection_expr->site, collection);
	}

	Ast range_start_expr = range->binary.x;
	Ast range_end_expr = range->binary.y;
	SourceSite range_start_site = range_start_expr ? range_start_expr->site : range->site;
	SourceSite range_end_site = range_end_expr ? range_end_expr->site : range->site;

	Ir range_start = range_start_expr
		? ir_emit_expr(ctx, range_start_expr)
		: ir_new_int(ctx, range_start_site, 0);
	Ir range_end = 0;
	if (range_end_expr) {
		range_end = ir_emit_expr(ctx, range_end_expr);
	}
	else if (collection_local)
	{
		Ir collection = ir_new_load_local(ctx, range_end_site, collection_local);
		range_end = ir_new_measure(ctx, range_end_site, collection);
	}
	else
	{
		report_lowering_error(ctx, COM_ERROR_LOWERING, range->site,
			"an omitted range end requires an indexed collection");
		set_entity_scope(ctx, scope);
		return ERROR_IR;
	}

	Ir iterator_local = ir_new_local(ctx, range_start_site, range_start);
	Ir range_end_local = ir_new_local(ctx, range_end_site, range_end);

	Ir *var_locals = elf_arena_push(ctx->module->arena, sizeof(*var_locals) * nvars);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = ast_ensure_kind(name_tuple->tuple.items[i], AST_IDENT);
		if (ast_is_error(name))
		{
			report_lowering_error(ctx, COM_ERROR_INTERNAL, name_tuple->site, "expected for-loop variable name");
			set_entity_scope(ctx, scope);
			return ERROR_IR;
		}

		Ir local = ir_new_local(ctx, name->site, 0);
		var_locals[i] = local;

		IrEntity *entity = declare_local_entity(ctx, name->site, ENTITY_TAG_FORLOOP | entity_tags, name->atom);
		entity->memory_ir = local;
	}

	Ir start_label = ir_new_label(ctx, stat->site);
	Ir continue_label = ir_new_label(ctx, first_name->site);
	Ir break_label = ir_new_label(ctx, stat->site);

	LoopLabels loop_labels = {};
	loop_labels.continue_label = continue_label;
	loop_labels.break_label = break_label;
	loop_labels.defer_start = ctx->defer_count;
	push_loop_labels(ctx, loop_labels);
	Ir body = ir_lower_stat_as_block(ctx, stat->for_range_stat.body);
	pop_loop_labels(ctx);

	Ir load_iterator_local = ir_new_load_local(ctx, first_name->site, iterator_local);
	Ir load_range_end_local = ir_new_load_local(ctx, range_end_site, range_end_local);
	Ir iterator_less_than_range_end = ir_new_binary(ctx, range->site, IR_LESS_THAN, load_iterator_local, load_range_end_local);
	Ir exit_jump = ir_new_jump_if_false(ctx, range->site, iterator_less_than_range_end, break_label);

	IrFrame loop_items = begin_ir_frame(ctx);
	push_ir(&loop_items, start_label);
	push_ir(&loop_items, exit_jump);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = name_tuple->tuple.items[i];
		Ir iterator_load = ir_new_load_local(ctx, name->site, iterator_local);
		Ir value = iterator_load;
		if (i != 0)
		{
			Ir offset = ir_new_int(ctx, name->site, i);
			value = ir_new_binary(ctx, name->site, IR_ADD, iterator_load, offset);
		}

		if (collection_local)
		{
			Ir collection = ir_new_load_local(ctx, name->site, collection_local);
			value = ir_new_index(ctx, name->site, collection, value);
		}

		Ir var_dest = ir_new_load_local(ctx, name->site, var_locals[i]);
		push_ir(&loop_items, ir_new_store(ctx, name->site, var_dest, value));
	}

	Ir iterator_load_for_add = ir_new_load_local(ctx, first_name->site, iterator_local);
	Ir stride = ir_new_int(ctx, first_name->site, nvars);
	Ir next_value = ir_new_binary(ctx, first_name->site, IR_ADD, iterator_load_for_add, stride);
	Ir iterator_dest = ir_new_load_local(ctx, first_name->site, iterator_local);
	Ir increment = ir_new_store(ctx, first_name->site, iterator_dest, next_value);

	push_ir(&loop_items, body);
	push_ir(&loop_items, continue_label);
	push_ir(&loop_items, increment);
	push_ir(&loop_items, ir_new_jump(ctx, first_name->site, start_label));
	push_ir(&loop_items, break_label);
	IrArray loop_stats = close_ir_frame(&loop_items);
	Ir loop = ir_new_block(ctx, stat->site, loop_stats);

	IrFrame block_items = begin_ir_frame(ctx);
	if (collection_local) {
		push_ir(&block_items, collection_local);
	}
	push_ir(&block_items, iterator_local);
	push_ir(&block_items, range_end_local);
	for (u32 i = 0; i < nvars; ++ i)
	{
		push_ir(&block_items, var_locals[i]);
	}
	push_ir(&block_items, loop);

	IrArray stats = close_ir_frame(&block_items);
	Ir ir = ir_new_block(ctx, stat->site, stats);

	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_ast_for_range_to_ir(IrEmitter *ctx, Ast stat)
{
	Ast decl = ast_ensure_kind(stat->for_range_stat.decl, AST_DECL_STAT);
	if (ast_is_error(decl))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	Ast name_tuple = ast_ensure_kind(decl->decl.name, AST_TUPLE);
	Ast expr_tuple = ast_ensure_kind(decl->decl.expr, AST_TUPLE);
	if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid range for-loop declaration");
		return ERROR_IR;
	}

	if (name_tuple->tuple.count == 0 || expr_tuple->tuple.count != 1)
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid range for-loop arity");
		return ERROR_IR;
	}

	Ast range = expr_tuple->tuple.items[0];
	if (!range || (range->kind != AST_RANGE && range->kind != AST_RANGE_INDEX))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid range for-loop expression");
		return ERROR_IR;
	}

	u32 entity_tags = decl->decl.tags & AST_DECL_TAG_CONSTANT ? ENTITY_TAG_CONSTANT : 0;
	return lower_ast_for_range_expr_to_ir(ctx, stat, name_tuple, range, entity_tags);
}

static Ir lower_ast_for_to_ir(IrEmitter *ctx, Ast stat)
{
	Ast init = ast_ensure_kind(stat->for_stat.init, AST_DECL_STAT);
	if (ast_is_error(init))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid C-style for-loop initializer");
		return ERROR_IR;
	}

	EntityScope scope = get_entity_scope(ctx);
	IrFrame block_items = begin_ir_frame(ctx);
	ir_lower_stat(&block_items, init);

	Ir condition_label = ir_new_label(ctx, stat->site);
	Ir continue_label = ir_new_label(ctx, stat->for_stat.step->site);
	Ir break_label = ir_new_label(ctx, stat->site);

	LoopLabels loop_labels = {};
	loop_labels.continue_label = continue_label;
	loop_labels.break_label = break_label;
	loop_labels.defer_start = ctx->defer_count;
	push_loop_labels(ctx, loop_labels);
	Ir body = ir_lower_stat_as_block(ctx, stat->for_stat.body);
	pop_loop_labels(ctx);

	Ir pred = ir_emit_expr(ctx, stat->for_stat.pred);
	Ir exit_jump = ir_new_jump_if_false(ctx, stat->for_stat.pred->site, pred, break_label);
	Ir step = ir_lower_stat_as_block(ctx, stat->for_stat.step);

	IrFrame loop_items = begin_ir_frame(ctx);
	push_ir(&loop_items, condition_label);
	push_ir(&loop_items, exit_jump);
	push_ir(&loop_items, body);
	push_ir(&loop_items, continue_label);
	push_ir(&loop_items, step);
	push_ir(&loop_items, ir_new_jump(ctx, stat->site, condition_label));
	push_ir(&loop_items, break_label);
	IrArray loop_stats = close_ir_frame(&loop_items);
	push_ir(&block_items, ir_new_block(ctx, stat->site, loop_stats));

	IrArray stats = close_ir_frame(&block_items);
	Ir ir = ir_new_block(ctx, stat->site, stats);
	set_entity_scope(ctx, scope);
	return ir;
}

static Ir lower_compound_assign_to_ir(IrEmitter *ctx, IrFrame *items, Ast stat)
{
	Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
	Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
	if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
	{
		report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid compound assignment");
		return ERROR_IR;
	}

	if (dest_tuple->tuple.count != 1 || expr_tuple->tuple.count != 1)
	{
		report_lowering_error(ctx, COM_ERROR_LOWERING, stat->site, "compound assignment requires one destination and one value");
		return ERROR_IR;
	}

	IrKind op = ir_kind_from_compound_assign_ast_kind(stat->kind);
	ASSERT(op != IR_NONE);

	Ast dest_ast = dest_tuple->tuple.items[0];
	Ast expr_ast = expr_tuple->tuple.items[0];
	if (!mark_entity_assigned(ctx, dest_ast)) return ERROR_IR;

	Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
	Ir right_ir = ir_emit_expr(ctx, expr_ast);
	Ir value_ir = ir_new_binary(ctx, stat->site, op, dest_ir, right_ir);
	return ir_new_store(ctx, stat->site, dest_ir, value_ir);
}

static void ir_lower_stat(IrFrame *items, Ast stat)
{
	IrEmitter *ctx = items->ctx;

	if (!stat || ast_is_error(stat))
	{
		push_ir(items, ERROR_IR);
		return;
	}

	switch (stat->kind)
	{
		case AST_IF:
		{
			Ast pred = stat->if_stat.pred;
			Ast true_clause = stat->if_stat.true_clause;
			Ast else_clause = stat->if_stat.else_clause;

			Ir pred_ir = ir_emit_expr(ctx, pred);
			Ir true_ir = ir_lower_stat_as_block(ctx, true_clause);
			Ir else_ir = 0;
			if (else_clause) {
				else_ir = ir_lower_stat_as_block(ctx, else_clause);
			}

			Ir ir = ir_new_if(ctx, stat->site, pred_ir, true_ir, else_ir);
			push_ir(items, ir);
		}
		break;

		case AST_WHILE:
		{
			Ast pred = stat->while_stat.pred;
			Ast body = stat->while_stat.body;

			Ir start_label = ir_new_label(ctx, stat->site);
			Ir break_label = ir_new_label(ctx, stat->site);

			LoopLabels loop_labels = {};
			loop_labels.continue_label = start_label;
			loop_labels.break_label = break_label;
			loop_labels.defer_start = ctx->defer_count;
			push_loop_labels(ctx, loop_labels);
			Ir body_ir = ir_lower_stat_as_block(ctx, body);
			pop_loop_labels(ctx);

			Ir pred_ir = ir_emit_expr(ctx, pred);
			Ir jump_if_false = ir_new_jump_if_false(ctx, pred->site, pred_ir, break_label);

			IrFrame loop_items = begin_ir_frame(ctx);
			push_ir(&loop_items, start_label);
			push_ir(&loop_items, jump_if_false);
			push_ir(&loop_items, body_ir);
			push_ir(&loop_items, ir_new_jump(ctx, stat->site, start_label));
			push_ir(&loop_items, break_label);

			IrArray loop_stats = close_ir_frame(&loop_items);
			Ir ir = ir_new_block(ctx, stat->site, loop_stats);
			push_ir(items, ir);
		}
		break;

		case AST_FOR:
		{
			Ir ir = lower_ast_for_to_ir(ctx, stat);
			push_ir(items, ir);
		}
		break;

		case AST_FOR_RANGE:
		{
			Ir ir = lower_ast_for_range_to_ir(ctx, stat);
			push_ir(items, ir);
		}
		break;

		case AST_BLOCK_STAT:
		{
			push_ir(items, ir_lower_stat_as_block(ctx, stat));
		}
		break;

		case AST_STATIC_DECL:
		{
			// Static declarations are consumed before IR lowering.
		}
		break;

		case AST_DECL_STAT:
		{
			Ast name_tuple = ast_ensure_kind(stat->decl.name, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->decl.expr, AST_TUPLE);
			if (ast_is_error(name_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid declaration");
				push_ir(items, ERROR_IR);
				break;
			}

			u32 entity_tags = stat->decl.tags & AST_DECL_TAG_CONSTANT ? ENTITY_TAG_CONSTANT : 0;
			lower_local_decl_tuples_to_ir(ctx, items, name_tuple, expr_tuple, entity_tags);
		}
		break;

		case AST_ASSIGN:
		{
			Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid assignment");
				push_ir(items, ERROR_IR);
				break;
			}

			u32 nargs = dest_tuple->tuple.count;
			if (nargs == 1)
			{
				Ast dest_ast = dest_tuple->tuple.items[0];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					break;
				}
				Ast expr_ast = expr_tuple->tuple.count ? expr_tuple->tuple.items[0] : 0;

				Ir dest_ir = ir_emit_expr(ctx, dest_ast);
				Ir expr_ir = expr_ast ? ir_emit_expr(ctx, expr_ast) : ir_new_nil(ctx, dest_ast->site);
				Ir ir = ir_new_store(ctx, stat->site, dest_ir, expr_ir);
				push_ir(items, ir);
				break;
			}

			// TODO(RJ): we don't need multi-local! could be done with the same mechanism!
			Ir *values = elf_arena_push(ctx->module->arena, sizeof(*values) * nargs);
			if (expr_tuple->tuple.count == 1 && ast_is_call(expr_tuple->tuple.items[0]))
			{
				Ast expr_ast = expr_tuple->tuple.items[0];
				Ir expr_ir = ir_emit_expr(ctx, expr_ast);
				for (u32 i = 0; i < nargs; ++i) {
					values[i] = ir_new_local(ctx, expr_ast->site, 0);
				}
				push_ir(items, ir_new_multi_local(ctx, expr_ast->site, expr_ir, (IrArray){ .items = values, .count = nargs }));
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
						expr_ir = ir_emit_expr(ctx, expr_ast);
					}
					else {
						expr_ir = ir_new_nil(ctx, site);
					}

					values[i] = ir_new_local(ctx, site, expr_ir);
					push_ir(items, values[i]);
				}
			}

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ast dest_ast = dest_tuple->tuple.items[i];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					continue;
				}
				Ir dest_ir = ir_emit_expr(ctx, dest_ast);
				Ir value_ir = ir_new_load_local(ctx, values[i]->site, values[i]);
				Ir ir = ir_new_store(ctx, stat->site, dest_ir, value_ir);
				push_ir(items, ir);
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
			Ir ir = lower_compound_assign_to_ir(ctx, items, stat);
			push_ir(items, ir);
		}
		break;

		case AST_NIL_ASSIGN:
		{
			Ast dest_tuple = ast_ensure_kind(stat->binary.x, AST_TUPLE);
			Ast expr_tuple = ast_ensure_kind(stat->binary.y, AST_TUPLE);
			if (ast_is_error(dest_tuple) || ast_is_error(expr_tuple))
			{
				report_lowering_error(ctx, COM_ERROR_INTERNAL, stat->site, "invalid nil assignment");
				push_ir(items, ERROR_IR);
				break;
			}

			for (u32 i = 0; i < dest_tuple->tuple.count; ++ i)
			{
				if (i >= expr_tuple->tuple.count) {
					ASSERT(!"Error");
				}

				Ast dest_ast = dest_tuple->tuple.items[i];
				Ast expr_ast = expr_tuple->tuple.items[i];
				if (!mark_entity_assigned(ctx, dest_ast)) {
					push_ir(items, ERROR_IR);
					continue;
				}

				Ir dest_ir = lower_ast_lvalue_to_ir_once(ctx, items, dest_ast);
				Ir nil_ir = ir_new_nil(ctx, dest_ast->site);
				Ir pred_ir = ir_new_binary(ctx, stat->site, IR_EQ, dest_ir, nil_ir);
				Ir value_ir = ir_emit_expr(ctx, expr_ast);
				Ir store_ir = ir_new_store(ctx, stat->site, dest_ir, value_ir);

				IrFrame true_items = begin_ir_frame(ctx);
				push_ir(&true_items, store_ir);
				IrArray true_stats = close_ir_frame(&true_items);
				Ir true_clause = ir_new_block(ctx, stat->site, true_stats);
				Ir ir = ir_new_if(ctx, stat->site, pred_ir, true_clause, 0);
				push_ir(items, ir);
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
				expr_ir = ir_emit_expr(ctx, expr);
			}
			if (expr_ir && ctx->defer_count > 0)
			{
				Ir return_value = ir_new_local(ctx, stat->site, expr_ir);
				push_ir(items, return_value);
				expr_ir = ir_new_load_local(ctx, stat->site, return_value);
			}
			emit_defer_range(ctx, items, ctx->function_defer_start);
			Ir ir = ir_new_return(ctx, stat->site, expr_ir);
			push_ir(items, ir);
		}
		break;

		case AST_BREAK:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = ir_new_jump(ctx, stat->site, loop_labels.break_label);
			push_ir(items, ir);
		}
		break;

		case AST_CONTINUE:
		{
			LoopLabels loop_labels = current_loop_labels(ctx, stat->site);
			emit_defer_range(ctx, items, loop_labels.defer_start);
			Ir ir = ir_new_jump(ctx, stat->site, loop_labels.continue_label);
			push_ir(items, ir);
		}
		break;

		case AST_DEFER_STAT:
		{
			push_defer_stat(ctx, stat->defer_stat.body);
		}
		break;

		case AST_TUPLE:
		{
			Ast *args = stat->tuple.items;
			u32 nargs = stat->tuple.count;

			for (u32 i = 0; i < nargs; ++ i)
			{
				Ir ir = ir_emit_expr(ctx, args[i]);
				push_ir(items, ir);
			}
		}
		break;

		default:
		{
			Ir ir = ir_emit_expr(ctx, stat);
			push_ir(items, ir);
		}
		break;
	}
}

static u32 ir_lower_function(IrEmitter *emitter, SourceSite site, b32 variadic, Ast_Array ast_params, Ast body)
{
	ASSERT(emitter->module->function_count < emitter->module->function_capacity);

	u32 index = emitter->module->function_count ++;

	IrFunDef *definition = &emitter->module->functions[index];
	definition->site = site;
	definition->variadic = variadic;
	definition->param_count = IMPLICIT_PARAM_COUNT + ast_params.count;

	EntityScope function_scope = get_entity_scope(emitter);
	LwScopeFunction function_ctx = {};
	function_ctx.parent = emitter->function;
	function_ctx.function = definition;
	function_ctx.scope_start = emitter->scope_start;

	LwScopeFunction *outer_function = emitter->function;
	u32 outer_defer_count = emitter->defer_count;
	u32 outer_defer_scope_start = emitter->defer_scope_start;
	u32 outer_function_defer_start = emitter->function_defer_start;
	u32 outer_loop_count = emitter->loop_count;
	emitter->function = &function_ctx;
	emitter->defer_scope_start = outer_defer_count;
	emitter->function_defer_start = outer_defer_count;
	emitter->loop_count = 0;

	Ir *ir_params = elf_arena_push_zero(emitter->module->arena, sizeof(*ir_params) * (IMPLICIT_PARAM_COUNT + ast_params.count));
	ir_params[IMPLICIT_PARAM_INDEX] = ir_new_local(emitter, site, 0);
	definition->params = ir_params;

	for (u32 i = 0; i < ast_params.count; ++ i)
	{
		Ast param = ast_ensure_kind(ast_params.items[i], AST_FUNCTION_PARAM);
		if (ast_is_error(param)) {
			report_lowering_error(emitter, COM_ERROR_INTERNAL, param->site, "expected function parameter");
			continue;
		}

		Ast name = ast_ensure_kind(param->param.name, AST_IDENT);
		if (ast_is_error(name)) {
			report_lowering_error(emitter, COM_ERROR_INTERNAL, param->site, "expected function parameter name");
			continue;
		}

		Ir param_memory = ir_new_local(emitter, name->site, 0);
		ir_params[IMPLICIT_PARAM_COUNT + i] = param_memory;

		Ast type = param->param.type;
		if (type)
		{
			type = ast_ensure_kind(type, AST_IDENT);
			if (ast_is_error(type))
			{
				report_lowering_error(emitter, COM_ERROR_INTERNAL, param->site, "expected function parameter type name");
			}
			else
			{
				TypeRule rule = type_rule_from_atom(type->atom);
				if (rule == TRULE_NONE)
				{
					report_lowering_error(emitter, COM_ERROR_LOWERING, type->site, "'%s' is not a known type name", atom_data(type->atom));
				}
				else
				{
					param_memory->type_rule = rule;
				}
			}
		}

		IrEntity *entity = declare_local_entity(emitter, name->site, ENTITY_TAG_PARAMETER, name->atom);
		entity->memory_ir = param_memory;
	}

	definition->body = ir_lower_stat_as_block(emitter, body);

	IrArray captures = {};
	captures.count = function_ctx.capture_count;
	if (captures.count) {
		captures.items = elf_arena_push(emitter->module->arena, sizeof(*captures.items) * captures.count);
	}
	u32 capture_index = 0;
	for (LwCapture *capture = function_ctx.first_capture; capture; capture = capture->next)
	{
		ASSERT(capture_index < captures.count);
		captures.items[capture_index++] = capture->source;
	}
	ASSERT(capture_index == captures.count);


	emitter->function = outer_function;
	emitter->loop_count = outer_loop_count;
	emitter->defer_count = outer_defer_count;
	emitter->defer_scope_start = outer_defer_scope_start;
	emitter->function_defer_start = outer_function_defer_start;

	definition->capture_sources = captures;

	set_entity_scope(emitter, function_scope);

	return index;
}

IrModule *ir_emit_module(Compiler *compiler, Checker *checker, Ast ast)
{
	ASSERT(compiler);
	ASSERT(compiler->arena);
	ASSERT(ast);
	ASSERT(ast->kind == AST_FILE);

	IrModule *module = dy_arena_push_zero(compiler->arena, sizeof(*module), 8);
	module->arena = compiler->arena;
	module->function_capacity = 4096;
	module->functions = elf_arena_push_zero(module->arena, sizeof(*module->functions) * module->function_capacity);
	module->function_count = 0;

	IrEmitter *emitter = elf_arena_push_zero(module->arena, sizeof(*emitter));
	emitter->checker = checker;
	emitter->compiler = compiler;
	emitter->module = module;
	emitter->ir_stack_size = 4096;
	emitter->ir_stack = elf_arena_push_zero(module->arena, sizeof(*emitter->ir_stack) * emitter->ir_stack_size);
	emitter->defer_stack_size = 4096;
	emitter->defer_stack = elf_arena_push_zero(module->arena, sizeof(*emitter->defer_stack) * emitter->defer_stack_size);

	u32 index = ir_lower_function(emitter, ast->site, 1, (Ast_Array){0}, ast->file.body);
	ASSERT(index == 0);
	ASSERT(!module->functions[index].capture_sources.items && !module->functions[index].capture_sources.count);

	ASSERT(emitter->scope_end == 0);
	ASSERT(emitter->scope_start == 0);
	ASSERT(emitter->scope_depth == 0);
	return module;
}
