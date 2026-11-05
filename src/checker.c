//
// See Copyright Notice In elf.h
//

static Entity entity_error_sentinel =
{
	.status = COM_STATUS_REPORTED,
};

static Entity entity_oom_sentinel =
{
	.status = COM_STATUS_OOM,
};

static b32 entity_is_constant(const Entity *entity)
{
	ASSERT(entity);
	ASSERT(!entity_is_error(entity));
	return (entity->tags & ENTITY_TAG_CONSTANT) != 0;
}

static b32 entity_is_exported(const Entity *entity)
{
	ASSERT(entity);
	return (entity->tags & ENTITY_TAG_EXPORTED) != 0;
}

static Scope *scope_create(Arena *arena, Scope *parent, struct Ast_T *owner, Com_Source *source)
{
	ASSERT(!parent || !parent->source || parent->source == source);
	Scope *scope = arena_push_zero(arena, sizeof(*scope), _Alignof(Scope));
	if (scope)
	{
		scope->arena = arena;
		scope->parent = parent;
		scope->owner = owner;
		scope->source = source;
	}
	return scope;
}

// Todo, not sure if we should really consider buckets for entities with the same name ...
static Entity *scope_find_current(Scope *scope, Atom *name)
{
	for (u32 i = 0; i < scope->count; ++i)
	{
		Entity *entity = scope->entities[i];

		if (entity->name == name) {
			ASSERT(entity->count >= 1);
			ASSERT(entity->count != 1 || (entity->first == entity && entity->last == entity));
			return entity;
		}
	}
	return 0;
}

static Entity *scope_find(Scope *scope, Atom *name)
{
	for (Scope *current = scope; current; current = current->parent)
	{
		Entity *entity = scope_find_current(current, name);
		if (entity) {
			return entity;
		}
	}
	return 0;
}

static Entity *scope_declare(Scope *scope, Atom *name, Entity_Tags tags)
{
	Entity *anchor = scope_find_current(scope, name);
	ASSERT(!anchor || (anchor->count >= 1 && anchor->first == anchor));

	if (scope->count == scope->capacity)
	{
		u32 capacity = scope->capacity ? scope->capacity * 2 : 16;
		Entity **entities = arena_push(scope->arena, sizeof(*entities) * capacity, _Alignof(Entity *));
		if (!entities) {
			return 0;
		}
		if (scope->count) {
			copy_memory(entities, scope->entities, sizeof(*entities) * scope->count);
		}
		scope->entities = entities;
		scope->capacity = capacity;
	}

	Entity *entity = arena_push_zero(scope->arena, sizeof(*entity), _Alignof(Entity));

	if (!entity) {
		return 0;
	}

	if (!anchor) {
		anchor = entity;
	}
	anchor->count ++;
	entity->next = anchor->last;
	anchor->last = entity;

	entity->name = name;
	entity->scope = scope;
	entity->tags = tags;
	entity->first = entity;
	entity->last  = entity;

	scope->entities[scope->count ++] = entity;
	return entity;
}

static Ir eval_static(elf_State *state, IrModule *module, Com_Source *source, Ir ir);
static Com_Status check(IrModule *module, Compiler *compiler);
static Com_Status eval_statics(IrModule *module, Compiler *compiler);
static Com_Status check_stat(IrEmitter *emitter, IrQueue *items, Ast ast);
static Com_Value check_expr(IrEmitter *emitter, Ast expr);
static Ir check_ir(IrEmitter *emitter, Ast expr);
static Ir check_block(IrEmitter *emitter, Ast stat);
static Ir check_for(IrEmitter *emitter, Ast stat);
static Ir check_range_for(IrEmitter *emitter, Ast stat);
static Com_Status prepare_func(IrModule *mod, Com_Source *source, Scope *parent_scope, IrFunction *func);
static Com_Status prepare_ast(IrModule *mod, Com_Source *source, Scope *scope, IrFunction *func, Ast ast);
static Com_Status prepare_ast_array(IrModule *mod, Com_Source *source, Scope *scope, IrFunction *func, Ast_Array array);
static Com_Status prepare_scope(Com_Source *source, Scope *scope, Ast_Array stats);
static Com_Status check_function(IrEmitter *emitter, IrFunction *function);

static Com_Value com_value_error(Com_Status status)
{
	ASSERT(status != COM_STATUS_OK);
	Com_Value value = {.type = COM_VALUE_ERROR, .status = status};
	return value;
}

static Com_Status com_value_status(Com_Value value)
{
	return value.type == COM_VALUE_ERROR ? value.status : COM_STATUS_OK;
}

static Com_Value com_value_ir(Ir ir)
{
	ASSERT(ir);
	if (ir_is_error(ir)) {
		return com_value_error(ir_status(ir));
	}
	return (Com_Value) { .type = COM_VALUE_IR, .ir = ir };
}

static Ir ir_from_value(IrEmitter *emitter, SourceSite site, elf_Value value)
{
	switch (value.type)
	{
		case ELF_VALUE_TYPE_NIL: {
			return ir_new_nil(emitter->module, site);
		}
		case ELF_VALUE_TYPE_INTEGER: {
			return ir_new_int(emitter->module, site, value_as_integer(value));
		}
		case ELF_VALUE_TYPE_NUMBER: {
			return ir_new_num(emitter->module, site, value_as_number(value));
		}
		case ELF_VALUE_TYPE_STRING:
		{
			elf_String *string = value_as_string(value);
			Atom *atom = atom_from_data_size(emitter->source->compiler->atoms, elf_string_data(string), elf_string_size(string));
			return atom ? ir_new_atom(emitter->module, site, atom) : ERROR_OOM_IR;
		}
		case ELF_VALUE_TYPE_TABLE: {
			return ir_new_tab_ref(emitter->module, site, value_as_table(value));
		}
		case ELF_VALUE_TYPE_ARRAY: {
			return ir_new_arr_ref(emitter->module, site, value_as_array(value));
		}
		default: {
			ASSERT(!"unsupported static result type");
			return ERROR_IR;
		}
	}
}

static Com_Status checker_report(Com_Source *source, elf_ErrorCode error, ErrorSeverity severity, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 reported = reportv(source, error, severity, ERROR_PHASE_CHECKER, site, format, args);
	va_end(args);
	return reported ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Com_Status checker_warn(Com_Source *source, elf_ErrorCode warning, SourceSite site, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	b32 reported = reportv(source, warning, ERROR_WARNING, ERROR_PHASE_CHECKER, site, format, args);
	va_end(args);
	return reported ? COM_STATUS_OK : COM_STATUS_OOM;
}

// TODO(RJ): why the fuck are we routing out of memory errors to the fucking diagnostics system!
static Com_Status checker_out_of_memory(Com_Source *source, SourceSite site)
{
	return report(source, ELF_ERROR_OUT_OF_MEMORY, ERROR_FATAL, ERROR_PHASE_CHECKER, site, "out of memory during semantic analysis") ? COM_STATUS_REPORTED : COM_STATUS_OOM;
}

static Scope *push_scope(Com_Source *source, Scope *parent, Ast owner)
{
	ASSERT(source);
	ASSERT(parent);
	ASSERT(owner);
	ASSERT(!parent->source || parent->source == source);
	Scope *scope = scope_create(parent->arena, parent, owner, source);
	if (!scope)
	{
		checker_out_of_memory(source, owner->site);
		return 0;
	}
	ASSERT(!owner->scope);
	owner->scope = scope;
	return scope;
}

static Scope *enter_scope(IrEmitter *emitter, Ast owner)
{
	ASSERT(emitter);
	ASSERT(emitter->active_scope);
	ASSERT(owner);
	ASSERT(owner->scope);
	ASSERT(owner->scope->parent == emitter->active_scope);
	emitter->active_scope = owner->scope;
	return owner->scope;
}

static void pop_scope(IrEmitter *emitter, Scope *scope)
{
	ASSERT(scope);
	ASSERT(emitter->active_scope == scope);
	emitter->active_scope = scope->parent;
}

static Entity *declare_entity(Scope *scope, SourceSite site, Atom *name, Entity_Tags tags, Com_Value value)
{
	ASSERT(name);
	ASSERT(scope);
	ASSERT(value.type != COM_VALUE_NONE);

	Entity *entity = scope_declare(scope, name, tags);
	if (!entity) {
		// TODO(RJ): why the fuck are we routing OOM errors to the diagnostics reporting path?
		return entity_from_status(checker_out_of_memory(scope->source, site));
	}

	if (entity->count != 1)
	{
		Com_Status status = checker_report(scope->source, ELF_ERROR_DUPLICATE_DECLARATION, ERROR_ERROR, site, "'%s' is already declared in this scope", atom_data(name));
		return entity_from_status(status);
	}

	if (scope->parent && scope_find(scope->parent, name))
	{
		Com_Status status = checker_warn(scope->source, ELF_ERROR_SHADOWED_ENTITY, site, "declaration of '%s' shadows an outer declaration", atom_data(name));
		if (status != COM_STATUS_OK) {
			return entity_from_status(status);
		}
	}

	entity->value = value;
	Compiler *compiler = scope->source->compiler;
	entity->next_in_compiler = compiler->entities;
	compiler->entities = entity;
	return entity;
}

static Ir ir_emit_capture(IrEmitter *emitter, IrFunction *function, Entity *entity, SourceSite site)
{
	ASSERT(entity->value.type == COM_VALUE_IR);
	Ir local = entity->value.ir;
	ASSERT(local->kind == IR_LOCAL);
	ASSERT(local->ir_local.owner);
	ASSERT(function);
	ASSERT(function->parent || !function->ncaps);

	if (local->ir_local.owner == function) {
		return ir_new_load(emitter->module, site, local);
	}

	u32 capture = 0;
	while (capture < function->ncaps && function->caps[capture].local != local) {
		capture += 1;
	}
	if (capture == function->ncaps)
	{
		ASSERT(function->ncaps < 255);
		IrFunction *parent = function->parent;
		if (!parent)
		{
			ir_report_error(emitter, ELF_ERROR_LOWERING, site, "local '%s' cannot be captured from an unrelated function", atom_data(entity->name));
			return ERROR_IR;
		}

		Ir target = ir_emit_capture(emitter, parent, entity, site);
		if (ir_is_error(target)) {
			return ERROR_IR;
		}

		if (function->ncaps == function->xcaps)
		{
			u32 cap = function->xcaps ? function->xcaps * 2 : 8;
			IrCapture *caps = arena_push(emitter->module->arena, sizeof(*caps) * cap, _Alignof(IrCapture));
			if (function->ncaps) {
				copy_memory(caps, function->caps, sizeof(*caps) * function->ncaps);
			}
			function->caps = caps;
			function->xcaps = cap;
		}
		function->caps[function->ncaps++] = (IrCapture) { .local = local, .target = target };
	}

	return ir_new_capture_ref(emitter->module, site, capture);
}

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
		default:                   return IR_NONE;
	}
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

