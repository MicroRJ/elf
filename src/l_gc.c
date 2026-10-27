//
// See Copyright Notice In elf.h
//

static void gc_set_counter(elf_State *state, elf_Table *table,
	const char *name, u64 value)
{
	elf_Value key = str_value(elf_string_from_data(state, name));
	elf_table_set(state, table, key, int_value((i64)value));
}

ELF_FUNCTION(l_gc_counters)
{
	if (!lib_check_arg_count(S, "gc.counters", nargs, 0, 1)) {
		return 1;
	}

	u64 allocated_objects = S->gc_allocated_objects;
	u64 allocated_bytes = S->gc_allocated_bytes;
	u64 allocated_closures = S->gc_allocated_by_type[ELF_OBJECT_CLOSURE];
	u64 allocated_tables = S->gc_allocated_by_type[ELF_OBJECT_TABLE];
	u64 allocated_strings = S->gc_allocated_by_type[ELF_OBJECT_STRING];
	u64 allocated_buffers = S->gc_allocated_by_type[ELF_OBJECT_BUFFER];
	u64 allocated_arrays = S->gc_allocated_by_type[ELF_OBJECT_ARRAY];
	u64 freed_objects = S->gc_freed_objects;
	u64 freed_bytes = S->gc_freed_bytes;
	u64 collections = S->gc_collection_count;
	u64 live_objects = S->gc_reference_count;
	u64 live_bytes = S->gc_live_bytes;
	u64 peak_live_bytes = S->gc_peak_live_bytes;
	u64 collection_time_ns = S->gc_collection_time_ns;
	u64 last_collection_time_ns = S->gc_last_collection_time_ns;
	u64 max_collection_time_ns = S->gc_max_collection_time_ns;

	elf_Table *result = 0;
	if (nargs == 1)
	{
		elf_Value value = load_value(S, 0);
		check_value_type(S, value, ELF_VALUE_TYPE_TABLE);
		result = value_as_table(value);
		push_table(S, result);
	}
	else result = push_new_table(S);

	gc_set_counter(S, result, "allocated_objects", allocated_objects);
	gc_set_counter(S, result, "allocated_bytes", allocated_bytes);
	gc_set_counter(S, result, "allocated_closures", allocated_closures);
	gc_set_counter(S, result, "allocated_tables", allocated_tables);
	gc_set_counter(S, result, "allocated_strings", allocated_strings);
	gc_set_counter(S, result, "allocated_buffers", allocated_buffers);
	gc_set_counter(S, result, "allocated_arrays", allocated_arrays);
	gc_set_counter(S, result, "freed_objects", freed_objects);
	gc_set_counter(S, result, "freed_bytes", freed_bytes);
	gc_set_counter(S, result, "collections", collections);
	gc_set_counter(S, result, "live_objects", live_objects);
	gc_set_counter(S, result, "live_bytes", live_bytes);
	gc_set_counter(S, result, "peak_live_bytes", peak_live_bytes);
	gc_set_counter(S, result, "collection_time_ns", collection_time_ns);
	gc_set_counter(S, result, "last_collection_time_ns", last_collection_time_ns);
	gc_set_counter(S, result, "max_collection_time_ns", max_collection_time_ns);
	return 1;
}
