//
// See Copyright Notice In elf.h
//

typedef union
{
	i64           integer;
	f64           number;
	dy_String     string;
	elf_SymTable *table;
	elf_Function  function;
}
SymbolInput;

static b32 symbol_name_equal(dy_String left, dy_String right)
{
	return left.size == right.size && (!left.size || !memcmp(left.data, right.data, (size_t)left.size));
}

static b32 symbol_exists(elf_SymTable *table, dy_String name)
{
	for (elf_SymField *field = table->first_field; field; field = field->next)
	{
		if (symbol_name_equal(field->name, name))
		{
			return true;
		}
	}
	return false;
}

static ErrorCode symbol_copy_string(dy_String source, dy_String *destination)
{
	ASSERT(destination);
	*destination = (dy_String) {0};
	if (!source.data && source.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (source.size == UINT64_MAX || source.size + 1 > SIZE_MAX)
	{
		return ERROR_OUT_OF_RANGE;
	}

	char *data = malloc((size_t)source.size + 1);
	if (!data)
	{
		return ERROR_OUT_OF_MEMORY;
	}
	if (source.size)
	{
		copy_memory(data, source.data, source.size);
	}
	data[source.size] = 0;
	destination->data = data;
	destination->size = source.size;
	return ERROR_NONE;
}

static ErrorCode symbol_validate_table(elf_SymTable *parent, elf_SymTable *child)
{
	if (!child || child->parent)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	for (elf_SymTable *ancestor = parent; ancestor; ancestor = ancestor->parent)
	{
		if (ancestor == child)
		{
			return ERROR_INVALID_ARGUMENT;
		}
	}
	return ERROR_NONE;
}

static ErrorCode symbol_add(elf_SymTable *table, dy_String name, elf_SymbolType type, SymbolInput input)
{
	if (!table || !name.data || !name.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (table->field_count == UINT32_MAX)
	{
		return ERROR_OUT_OF_RANGE;
	}
	if (type == ELF_SYMBOL_STRING && !input.string.data && input.string.size)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (type == ELF_SYMBOL_TABLE)
	{
		ErrorCode error = symbol_validate_table(table, input.table);
		if (error != ERROR_NONE)
		{
			return error;
		}
	}
	if (type == ELF_SYMBOL_FUNCTION && !input.function)
	{
		return ERROR_INVALID_ARGUMENT;
	}
	if (type > ELF_SYMBOL_FUNCTION || symbol_exists(table, name))
	{
		return ERROR_INVALID_ARGUMENT;
	}

	elf_SymField *field = malloc(sizeof(*field));
	if (!field)
	{
		return ERROR_OUT_OF_MEMORY;
	}
	zero_memory(field, sizeof(*field));

	ErrorCode error = symbol_copy_string(name, &field->name);
	if (error != ERROR_NONE)
	{
		free(field);
		return error;
	}

	dy_String string = {0};
	if (type == ELF_SYMBOL_STRING)
	{
		error = symbol_copy_string(input.string, &string);
		if (error != ERROR_NONE)
		{
			free(field->name.data);
			free(field);
			return error;
		}
	}

	field->type = type;
	switch (type)
	{
		case ELF_SYMBOL_INTEGER:  field->integer = input.integer; break;
		case ELF_SYMBOL_NUMBER:   field->number = input.number; break;
		case ELF_SYMBOL_STRING:   field->string = string; break;
		case ELF_SYMBOL_TABLE:    field->table = input.table; break;
		case ELF_SYMBOL_FUNCTION: field->function = input.function; break;
	}

	if (table->last_field)
	{
		table->last_field->next = field;
	}
	else
	{
		table->first_field = field;
	}
	table->last_field = field;
	table->field_count += 1;
	if (type == ELF_SYMBOL_TABLE)
	{
		input.table->parent = table;
	}
	return ERROR_NONE;
}

static elf_ErrorCode symbol_error(ErrorCode error)
{
	switch (error)
	{
		case ERROR_NONE:             return ELF_ERROR_NONE;
		case ERROR_INVALID_ARGUMENT: return ELF_ERROR_INVALID_ARGUMENT;
		case ERROR_OUT_OF_RANGE:     return ELF_ERROR_OUT_OF_RANGE;
		case ERROR_OUT_OF_MEMORY:    return ELF_ERROR_OUT_OF_MEMORY;
		default:
		{
			ASSERT(!"unhandled symbol error");
			return ELF_ERROR_INVALID_ARGUMENT;
		}
	}
}

elf_ErrorCode elf_sym_create(elf_SymTable **table)
{
	if (!table)
	{
		return ELF_ERROR_INVALID_ARGUMENT;
	}
	*table = malloc(sizeof(**table));
	if (!*table)
	{
		return ELF_ERROR_OUT_OF_MEMORY;
	}
	zero_memory(*table, sizeof(**table));
	return ELF_ERROR_NONE;
}

static void symbol_table_destroy(elf_SymTable *table)
{
	if (!table)
	{
		return;
	}
	for (elf_SymField *field = table->first_field; field;)
	{
		elf_SymField *next = field->next;
		free(field->name.data);
		if (field->type == ELF_SYMBOL_STRING)
		{
			free(field->string.data);
		}
		else if (field->type == ELF_SYMBOL_TABLE)
		{
			symbol_table_destroy(field->table);
		}
		free(field);
		field = next;
	}
	free(table);
}

void elf_sym_destroy(elf_SymTable *table)
{
	if (table && !table->parent)
	{
		symbol_table_destroy(table);
	}
}

elf_ErrorCode elf_sym_tab(elf_SymTable *table, elf_StrSlice name, elf_SymTable *value)
{
	SymbolInput input = {.table = value};
	return symbol_error(symbol_add(table, (dy_String) {name.data, name.size}, ELF_SYMBOL_TABLE, input));
}

elf_ErrorCode elf_sym_int(elf_SymTable *table, elf_StrSlice name, elf_Int value)
{
	SymbolInput input = {.integer = value};
	return symbol_error(symbol_add(table, (dy_String) {name.data, name.size}, ELF_SYMBOL_INTEGER, input));
}

elf_ErrorCode elf_sym_num(elf_SymTable *table, elf_StrSlice name, elf_Num value)
{
	SymbolInput input = {.number = value};
	return symbol_error(symbol_add(table, (dy_String) {name.data, name.size}, ELF_SYMBOL_NUMBER, input));
}

elf_ErrorCode elf_sym_str(elf_SymTable *table, elf_StrSlice name, elf_StrSlice value)
{
	SymbolInput input = {.string = (dy_String) {value.data, value.size}};
	return symbol_error(symbol_add(table, (dy_String) {name.data, name.size}, ELF_SYMBOL_STRING, input));
}

elf_ErrorCode elf_sym_fun(elf_SymTable *table, elf_StrSlice name, elf_Function value)
{
	SymbolInput input = {.function = value};
	return symbol_error(symbol_add(table, (dy_String) {name.data, name.size}, ELF_SYMBOL_FUNCTION, input));
}