static Com_Value check_entity(IrEmitter *checker, SourceSite site, Entity *entity)
{
	ASSERT(checker);
	ASSERT(entity);
	ASSERT(entity->scope);

	Com_Value value = entity->value;
	if ((entity->tags & ENTITY_TAG_STATIC) && value.type == COM_VALUE_RESOLVING)
	{
		ASSERT(entity->declaration);
		ASSERT(entity->scope->source);
		Com_Status status = checker_report(entity->scope->source, ELF_ERROR_CYCLIC_STATIC_DEFINITION, ERROR_ERROR, entity->declaration->site, "cyclic static definition involving '%s'", atom_data(entity->name));
		value = com_value_error(status);
		entity->value = value;
	}
	else if ((entity->tags & ENTITY_TAG_STATIC) && value.type == COM_VALUE_AST)
	{
		ASSERT(entity->declaration);
		ASSERT(entity->scope->source);
		Ast expr = value.ast;
		ASSERT(expr);

		entity->value.type = COM_VALUE_RESOLVING;
		value = check_expr(checker, expr);
		if (value.type == COM_VALUE_NONE)
		{
			Com_Status status = checker_report(entity->scope->source, ELF_ERROR_INVALID_STATIC_EXPRESSION, ERROR_ERROR, expr->site, "static definition '%s' does not resolve to a static value", atom_data(entity->name));
			value = com_value_error(status);
		}
		else if (value.type == COM_VALUE_IR)
		{
			Com_Source *source = entity->scope->source;
			Ir ir = eval_static(source->compiler->target, checker->module, source, value.ir);
			if (ir == ERROR_OOM_IR) {
				value = com_value_error(checker_out_of_memory(source, expr->site));
			}
			else {
				value = com_value_ir(ir);
			}
		}
		entity->value = value;
	}
	if (value.type == COM_VALUE_ERROR) {
		return value;
	}
	if (value.type == COM_VALUE_SCOPE || value.type == COM_VALUE_EXPORTS || value.type == COM_VALUE_TYPE || value.type == COM_VALUE_TYPE_RULE) {
		return value;
	}

	Ir ir = 0;
	switch (value.type)
	{
		case COM_VALUE_IR:
		{
			ir = value.ir->kind == IR_LOCAL ? ir_emit_capture(checker, checker->function, entity, site) : value.ir;
		}
		break;

		case COM_VALUE_NUMBER: {
			ir = ir_new_num(checker->module, site, value.number);
		}
		break;

		case COM_VALUE_INTEGER: {
			ir = ir_new_int(checker->module, site, value.integer);
		}
		break;

		case COM_VALUE_STRING: {
			ir = ir_new_atom(checker->module, site, value.string);
		}
		break;

		case COM_VALUE_NATIVE_FUNCTION: {
			ir = ir_new_native_function(checker->module, site, value.native_function_index);
		}
		break;

		case COM_VALUE_ERROR: {
			ir = ERROR_IR;
		}
		break;

		case COM_VALUE_NONE:
		case COM_VALUE_AST:
		case COM_VALUE_RESOLVING:
		default:
		{
			ir_report_error(checker, ELF_ERROR_INTERNAL, site, "static entity '%s' has invalid value type '%s'", atom_data(entity->name), com_value_type_name(value.type));
			ir = ERROR_IR;
		}
		break;
	}

	if (ir_is_error(ir)) {
		return com_value_error(ir_status(ir));
	}
	return com_value_ir(ir);
}

static Ir check_ir(IrEmitter *checker, Ast expr)
{
	Com_Value value = check_expr(checker, expr);
	if (value.type == COM_VALUE_ERROR) {
		return ir_from_status(value.status);
	}
	if (value.type != COM_VALUE_IR)
	{
		ir_report_error(checker, ELF_ERROR_LOWERING, expr->site, "compiler value '%s' cannot be used as a runtime value", com_value_type_name(value.type));
		return ERROR_IR;
	}
	return value.ir;
}

static Com_Value check_expr(IrEmitter *checker, Ast expr)
{
	ASSERT(expr);
	IrEmitter *emitter = checker;
	Com_Value value = {COM_VALUE_NONE};

	switch (expr->kind)
	{
		case AST_NONE:
		case AST_ELLIPSIS:
		{
		}
		break;

		case AST_ERROR:
		{
			value = com_value_error(ast_status(expr));
		}
		break;

		case AST_RECURSE:
		{
			ASSERT(checker->function);
			value = com_value_ir(ir_new_recurse(emitter->module, expr->site));
		}
		break;

		case AST_NIL_LITERAL:
		{
			value = com_value_ir(ir_new_nil(emitter->module, expr->site));
		}
		break;

		case AST_INTEGER_LITERAL:
		{
			value = com_value_ir(ir_new_int(emitter->module, expr->site, expr->integer_value));
		}
		break;

		case AST_NUMBER_LITERAL:
		{
			value = com_value_ir(ir_new_num(emitter->module, expr->site, expr->number_value));
		}
		break;

		case AST_STRING_LITERAL:
		{
			value = com_value_ir(ir_new_atom(emitter->module, expr->site, expr->string_value));
		}
		break;

		case AST_FUNCTION:
		{
			IrFunction *function = expr->function.ir;
			ASSERT(function);
			Com_Status status = check_function(checker, function);
			if (status != COM_STATUS_OK) {
				value = com_value_error(status);
				break;
			}
			value = com_value_ir(ir_new_function_ref(emitter->module, expr->site, function));
		}
		break;

		case AST_IMPORT:
		{
			ASSERT(expr->import.target);
			ASSERT(expr->import.target->state >= COM_SOURCE_PREPARED);
			ASSERT(expr->import.target->scope);
			value.type = COM_VALUE_EXPORTS;
			value.exports = expr->import.target->scope;
		}
		break;

		case AST_IDENT:
		{
			Atom *name = expr->identifier;
			Entity *entity = expr->entity;
			if (!entity) {
				entity = scope_find(checker->active_scope, name);
			}
			if (!entity)
			{
				Com_Status status = checker_report(checker->source, ELF_ERROR_UNDECLARED_IDENTIFIER, ERROR_ERROR, expr->site, "undeclared identifier '%s'", atom_data(name));
				value = com_value_error(status);
				break;
			}
			expr->entity = entity;
			entity->referenced = true;
			value = check_entity(checker, expr->site, entity);
		}
		break;

		case AST_FIELD:
		{
			ASSERT(expr->binary.x);
			ASSERT(expr->binary.y);

			Com_Value left = check_expr(checker, expr->binary.x);
			if (left.type == COM_VALUE_ERROR) {
				value = left;
				break;
			}

			if (left.type == COM_VALUE_SCOPE)
			{
				ASSERT(expr->binary.y->kind == AST_IDENT || expr->binary.y->kind == AST_STRING_LITERAL);
				ASSERT(expr->binary.y->atom);
				Entity *entity = scope_find_current(left.scope, expr->binary.y->atom);
				if (!entity)
				{
					Com_Status status = checker_report(checker->source, ELF_ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "host namespace has no member named '%s'", atom_data(expr->binary.y->atom));
					value = com_value_error(status);
					break;
				}
				ASSERT(entity->value.type != COM_VALUE_IR || entity->value.ir->kind != IR_LOCAL);
				expr->entity = entity;
				entity->referenced = true;
				value = check_entity(checker, expr->site, entity);
				break;
			}

			if (left.type == COM_VALUE_EXPORTS)
			{
				ASSERT(expr->binary.y->kind == AST_IDENT || expr->binary.y->kind == AST_STRING_LITERAL);
				ASSERT(expr->binary.y->atom);
				Entity *entity = scope_find_current(left.exports, expr->binary.y->atom);
				if (!entity)
				{
					Com_Status status = checker_report(checker->source, ELF_ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "source '%s' has no static declaration named '%s'", atom_data(left.exports->source->name), atom_data(expr->binary.y->atom));
					value = com_value_error(status);
					break;
				}
				if (!entity_is_exported(entity))
				{
					Com_Status status = checker_report(checker->source, ELF_ERROR_STATIC_LOOKUP, ERROR_ERROR, expr->binary.y->site, "static declaration '%s' is private to source '%s'", atom_data(entity->name), atom_data(left.exports->source->name));
					value = com_value_error(status);
					break;
				}
				expr->entity = entity;
				entity->referenced = true;
				value = check_entity(checker, expr->site, entity);
				break;
			}

			ASSERT(left.type == COM_VALUE_IR);
			Com_Value right = check_expr(checker, expr->binary.y);
			if (right.type == COM_VALUE_ERROR) {
				value = right;
				break;
			}
			ASSERT(right.type == COM_VALUE_IR);
			value = com_value_ir(ir_new_binary(emitter->module, expr->site, IR_FIELD, left.ir, right.ir));
		}
		break;

		case AST_BITWISE_NOT:
		{
			Ir operand = check_ir(checker, expr->unary);
			value = ir_is_error(operand) ? com_value_error(ir_status(operand)) : com_value_ir(ir_new_unary(emitter->module, expr->site, IR_BITWISE_NOT, operand));
		}
		break;

		case AST_MEASURE:
		{
			Ir operand = check_ir(checker, expr->unary);
			value = ir_is_error(operand) ? com_value_error(ir_status(operand)) : com_value_ir(ir_new_measure(emitter->module, expr->site, operand));
		}
		break;

		case AST_GET_MEM:
		{
			Ir operand = check_ir(checker, expr->unary);
			if (ir_is_error(operand)) {
				value = com_value_error(ir_status(operand));
			}
			else if (operand->kind != IR_LOAD)
			{
				ir_report_error(emitter, ELF_ERROR_LOWERING, expr->site, "#get_mem expects a local or parameter expression");
				value = com_value_error(COM_STATUS_REPORTED);
			}
			else {
				value = com_value_ir(ir_new_get_mem(emitter->module, expr->site, operand));
			}
		}
		break;

		case AST_META_FIELD:
		case AST_INDEX:
		case AST_GREATER_THAN:
		case AST_GREATER_THAN_EQ:
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
			Ir x = check_ir(checker, expr->binary.x);
			if (ir_is_error(x)) {
				value = com_value_error(ir_status(x));
				break;
			}
			Ir y = check_ir(checker, expr->binary.y);
			if (ir_is_error(y)) {
				value = com_value_error(ir_status(y));
				break;
			}

			if (expr->kind == AST_META_FIELD) {
				value = com_value_ir(ir_new_binary(emitter->module, expr->site, IR_META_FIELD, x, y));
			}
			else if (expr->kind == AST_INDEX) {
				value = com_value_ir(ir_new_index(emitter->module, expr->site, x, y));
			}
			else if (expr->kind == AST_GREATER_THAN) {
				value = com_value_ir(ir_new_binary(emitter->module, expr->site, IR_LESS_THAN, y, x));
			}
			else if (expr->kind == AST_GREATER_THAN_EQ) {
				value = com_value_ir(ir_new_binary(emitter->module, expr->site, IR_LESS_THAN_EQ, y, x));
			}
			else {
				value = com_value_ir(ir_new_binary(emitter->module, expr->site, ir_kind_from_ast_kind(expr->kind), x, y));
			}
		}
		break;

		case AST_NIL_AND:
		case AST_COMMA_EXPR:
		case AST_RANGE_INDEX:
		{
			Com_Value operand = check_expr(checker, expr->binary.x);
			if (operand.type != COM_VALUE_ERROR) {
				operand = check_expr(checker, expr->binary.y);
			}
			if (operand.type == COM_VALUE_ERROR) {
				value = operand;
			}
		}
		break;

		case AST_RANGE:
		{
			if (expr->binary.x)
			{
				value = check_expr(checker, expr->binary.x);
				if (value.type == COM_VALUE_ERROR) {
					break;
				}
			}
			if (expr->binary.y) {
				value = check_expr(checker, expr->binary.y);
			}
			if (value.type != COM_VALUE_ERROR) {
				value = (Com_Value) {COM_VALUE_NONE};
			}
		}
		break;

		case AST_INTERPOLATED_STRING:
		{
			if (!expr->interpolated_string.count)
			{
				ir_report_error(emitter, ELF_ERROR_INTERNAL, expr->site, "interpolated string has no parts");
				value = com_value_error(COM_STATUS_REPORTED);
				break;
			}

			Ir ir = 0;
			for (u32 i = 0; i < expr->interpolated_string.count; ++ i)
			{
				Ast part = expr->interpolated_string.items[i];
				if (!part)
				{
					ir_report_error(emitter, ELF_ERROR_INTERNAL, expr->site, "interpolated string part %u is missing", i);
					value = com_value_error(COM_STATUS_REPORTED);
					break;
				}
				Ir part_ir = check_ir(checker, part);
				if (ir_is_error(part_ir))
				{
					value = com_value_error(ir_status(part_ir));
					break;
				}
				ir = ir ? ir_new_binary(emitter->module, part->site, IR_ADD, ir, part_ir) : part_ir;
			}
			if (value.type != COM_VALUE_ERROR) {
				value = com_value_ir(ir);
			}
		}
		break;

		case AST_CALL:
		{
			Com_Value target = check_expr(checker, expr->call.expr);
			if (target.type == COM_VALUE_ERROR) {
				value = target;
				break;
			}

			ASSERT(target.type == COM_VALUE_IR);
			Ir target_ir = target.ir;
			Ast_Array args = expr->call.args;
			IrArray ir_args = ir_array_alloc(emitter, args.count);

			for (u32 i = 0; i < args.count; ++ i)
			{
				Ast argument = args.items[i];
				ASSERT(argument);
				if (argument->kind == AST_NAMED_ARGUMENT)
				{
					Com_Status status = checker_report(checker->source, ELF_ERROR_NOT_IMPLEMENTED, ERROR_ERROR, argument->site, "named arguments are not supported");
					value = com_value_error(status);
					break;
				}

				ir_args.items[i] = check_ir(checker, argument);
				if (ir_is_error(ir_args.items[i]))
				{
					value = com_value_error(ir_status(ir_args.items[i]));
					break;
				}
			}

			if (value.type == COM_VALUE_ERROR) {
				break;
			}
			value = com_value_ir(ir_new_call(emitter->module, expr->site, target_ir, ir_args));
		}
		break;

		case AST_TUPLE:
		{
			IrArray items = ir_array_alloc(emitter, expr->tuple.count);
			for (u32 i = 0; i < expr->tuple.count; ++ i)
			{
				ASSERT(expr->tuple.items[i]);
				items.items[i] = check_ir(checker, expr->tuple.items[i]);
				if (ir_is_error(items.items[i]))
				{
					value = com_value_error(ir_status(items.items[i]));
					break;
				}
			}
			if (value.type != COM_VALUE_ERROR) {
				value = com_value_ir(ir_new_tuple(emitter->module, expr->site, items));
			}
		}
		break;

		case AST_TABLE:
		{
			IrQueue items = ir_queue_begin(emitter);
			Ir local = ir_new_local(emitter->module, emitter->function, expr->site, ir_new_tab(emitter->module, expr->site));
			ir_queue_push(&items, local);

			for (u32 i = 0; i < expr->table.count; ++ i)
			{
				Ast entry = expr->table.items[i];
				ASSERT(entry);
				ASSERT(entry->kind == AST_TABLE_ENTRY);
				ASSERT(entry->table_entry.key);
				Ir item = check_ir(checker, entry->table_entry.value);
				if (ir_is_error(item))
				{
					value = com_value_error(ir_status(item));
					break;
				}
				Ir key = check_ir(checker, entry->table_entry.key);
				if (ir_is_error(key))
				{
					value = com_value_error(ir_status(key));
					break;
				}
				Ir table = ir_new_load(emitter->module, entry->site, local);
				Ir field = ir_new_binary(emitter->module, entry->site, IR_FIELD, table, key);
				ir_queue_push(&items, ir_new_store(emitter->module, entry->site, field, item));
			}

			IrArray stats = ir_queue_end(&items);
			if (value.type != COM_VALUE_ERROR)
			{
				Ir result = ir_new_load(emitter->module, expr->site, local);
				value = com_value_ir(ir_new_expr_block(emitter->module, expr->site, stats, result));
			}
		}
		break;

		case AST_ARRAY:
		{
			IrQueue items = ir_queue_begin(emitter);
			Ir local = ir_new_local(emitter->module, emitter->function, expr->site, ir_new_arr(emitter->module, expr->site));
			ir_queue_push(&items, local);
			for (u32 i = 0; i < expr->array.count; ++ i)
			{
				Ir item = check_ir(checker, expr->array.items[i]);
				if (ir_is_error(item))
				{
					value = com_value_error(ir_status(item));
					break;
				}
				Ir array = ir_new_load(emitter->module, expr->site, local);
				ir_queue_push(&items, ir_new_array_add(emitter->module, expr->array.items[i]->site, array, item));
			}

			IrArray stats = ir_queue_end(&items);
			if (value.type != COM_VALUE_ERROR)
			{
				Ir result = ir_new_load(emitter->module, expr->site, local);
				value = com_value_ir(ir_new_expr_block(emitter->module, expr->site, stats, result));
			}
		}
		break;

		case AST_TABLE_ENTRY:
		{
			ir_report_error(emitter, ELF_ERROR_INTERNAL, expr->site, "table entries are only valid inside table literals");
			value = com_value_error(COM_STATUS_REPORTED);
		}
		break;

		default:
		{
			Com_Status status = checker_report(checker->source, ELF_ERROR_INTERNAL, ERROR_ERROR, expr->site, "'%s' is not an expression", ast_type_name(expr->kind));
			value = com_value_error(status);
		}
		break;
	}
	return value;
}

static Ir check_block(IrEmitter *checker, Ast stat)
{
	ASSERT(checker);
	ASSERT(stat);

	Scope *scope = 0;
	if (stat->kind == AST_BLOCK_STAT) {
		scope = enter_scope(checker, stat);
	}

	IrEmitter *emitter = checker;
	IrQueue queue = ir_queue_begin(emitter);
	IrDefer *defer_scope = ir_defer_begin(emitter);
	Com_Status status = COM_STATUS_OK;

	if (stat->kind == AST_BLOCK_STAT)
	{
		for (u32 i = 0; i < stat->block.count; ++ i)
		{
			status = check_stat(checker, &queue, stat->block.items[i]);
			if (status != COM_STATUS_OK) {
				break;
			}
		}
	}
	else {
		status = check_stat(checker, &queue, stat);
	}

	if (status == COM_STATUS_OK) {
		ir_defer_emit(emitter, &queue, emitter->defer_scope_start);
	}
	ir_defer_end(emitter, defer_scope);

	IrArray stats = ir_queue_end(&queue);
	if (scope) {
		pop_scope(checker, scope);
	}
	if (status != COM_STATUS_OK) {
		return ir_from_status(status);
	}
	return ir_new_block(emitter->module, stat->site, stats);
}

static Ir check_for(IrEmitter *checker, Ast stat)
{
	ASSERT(stat->kind == AST_FOR);

	IrEmitter *emitter = checker;
	Scope *scope = enter_scope(checker, stat);
	IrQueue block_items = ir_queue_begin(emitter);

	Com_Status status = check_stat(checker, &block_items, stat->for_stat.init);
	if (status != COM_STATUS_OK) {
		ir_queue_end(&block_items);
		pop_scope(checker, scope);
		return ir_from_status(status);
	}

	Ir pred = check_ir(checker, stat->for_stat.pred);
	if (ir_is_error(pred))
	{
		ir_queue_end(&block_items);
		pop_scope(checker, scope);
		return pred;
	}

	Ir step = check_block(checker, stat->for_stat.step);
	if (ir_is_error(step))
	{
		ir_queue_end(&block_items);
		pop_scope(checker, scope);
		return step;
	}

	Ir condition_label = ir_new_label(emitter->module, stat->site);
	Ir continue_label = ir_new_label(emitter->module, stat->for_stat.step->site);
	Ir break_label = ir_new_label(emitter->module, stat->site);

	IrLoopFrame loop_frame = {};
	loop_frame.continue_label = continue_label;
	loop_frame.break_label = break_label;
	loop_frame.defer_start = emitter->defer;
	ir_loop_push(emitter, &loop_frame);
	Ir body = check_block(checker, stat->for_stat.body);
	ir_loop_pop(emitter, &loop_frame);
	if (ir_is_error(body))
	{
		ir_queue_end(&block_items);
		pop_scope(checker, scope);
		return body;
	}

	Ir exit_jump = ir_new_jump_if_false(emitter->module, stat->for_stat.pred->site, pred, break_label);
	IrQueue loop_items = ir_queue_begin(emitter);
	ir_queue_push(&loop_items, condition_label);
	ir_queue_push(&loop_items, exit_jump);
	ir_queue_push(&loop_items, body);
	ir_queue_push(&loop_items, continue_label);
	ir_queue_push(&loop_items, step);
	ir_queue_push(&loop_items, ir_new_jump(emitter->module, stat->site, condition_label));
	ir_queue_push(&loop_items, break_label);
	IrArray loop_stats = ir_queue_end(&loop_items);
	ir_queue_push(&block_items, ir_new_block(emitter->module, stat->site, loop_stats));

	IrArray stats = ir_queue_end(&block_items);
	Ir result = ir_new_block(emitter->module, stat->site, stats);
	pop_scope(checker, scope);
	return result;
}

static Ir check_range_for(IrEmitter *checker, Ast stat)
{
	ASSERT(stat->kind == AST_FOR_RANGE);

	Ast decl = stat->for_range_stat.decl;
	ASSERT(decl);
	ASSERT(decl->kind == AST_DECL_STAT);
	Ast name_tuple = decl->decl.name;
	ASSERT(name_tuple);
	ASSERT(name_tuple->kind == AST_TUPLE);
	ASSERT(name_tuple->tuple.count);
	Ast expr_tuple = decl->decl.expr;
	ASSERT(expr_tuple);
	ASSERT(expr_tuple->kind == AST_TUPLE);
	ASSERT(expr_tuple->tuple.count == 1);

	Ast range_expr = expr_tuple->tuple.items[0];
	Ast collection_expr = 0;
	Ast range = range_expr;
	if (range_expr->kind == AST_RANGE_INDEX)
	{
		collection_expr = range_expr->binary.x;
		range = range_expr->binary.y;
	}
	if (!range || range->kind != AST_RANGE) {
		return ir_from_status(checker_report(checker->source, ELF_ERROR_INTERNAL, ERROR_ERROR, stat->site, "invalid range for-loop expression"));
	}

	IrEmitter *emitter = checker;
	Scope *scope = enter_scope(checker, stat);

	Ir collection_local = 0;
	if (collection_expr)
	{
		Ir collection = check_ir(checker, collection_expr);
		if (ir_is_error(collection)) {
			pop_scope(checker, scope);
			return collection;
		}
		collection_local = ir_new_local(emitter->module, emitter->function, collection_expr->site, collection);
	}

	Ast range_start_expr = range->binary.x;
	Ast range_end_expr = range->binary.y;
	SourceSite range_start_site = range_start_expr ? range_start_expr->site : range->site;
	SourceSite range_end_site = range_end_expr ? range_end_expr->site : range->site;
	Ir range_start = range_start_expr ? check_ir(checker, range_start_expr) : ir_new_int(emitter->module, range_start_site, 0);
	if (ir_is_error(range_start)) {
		pop_scope(checker, scope);
		return range_start;
	}

	Ir range_end = 0;
	if (range_end_expr) {
		range_end = check_ir(checker, range_end_expr);
	}
	else if (collection_local) {
		range_end = ir_new_measure(emitter->module, range_end_site, ir_new_load(emitter->module, range_end_site, collection_local));
	}
	else
	{
		pop_scope(checker, scope);
		return ir_from_status(checker_report(checker->source, ELF_ERROR_LOWERING, ERROR_ERROR, range->site, "an omitted range end requires an indexed collection"));
	}
	if (ir_is_error(range_end)) {
		pop_scope(checker, scope);
		return range_end;
	}

	u32 nvars = name_tuple->tuple.count;
	IrArray var_locals = ir_array_alloc(emitter, nvars);
	Com_Status status = COM_STATUS_OK;
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = name_tuple->tuple.items[i];
		if (!name || name->kind != AST_IDENT) {
			status = checker_report(checker->source, ELF_ERROR_INVALID_DECLARATION, ERROR_ERROR, name_tuple->site, "declaration name must be an identifier");
			break;
		}

		Ir local = ir_new_local(emitter->module, emitter->function, name->site, 0);
		var_locals.items[i] = local;
		Entity_Tags tags = decl->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
		Entity *entity = declare_entity(checker->active_scope, name->site, name->identifier, tags, com_value_ir(local));
		if (entity_is_error(entity)) {
			status = entity_status(entity);
			break;
		}
		entity->declaration = decl;
		name->entity = entity;
	}
	if (status != COM_STATUS_OK)
	{
		pop_scope(checker, scope);
		return ir_from_status(status);
	}

	Ir iterator_local = ir_new_local(emitter->module, emitter->function, range_start_site, range_start);
	Ir range_end_local = ir_new_local(emitter->module, emitter->function, range_end_site, range_end);
	Ast first_name = name_tuple->tuple.items[0];
	Ir start_label = ir_new_label(emitter->module, stat->site);
	Ir continue_label = ir_new_label(emitter->module, first_name->site);
	Ir break_label = ir_new_label(emitter->module, stat->site);

	IrLoopFrame loop_frame = {};
	loop_frame.continue_label = continue_label;
	loop_frame.break_label = break_label;
	loop_frame.defer_start = emitter->defer;
	ir_loop_push(emitter, &loop_frame);
	Ir body = check_block(checker, stat->for_range_stat.body);
	ir_loop_pop(emitter, &loop_frame);
	if (ir_is_error(body))
	{
		pop_scope(checker, scope);
		return body;
	}

	Ir iterator_load = ir_new_load(emitter->module, first_name->site, iterator_local);
	Ir end_load = ir_new_load(emitter->module, range_end_site, range_end_local);
	Ir pred = ir_new_binary(emitter->module, range->site, IR_LESS_THAN, iterator_load, end_load);
	Ir exit_jump = ir_new_jump_if_false(emitter->module, range->site, pred, break_label);

	IrQueue loop_items = ir_queue_begin(emitter);
	ir_queue_push(&loop_items, start_label);
	ir_queue_push(&loop_items, exit_jump);
	for (u32 i = 0; i < nvars; ++ i)
	{
		Ast name = name_tuple->tuple.items[i];
		Ir value = ir_new_load(emitter->module, name->site, iterator_local);
		if (i != 0) {
			value = ir_new_binary(emitter->module, name->site, IR_ADD, value, ir_new_int(emitter->module, name->site, i));
		}
		if (collection_local) {
			value = ir_new_index(emitter->module, name->site, ir_new_load(emitter->module, name->site, collection_local), value);
		}
		ir_queue_push(&loop_items, ir_new_store(emitter->module, name->site, var_locals.items[i], value));
	}

	Ir next_value = ir_new_binary(emitter->module, first_name->site, IR_ADD, ir_new_load(emitter->module, first_name->site, iterator_local), ir_new_int(emitter->module, first_name->site, nvars));
	Ir increment = ir_new_store(emitter->module, first_name->site, iterator_local, next_value);
	ir_queue_push(&loop_items, body);
	ir_queue_push(&loop_items, continue_label);
	ir_queue_push(&loop_items, increment);
	ir_queue_push(&loop_items, ir_new_jump(emitter->module, first_name->site, start_label));
	ir_queue_push(&loop_items, break_label);
	Ir loop = ir_new_block(emitter->module, stat->site, ir_queue_end(&loop_items));

	IrQueue block_items = ir_queue_begin(emitter);
	if (collection_local) {
		ir_queue_push(&block_items, collection_local);
	}
	ir_queue_push(&block_items, iterator_local);
	ir_queue_push(&block_items, range_end_local);
	for (u32 i = 0; i < nvars; ++ i) {
		ir_queue_push(&block_items, var_locals.items[i]);
	}
	ir_queue_push(&block_items, loop);
	Ir result = ir_new_block(emitter->module, stat->site, ir_queue_end(&block_items));
	pop_scope(checker, scope);
	return result;
}

static Ir lower_lvalue(IrEmitter *emitter, Ast expr)
{
	ASSERT(emitter);
	ASSERT(expr);

	if (expr->kind == AST_IDENT)
	{
		Entity *entity = expr->entity;
		if (!entity) {
			entity = scope_find(emitter->active_scope, expr->identifier);
		}
		if (!entity) {
			return ir_from_status(checker_report(emitter->source, ELF_ERROR_UNDECLARED_IDENTIFIER, ERROR_ERROR, expr->site, "undeclared identifier '%s'", atom_data(expr->identifier)));
		}
		expr->entity = entity;
		entity->referenced = true;
		if (entity_is_constant(entity))
		{
			Com_Status status = checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, expr->site, "'%s' is read-only", atom_data(expr->identifier));
			return ir_from_status(status);
		}
		if (entity->value.type != COM_VALUE_IR || entity->value.ir->kind != IR_LOCAL)
		{
			Com_Status status = checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, expr->site, "'%s' is not assignable", atom_data(expr->identifier));
			return ir_from_status(status);
		}
		Ir local = entity->value.ir;
		if (local->ir_local.owner != emitter->function)
		{
			Com_Status status = checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, expr->site, "captured binding '%s' is read-only", atom_data(expr->identifier));
			return ir_from_status(status);
		}
		return local;
	}

	Com_Value value = check_expr(emitter, expr);
	if (value.type == COM_VALUE_ERROR) {
		return ir_from_status(value.status);
	}
	if (value.type != COM_VALUE_IR)
	{
		Com_Status status = checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, expr->site, "expression is not assignable");
		return ir_from_status(status);
	}

	Ir ir = value.ir;
	if (ir->kind == IR_FIELD || ir->kind == IR_INDEX) {
		return ir;
	}
	Com_Status status = checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, expr->site, "expression is not assignable");
	return ir_from_status(status);
}

static Ir stabilize_lvalue(IrEmitter *emitter, IrQueue *items, Ir ir)
{
	ASSERT(ir);
	if (ir->kind != IR_FIELD && ir->kind != IR_INDEX) {
		return ir;
	}

	Ir receiver = ir_new_local(emitter->module, emitter->function, ir->ir_binary.x->site, ir->ir_binary.x);
	ir_queue_push(items, receiver);
	Ir x = ir_new_load(emitter->module, ir->ir_binary.x->site, receiver);
	Ir y = ir->ir_binary.y;
	if (ir->kind == IR_INDEX || y->kind != IR_ATOM)
	{
		Ir key = ir_new_local(emitter->module, emitter->function, y->site, y);
		ir_queue_push(items, key);
		y = ir_new_load(emitter->module, y->site, key);
	}
	return ir->kind == IR_INDEX ? ir_new_index(emitter->module, ir->site, x, y) : ir_new_binary(emitter->module, ir->site, IR_FIELD, x, y);
}

static Com_Status lower_compound_assign(IrEmitter *emitter, IrQueue *items, Ast stat)
{
	Ast dest_tuple = stat->binary.x;
	ASSERT(dest_tuple);
	ASSERT(dest_tuple->kind == AST_TUPLE);
	Ast expr_tuple = stat->binary.y;
	ASSERT(expr_tuple);
	ASSERT(expr_tuple->kind == AST_TUPLE);

	if (dest_tuple->tuple.count != 1 || expr_tuple->tuple.count != 1) {
		return checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, stat->site, "compound assignment requires one destination and one value");
	}

	Ast dest_ast = dest_tuple->tuple.items[0];
	Ir dest = lower_lvalue(emitter, dest_ast);
	if (ir_is_error(dest)) {
		return ir_status(dest);
	}

	Ir value = check_ir(emitter, expr_tuple->tuple.items[0]);
	if (ir_is_error(value)) {
		return ir_status(value);
	}

	dest = stabilize_lvalue(emitter, items, dest);
	Ir current = dest->kind == IR_LOCAL ? ir_new_load(emitter->module, dest_ast->site, dest) : dest;
	IrKind op = ir_kind_from_compound_assign_ast_kind(stat->kind);
	ASSERT(op != IR_NONE);
	Ir result = ir_new_binary(emitter->module, stat->site, op, current, value);
	ir_queue_push(items, ir_new_store(emitter->module, stat->site, dest, result));
	return COM_STATUS_OK;
}

static Com_Status lower_nil_assign(IrEmitter *emitter, IrQueue *items, Ast stat)
{
	Ast dest_tuple = stat->binary.x;
	ASSERT(dest_tuple);
	ASSERT(dest_tuple->kind == AST_TUPLE);
	Ast expr_tuple = stat->binary.y;
	ASSERT(expr_tuple);
	ASSERT(expr_tuple->kind == AST_TUPLE);

	if (dest_tuple->tuple.count != expr_tuple->tuple.count) {
		return checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, stat->site, "nil assignment requires one value for each destination");
	}

	for (u32 i = 0; i < dest_tuple->tuple.count; ++ i)
	{
		Ast dest_ast = dest_tuple->tuple.items[i];
		Ir dest = lower_lvalue(emitter, dest_ast);
		if (ir_is_error(dest)) {
			return ir_status(dest);
		}

		Ir value = check_ir(emitter, expr_tuple->tuple.items[i]);
		if (ir_is_error(value)) {
			return ir_status(value);
		}

		dest = stabilize_lvalue(emitter, items, dest);
		Ir current = dest->kind == IR_LOCAL ? ir_new_load(emitter->module, dest_ast->site, dest) : dest;
		Ir pred = ir_new_binary(emitter->module, stat->site, IR_EQ, current, ir_new_nil(emitter->module, dest_ast->site));
		Ir store = ir_new_store(emitter->module, stat->site, dest, value);
		IrQueue true_items = ir_queue_begin(emitter);
		ir_queue_push(&true_items, store);
		Ir true_clause = ir_new_block(emitter->module, stat->site, ir_queue_end(&true_items));
		ir_queue_push(items, ir_new_if(emitter->module, stat->site, pred, true_clause, 0));
	}
	return COM_STATUS_OK;
}

static Com_Status lower_assign(IrEmitter *emitter, IrQueue *items, Ast stat)
{
	Ast dest_tuple = stat->binary.x;
	ASSERT(dest_tuple);
	ASSERT(dest_tuple->kind == AST_TUPLE);
	Ast expr_tuple = stat->binary.y;
	ASSERT(expr_tuple);
	ASSERT(expr_tuple->kind == AST_TUPLE);
	u32 ndests = dest_tuple->tuple.count;
	u32 nexprs = expr_tuple->tuple.count;

	if (ndests == 1 && nexprs == 1)
	{
		Ir dest = lower_lvalue(emitter, dest_tuple->tuple.items[0]);
		if (ir_is_error(dest)) {
			return ir_status(dest);
		}
		Ir value = check_ir(emitter, expr_tuple->tuple.items[0]);
		if (ir_is_error(value)) {
			return ir_status(value);
		}
		ir_queue_push(items, ir_new_store(emitter->module, stat->site, dest, value));
		return COM_STATUS_OK;
	}

	if (nexprs > ndests) {
		return checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, stat->site, "assignment has more values than destinations");
	}

	b32 multi_call = nexprs < ndests;
	if (multi_call)
	{
		Ast final_expr = nexprs ? expr_tuple->tuple.items[nexprs - 1] : 0;
		if (!final_expr || final_expr->kind != AST_CALL) {
			return checker_report(emitter->source, ELF_ERROR_INVALID_ASSIGNMENT, ERROR_ERROR, stat->site, "assignment requires one value for each destination or a final function call");
		}
	}

	IrArray dests = ir_array_alloc(emitter, ndests);
	for (u32 i = 0; i < dests.count; ++ i)
	{
		dests.items[i] = lower_lvalue(emitter, dest_tuple->tuple.items[i]);
		if (ir_is_error(dests.items[i])) {
			return ir_status(dests.items[i]);
		}
	}

	IrArray locals = ir_array_alloc(emitter, ndests);
	u32 nfixed = multi_call ? nexprs - 1 : nexprs;
	for (u32 i = 0; i < nfixed; ++ i)
	{
		Ast expr = expr_tuple->tuple.items[i];
		Ir value = check_ir(emitter, expr);
		if (ir_is_error(value)) {
			return ir_status(value);
		}
		locals.items[i] = ir_new_local(emitter->module, emitter->function, expr->site, value);
		ir_queue_push(items, locals.items[i]);
	}

	if (multi_call)
	{
		Ast expr = expr_tuple->tuple.items[nfixed];
		Ir value = check_ir(emitter, expr);
		if (ir_is_error(value)) {
			return ir_status(value);
		}

		for (u32 i = nfixed; i < locals.count; ++ i) {
			locals.items[i] = ir_new_local(emitter->module, emitter->function, expr->site, 0);
		}
		IrArray results = { .items = locals.items + nfixed, .count = locals.count - nfixed };
		ir_queue_push(items, ir_new_multi_local(emitter->module, expr->site, value, results));
	}

	for (u32 i = 0; i < dests.count; ++ i)
	{
		Ir value = ir_new_load(emitter->module, locals.items[i]->site, locals.items[i]);
		ir_queue_push(items, ir_new_store(emitter->module, stat->site, dests.items[i], value));
	}
	return COM_STATUS_OK;
}

static Com_Status lower_decl(IrEmitter *emitter, IrQueue *items, Ast stat)
{
	Ast names = stat->decl.name;
	ASSERT(names);
	ASSERT(names->kind == AST_TUPLE);
	Ast exprs = stat->decl.expr;
	ASSERT(exprs);
	ASSERT(exprs->kind == AST_TUPLE);
	u32 nnames = names->tuple.count;
	u32 nexprs = exprs->tuple.count;

	for (u32 i = 0; i < nnames; ++ i)
	{
		Ast name = names->tuple.items[i];
		if (!name || name->kind != AST_IDENT) {
			return checker_report(emitter->source, ELF_ERROR_INVALID_DECLARATION, ERROR_ERROR, names->site, "declaration name must be an identifier");
		}
	}

	if (nexprs > nnames) {
		return checker_report(emitter->source, ELF_ERROR_INVALID_DECLARATION, ERROR_ERROR, stat->site, "declaration has more values than names");
	}

	b32 multi_call = nexprs < nnames;
	if (multi_call)
	{
		Ast final_expr = nexprs ? exprs->tuple.items[nexprs - 1] : 0;
		if (!final_expr || final_expr->kind != AST_CALL) {
			return checker_report(emitter->source, ELF_ERROR_INVALID_DECLARATION, ERROR_ERROR, stat->site, "declaration requires one value for each name or a final function call");
		}
	}

	if (stat->decl.type)
	{
		Com_Status status = com_value_status(check_expr(emitter, stat->decl.type));
		if (status != COM_STATUS_OK) {
			return status;
		}
	}

	for (u32 i = 0; i < nexprs; ++ i)
	{
		Ast expr = exprs->tuple.items[i];
		if (expr && (expr->kind == AST_RANGE || expr->kind == AST_RANGE_INDEX))
		{
			ir_report_error(emitter, ELF_ERROR_LOWERING, expr->site, "range expressions cannot be used as declaration values; use a range for loop");
			return COM_STATUS_REPORTED;
		}
	}

	if (nnames == 1 && nexprs == 1)
	{
		Ast name = names->tuple.items[0];
		Ir value = check_ir(emitter, exprs->tuple.items[0]);
		if (ir_is_error(value)) {
			return ir_status(value);
		}

		Ir local = ir_new_local(emitter->module, emitter->function, name->site, value);
		Entity_Tags tags = stat->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
		Entity *entity = declare_entity(emitter->active_scope, name->site, name->identifier, tags, com_value_ir(local));
		if (entity_is_error(entity)) {
			return entity_status(entity);
		}
		entity->declaration = stat;
		name->entity = entity;
		ir_queue_push(items, local);
		return COM_STATUS_OK;
	}

	IrArray values = ir_array_alloc(emitter, nexprs);
	for (u32 i = 0; i < values.count; ++ i)
	{
		values.items[i] = check_ir(emitter, exprs->tuple.items[i]);
		if (ir_is_error(values.items[i])) {
			return ir_status(values.items[i]);
		}
	}

	IrArray locals = ir_array_alloc(emitter, nnames);
	u32 nfixed = multi_call ? nexprs - 1 : nexprs;
	Entity_Tags tags = stat->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
	for (u32 i = 0; i < locals.count; ++ i)
	{
		Ast name = names->tuple.items[i];
		Ir value = i < nfixed ? values.items[i] : 0;
		Ir local = ir_new_local(emitter->module, emitter->function, name->site, value);
		locals.items[i] = local;
		Entity *entity = declare_entity(emitter->active_scope, name->site, name->identifier, tags, com_value_ir(local));
		if (entity_is_error(entity)) {
			return entity_status(entity);
		}
		entity->declaration = stat;
		name->entity = entity;
	}

	for (u32 i = 0; i < nfixed; ++ i) {
		ir_queue_push(items, locals.items[i]);
	}
	if (multi_call)
	{
		IrArray results = { .items = locals.items + nfixed, .count = locals.count - nfixed };
		ir_queue_push(items, ir_new_multi_local(emitter->module, exprs->site, values.items[nfixed], results));
	}
	return COM_STATUS_OK;
}

static Com_Status check_stat(IrEmitter *checker, IrQueue *items, Ast stat)
{
	ASSERT(checker);
	ASSERT(items);
	ASSERT(stat);
	ASSERT(checker->active_scope);
	IrEmitter *emitter = checker;

	switch (stat->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		case AST_STATIC_DECL:
		{
			return COM_STATUS_OK;
		}

		case AST_IMPORT:
		{
			return com_value_status(check_expr(checker, stat));
		}

		case AST_BLOCK_STAT:
		{
			Ir block = check_block(checker, stat);
			if (ir_is_error(block)) {
				return ir_status(block);
			}
			ir_queue_push(items, block);
			return COM_STATUS_OK;
		}

		case AST_DECL_STAT:
		{
			return lower_decl(emitter, items, stat);
		}

		case AST_IF:
		{
			Ir pred = check_ir(checker, stat->if_stat.pred);
			if (ir_is_error(pred)) {
				return ir_status(pred);
			}

			Ir true_clause = check_block(checker, stat->if_stat.true_clause);
			if (ir_is_error(true_clause)) {
				return ir_status(true_clause);
			}

			Ir else_clause = 0;
			if (stat->if_stat.else_clause)
			{
				else_clause = check_block(checker, stat->if_stat.else_clause);
				if (ir_is_error(else_clause)) {
					return ir_status(else_clause);
				}
			}

			ir_queue_push(items, ir_new_if(emitter->module, stat->site, pred, true_clause, else_clause));
			return COM_STATUS_OK;
		}

		case AST_WHILE:
		{
			Ir pred = check_ir(checker, stat->while_stat.pred);
			if (ir_is_error(pred)) {
				return ir_status(pred);
			}

			Ir start_label = ir_new_label(emitter->module, stat->site);
			Ir break_label = ir_new_label(emitter->module, stat->site);
			IrLoopFrame loop_frame = {};
			loop_frame.continue_label = start_label;
			loop_frame.break_label = break_label;
			loop_frame.defer_start = emitter->defer;
			ir_loop_push(emitter, &loop_frame);
			Ir body = check_block(checker, stat->while_stat.body);
			ir_loop_pop(emitter, &loop_frame);
			if (ir_is_error(body)) {
				return ir_status(body);
			}

			IrQueue loop_items = ir_queue_begin(emitter);
			ir_queue_push(&loop_items, start_label);
			ir_queue_push(&loop_items, ir_new_jump_if_false(emitter->module, stat->while_stat.pred->site, pred, break_label));
			ir_queue_push(&loop_items, body);
			ir_queue_push(&loop_items, ir_new_jump(emitter->module, stat->site, start_label));
			ir_queue_push(&loop_items, break_label);
			ir_queue_push(items, ir_new_block(emitter->module, stat->site, ir_queue_end(&loop_items)));
			return COM_STATUS_OK;
		}

		case AST_FOR:
		{
			Ir loop = check_for(checker, stat);
			if (ir_is_error(loop)) {
				return ir_status(loop);
			}
			ir_queue_push(items, loop);
			return COM_STATUS_OK;
		}

		case AST_FOR_RANGE:
		{
			Ir loop = check_range_for(checker, stat);
			if (ir_is_error(loop)) {
				return ir_status(loop);
			}
			ir_queue_push(items, loop);
			return COM_STATUS_OK;
		}

		case AST_DEFER_STAT:
		{
			Ir deferred = check_block(checker, stat->defer_stat.body);
			if (ir_is_error(deferred)) {
				return ir_status(deferred);
			}
			ir_defer_push_ir(emitter, deferred);
			return COM_STATUS_OK;
		}

		case AST_RETURN:
		{
			Ast expr = stat->return_stat.expr;
			Ir expr_ir = expr ? check_ir(checker, expr) : 0;
			if (ir_is_error(expr_ir)) {
				return ir_status(expr_ir);
			}

			/* revisit this because it might actually be unnecessary */
			if (expr_ir && emitter->defer)
			{
				ASSERT(expr_ir->kind == IR_TUPLE);
				IrArray tuple = ir_array_alloc(emitter, expr_ir->ir_tuple.count);
				for (u32 i = 0; i < expr_ir->ir_tuple.count; ++ i)
				{
					Ir local = ir_new_local(emitter->module, emitter->function, stat->site, expr_ir->ir_tuple.items[i]);
					ir_queue_push(items, local);
					tuple.items[i] = ir_new_load(emitter->module, stat->site, local);
				}
				expr_ir = ir_new_tuple(emitter->module, stat->site, tuple);
			}
			ir_defer_emit(emitter, items, 0);
			ir_queue_push(items, ir_new_return(emitter->module, stat->site, expr_ir));
			return COM_STATUS_OK;
		}

		case AST_BREAK:
		case AST_CONTINUE:
		{
			Ast expr = stat->kind == AST_BREAK ? stat->break_stat.expr : stat->continue_stat.expr;
			if (expr)
			{
				Com_Status status = com_value_status(check_expr(checker, expr));
				if (status != COM_STATUS_OK) {
					return status;
				}
			}

			IrLoopFrame loop = ir_loop_current(emitter, stat->site);
			Ir label = stat->kind == AST_BREAK ? loop.break_label : loop.continue_label;
			if (!label) {
				ir_queue_push(items, ERROR_IR);
				return COM_STATUS_REPORTED;
			}
			ir_defer_emit(emitter, items, loop.defer_start);
			ir_queue_push(items, ir_new_jump(emitter->module, stat->site, label));
			return COM_STATUS_OK;
		}

		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_MOD_ASSIGN:
		case AST_XOR_ASSIGN:
		case AST_SHL_ASSIGN:
		case AST_SHR_ASSIGN:
		{
			return lower_compound_assign(emitter, items, stat);
		}

		case AST_NIL_ASSIGN:
		{
			return lower_nil_assign(emitter, items, stat);
		}

		case AST_ASSIGN:
		{
			return lower_assign(emitter, items, stat);
		}

		case AST_TUPLE:
		{
			for (u32 i = 0; i < stat->tuple.count; ++ i)
			{
				Ast expr = stat->tuple.items[i];
				Com_Value value = check_expr(checker, expr);
				if (value.type == COM_VALUE_ERROR) {
					return value.status;
				}
				if (expr->kind != AST_IMPORT)
				{
					ASSERT(value.type == COM_VALUE_IR);
					ir_queue_push(items, value.ir);
				}
			}
			return COM_STATUS_OK;
		}

		default:
		{
			Ir ir = check_ir(checker, stat);
			if (ir_is_error(ir)) {
				return ir_status(ir);
			}
			ir_queue_push(items, ir);
			return COM_STATUS_OK;
		}
	}
}

static Com_Status check_function(IrEmitter *outer, IrFunction *function)
{
	ASSERT(outer);
	ASSERT(function);
	ASSERT(function->ast);
	ASSERT(function->ast->kind == AST_FUNCTION);
	ASSERT(function->ast->function.ir == function);

	Ast expr = function->ast;
	Ast_Array params = expr->function.params;
	Ast body = expr->function.body;

	if (!body) {
		return checker_report(outer->source, ELF_ERROR_MISSING_FUNCTION_BODY, ERROR_ERROR, expr->site, "missing function body");
	}

	IrEmitter emitter = {};
	emitter.module = outer->module;
	emitter.function = function;
	emitter.source = function->source;
	emitter.active_scope = outer->active_scope;
	emitter.ir_stack_size = 4096;
	emitter.ir_stack = arena_push_zero(emitter.module->arena, sizeof(*emitter.ir_stack) * emitter.ir_stack_size, _Alignof(Ir));
	if (!emitter.ir_stack) {
		return checker_out_of_memory(emitter.source, expr->site);
	}

	IrArray ir_params = ir_array_alloc(&emitter, params.count);
	function->params = ir_params;

	Scope *scope = enter_scope(&emitter, expr);

	Com_Status status = COM_STATUS_OK;
	for (u32 i = 0; i < params.count; ++ i)
	{
		Ast param = params.items[i];
		ASSERT(param);
		ASSERT(param->kind == AST_DECL_STAT);

		Ast name = param->decl.name;
		Ast type = param->decl.type;
		Ast expr = param->decl.expr;

		ASSERT(name);
		ASSERT(name->kind == AST_IDENT);
		ASSERT(name->atom);
		ASSERT(!type || type->kind == AST_IDENT);
		ASSERT(!name->entity);
		if (expr)
		{
			status = checker_report(emitter.source, ELF_ERROR_NOT_IMPLEMENTED, ERROR_ERROR, expr->site, "default arguments are not supported");
			break;
		}

		if (type)
		{
			Com_Value value = check_expr(&emitter, type);
			if (value.type == COM_VALUE_ERROR) {
				status = value.status;
				break;
			}

			if (value.type != COM_VALUE_TYPE) {
				status = checker_report(emitter.source, ELF_ERROR_NOT_A_TYPE, ERROR_ERROR, type->site, "expression does not evaluate to a type");
				break;
			}
		}

		Ir ir_param = ir_new_local(emitter.module, emitter.function, name->site, 0);
		ir_params.items[i] = ir_param;

		// TODO(RJ): we don't need to refetch this ...
		if (type) {
			ir_param->type_rule = type->entity->value.com_type->rule;
		}

		// TODO(RJ): dedicated builder
		Com_Value v_param = com_value_ir(ir_param);

		Entity_Tags tags = param->decl.constant ? ENTITY_TAG_CONSTANT : ENTITY_TAG_NONE;
		Entity *entity = declare_entity(emitter.active_scope, name->site, name->identifier, tags, v_param);
		if (entity_is_error(entity)) {
			status = entity_status(entity);
			break;
		}

		entity->declaration = param;
		name->entity = entity;
	}

	if (status == COM_STATUS_OK)
	{
		function->body = check_block(&emitter, body);
		if (ir_is_error(function->body)) {
			status = ir_status(function->body);
		}
	}
	pop_scope(&emitter, scope);
	return status;
}

static Com_Status prepare_scope(Com_Source *source, Scope *scope, Ast_Array stats)
{
	for (u32 i = 0; i < stats.count; ++ i)
	{
		Ast declaration = stats.items[i];
		if (declaration->kind != AST_STATIC_DECL) {
			continue;
		}

		Ast name = declaration->static_decl.name;
		Ast expr = declaration->static_decl.expr;
		ASSERT(name);
		ASSERT(name->kind == AST_IDENT);
		ASSERT(name->atom);
		ASSERT(!name->entity);
		ASSERT(expr);

		Entity_Tags tags = ENTITY_TAG_STATIC | ENTITY_TAG_CONSTANT;
		if (declaration->static_decl.exported) {
			tags |= ENTITY_TAG_EXPORTED;
		}
		Entity *entity = declare_entity(scope, name->site, name->atom, tags, (Com_Value) { .type = COM_VALUE_AST, .ast = expr });
		if (entity_is_error(entity)) {
			return entity_status(entity);
		}
		entity->declaration = declaration;
		name->entity = entity;
	}
	return COM_STATUS_OK;
}

static Com_Status prepare_ast_array(IrModule *mod, Com_Source *source, Scope *scope, IrFunction *func, Ast_Array array)
{
	for (u32 i = 0; i < array.count; ++ i)
	{
		Com_Status status = prepare_ast(mod, source, scope, func, array.items[i]);
		if (status != COM_STATUS_OK) {
			return status;
		}
	}
	return COM_STATUS_OK;
}

static Com_Status prepare_func(IrModule *mod, Com_Source *source, Scope *parent_scope, IrFunction *func)
{
	ASSERT(func);
	ASSERT(func->source == source);
	ASSERT(func->ast);
	ASSERT(func->ast->kind == AST_FUNCTION);
	ASSERT(func->ast->function.ir == func);

	Ast ast = func->ast;
	Scope *scope = push_scope(source, parent_scope, ast);
	if (!scope) {
		return COM_STATUS_REPORTED;
	}

	Com_Status status = prepare_ast_array(mod, source, scope, func, ast->function.params);
	if (status == COM_STATUS_OK) {
		status = prepare_ast(mod, source, scope, func, ast->function.body);
	}
	return status;
}

static Com_Status prepare_ast(IrModule *mod, Com_Source *source, Scope *scope, IrFunction *func, Ast ast)
{
	if (!ast) {
		return COM_STATUS_OK;
	}

	switch (ast->kind)
	{
		case AST_NONE:
		case AST_ERROR:
		case AST_IDENT:
		case AST_NUMBER_LITERAL:
		case AST_INTEGER_LITERAL:
		case AST_STRING_LITERAL:
		case AST_RECURSE:
		case AST_NIL_LITERAL:
		case AST_ELLIPSIS:
		{
			return COM_STATUS_OK;
		}

		case AST_IMPORT:
		{
			ASSERT(ast->import.target);
			return COM_STATUS_OK;
		}

		case AST_BITWISE_NOT:
		case AST_MEASURE:
		case AST_GET_MEM:
		{
			return prepare_ast(mod, source, scope, func, ast->unary);
		}

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
		case AST_ASSIGN:
		case AST_INDEX:
		case AST_FIELD:
		case AST_META_FIELD:
		case AST_COMMA_EXPR:
		case AST_RANGE:
		case AST_RANGE_INDEX:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->binary.x);
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->binary.y) : status;
		}

		case AST_INTERPOLATED_STRING:
		{
			return prepare_ast_array(mod, source, scope, func, ast->interpolated_string);
		}

		case AST_CALL:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->call.expr);
			return status == COM_STATUS_OK ? prepare_ast_array(mod, source, scope, func, ast->call.args) : status;
		}

		case AST_NAMED_ARGUMENT:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->named_argument.name);
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->named_argument.expr) : status;
		}

		case AST_TUPLE:
		{
			return prepare_ast_array(mod, source, scope, func, ast->tuple);
		}

		case AST_TABLE:
		{
			return prepare_ast_array(mod, source, scope, func, ast->table);
		}

		case AST_ARRAY:
		{
			return prepare_ast_array(mod, source, scope, func, ast->array);
		}

		case AST_TABLE_ENTRY:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->table_entry.key);
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->table_entry.value) : status;
		}

		case AST_FUNCTION:
		{
			ASSERT(!ast->function.ir);
			IrFunction *inner = ir_reserve_function(source, mod, func, ast);
			return prepare_func(mod, source, scope, inner);
		}

		case AST_DECL_STAT:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->decl.name);
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, scope, func, ast->decl.type);
			}
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->decl.expr) : status;
		}

		case AST_STATIC_DECL:
		{
			Ast expr = ast->static_decl.expr;
			ASSERT(expr);
			if (expr->kind == AST_FUNCTION)
			{
				Ast name = ast->static_decl.name;
				ASSERT(name);
				ASSERT(name->entity);
				ASSERT(name->entity->value.type == COM_VALUE_AST);
				ASSERT(name->entity->value.ast == expr);
				ASSERT(!expr->function.ir);

				IrFunction *static_func = ir_reserve_function(source, mod, 0, expr);
				name->entity->value = com_value_ir(ir_new_function_ref(mod, expr->site, static_func));
				return prepare_func(mod, source, scope, static_func);
			}
			return prepare_ast(mod, source, scope, func, expr);
		}

		case AST_BLOCK_STAT:
		{
			Scope *child_scope = push_scope(source, scope, ast);
			if (!child_scope) {
				return COM_STATUS_REPORTED;
			}

			Com_Status status = prepare_scope(source, child_scope, ast->block);
			if (status == COM_STATUS_OK) {
				status = prepare_ast_array(mod, source, child_scope, func, ast->block);
			}
			return status;
		}

		case AST_IF:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->if_stat.pred);
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, scope, func, ast->if_stat.true_clause);
			}
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, scope, func, ast->if_stat.else_clause);
			}
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->if_stat.then_clause) : status;
		}

		case AST_DEFER_STAT:
		{
			return prepare_ast(mod, source, scope, func, ast->defer_stat.body);
		}

		case AST_WHILE:
		{
			Com_Status status = prepare_ast(mod, source, scope, func, ast->while_stat.pred);
			return status == COM_STATUS_OK ? prepare_ast(mod, source, scope, func, ast->while_stat.body) : status;
		}

		case AST_FOR:
		{
			Scope *child_scope = push_scope(source, scope, ast);
			if (!child_scope) {
				return COM_STATUS_REPORTED;
			}

			Com_Status status = prepare_ast(mod, source, child_scope, func, ast->for_stat.init);
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, child_scope, func, ast->for_stat.pred);
			}
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, child_scope, func, ast->for_stat.step);
			}
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, child_scope, func, ast->for_stat.body);
			}
			return status;
		}

		case AST_FOR_RANGE:
		{
			Scope *child_scope = push_scope(source, scope, ast);
			if (!child_scope) {
				return COM_STATUS_REPORTED;
			}

			Com_Status status = prepare_ast(mod, source, child_scope, func, ast->for_range_stat.decl);
			if (status == COM_STATUS_OK) {
				status = prepare_ast(mod, source, child_scope, func, ast->for_range_stat.body);
			}
			return status;
		}

		case AST_RETURN:
		{
			return prepare_ast(mod, source, scope, func, ast->return_stat.expr);
		}

		case AST_BREAK:
		{
			return prepare_ast(mod, source, scope, func, ast->break_stat.expr);
		}

		case AST_CONTINUE:
		{
			return prepare_ast(mod, source, scope, func, ast->continue_stat.expr);
		}

		default:
		{
			ASSERT(0);
			return COM_STATUS_REPORTED;
		}
	}
}

static Com_Status prepare_source(IrModule *mod, Com_Source *source)
{
	ASSERT(source);
	if (source->state >= COM_SOURCE_PREPARED) {
		return COM_STATUS_OK;
	}
	ASSERT(source->state == COM_SOURCE_PARSED);

	Compiler *compiler = source->compiler;
	ASSERT(compiler);
	ASSERT(compiler->globals);

	Ast root = source->ast;
	ASSERT(root);
	ASSERT(root->kind == AST_FUNCTION);
	ASSERT(!root->function.parent);
	ASSERT(root->function.body);
	ASSERT(root->function.body->kind == AST_BLOCK_STAT);

	IrFunction *func = ir_reserve_function(source, mod, 0, root);
	Scope *function_scope = push_scope(source, compiler->globals, root);
	if (!function_scope) {
		return COM_STATUS_REPORTED;
	}

	Com_Status status = prepare_ast_array(mod, source, function_scope, func, root->function.params);
	Scope *source_scope = 0;
	if (status == COM_STATUS_OK)
	{
		source_scope = push_scope(source, function_scope, root->function.body);
		if (!source_scope) {
			status = COM_STATUS_REPORTED;
		}
	}
	if (status == COM_STATUS_OK)
	{
		ASSERT(!source->scope);
		source->scope = source_scope;
		status = prepare_scope(source, source_scope, root->function.body->block);
	}
	if (status == COM_STATUS_OK) {
		status = prepare_ast_array(mod, source, source_scope, func, root->function.body->block);
	}
	if (status == COM_STATUS_OK) {
		source->state = COM_SOURCE_PREPARED;
	}
	return status;
}

static Ir eval_static(elf_State *state, IrModule *module, Com_Source *source, Ir ir)
{
	ASSERT(module);
	ASSERT(source);
	ASSERT(ir);
	switch (ir->kind)
	{
		case IR_NIL:
		case IR_INTEGER:
		case IR_NUMBER:
		case IR_ATOM:
		case IR_NATIVE_FUNCTION:
		case IR_FUNCTION_REF:
		case IR_TAB_REF:
		case IR_ARR_REF:
		{
			return ir;
		}

		default: break;
	}

	ASSERT(state);
	IrEmitter emitter = {};
	emitter.module = module;
	emitter.source = source;

	IrFunction eval = {};
	eval.source = source;
	eval.site = ir->site;
	eval.body = ir_new_return(emitter.module, ir->site, ir);

	elf_Prototype *proto = bc_emit_func(state, module, &eval);
	push_value(state, proto_value(proto));
	u32 nrets = elf_call(state, 0);
	ASSERT(nrets == 1);

	elf_Value result = state->stack_top[-1];
	return ir_from_value(&emitter, ir->site, result);
}

static Com_Status eval_statics(IrModule *module, Compiler *compiler)
{
	for (Entity *entity = compiler->entities; entity; entity = entity->next_in_compiler)
	{
		ASSERT(entity->tags & ENTITY_TAG_STATIC);
		ASSERT(entity->declaration);
		ASSERT(entity->declaration->kind == AST_STATIC_DECL);
		Ast expr = entity->declaration->static_decl.expr;
		ASSERT(expr);

		if (expr->kind == AST_FUNCTION)
		{
			ASSERT(entity->value.type == COM_VALUE_IR);
			ASSERT(entity->value.ir->kind == IR_FUNCTION_REF);

			IrEmitter checker = {};
			checker.module = module;
			checker.source = entity->scope->source;
			checker.active_scope = entity->scope;

			Com_Status status = check_function(&checker, entity->value.ir->func);
			if (status != COM_STATUS_OK) {
				return status;
			}
			continue;
		}

		IrEmitter checker = {};
		checker.module = module;
		checker.source = entity->scope->source;
		checker.active_scope = entity->scope;

		Com_Value value = check_entity(&checker, expr->site, entity);
		if (value.type == COM_VALUE_ERROR) {
			return value.status;
		}
	}
	return COM_STATUS_OK;
}

static Com_Status check_unreferenced(Compiler *compiler)
{
	for (Entity *entity = compiler->entities; entity; entity = entity->next_in_compiler)
	{
		if (entity->referenced || entity_is_exported(entity)) {
			continue;
		}
		ASSERT(entity->declaration);
		ASSERT(entity->scope->source);
		Com_Status status = checker_warn(entity->scope->source, ELF_ERROR_UNREFERENCED_ENTITY, entity->declaration->site, "declaration of '%s' is never referenced", atom_data(entity->name));
		if (status != COM_STATUS_OK) {
			return status;
		}
	}
	return COM_STATUS_OK;
}

static Com_Status check(IrModule *module, Compiler *com)
{
	ASSERT(module);
	ASSERT(com);

	for (Com_Source *current = com->first_source; current; current = current->next)
	{
		Com_Status status = prepare_source(module, current);
		if (status != COM_STATUS_OK) {
			return status;
		}
	}

	{
		Com_Status status = eval_statics(module, com);
		if (status != COM_STATUS_OK) {
			return status;
		}
	}

	for (Com_Source *current = com->first_source; current; current = current->next)
	{
		ASSERT(current->state == COM_SOURCE_PREPARED);
		ASSERT(current->ast->function.ir);

		IrEmitter checker = {};
		checker.module = module;
		checker.source = current;
		checker.active_scope = com->globals;

		Com_Status status = check_function(&checker, current->ast->function.ir);
		current->state = COM_SOURCE_CHECKED;
		if (status != COM_STATUS_OK) {
			return status;
		}
	}
	return check_unreferenced(com);
}
