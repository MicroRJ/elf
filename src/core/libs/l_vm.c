//
// See Copyright Notice In elf.h
//

static void vm_set_counter(elf_State *state, elf_Table *table, const char *name, u64 value)
{
	elf_Value key = value_from_string(elf_string_from_data(state, name));
	elf_table_set(state, table, key, value_from_integer((i64)value));
}

ELF_FUNCTION(l_vm_counters)
{
	lib_check_arg_count(S, "vm.counters", nargs, 0, 1);

	u64 closure_calls = S->vm_closure_calls;
	u64 native_calls = S->vm_native_calls;
	u64 calls = closure_calls + native_calls;
	u64 instructions = S->vm_instructions;
	u64 max_stack_slots = S->vm_max_stack_slots;
	u64 max_call_depth = S->vm_max_call_depth;

	elf_Table *result = 0;
	if (nargs == 2)
	{
		elf_Value value = load_value(S, 1);
		check_value_type(S, value, ELF_VALUE_TYPE_TABLE);
		result = value_as_table(value);
		push_table(S, result);
	}
	else result = push_new_table(S);

	vm_set_counter(S, result, "calls", calls);
	vm_set_counter(S, result, "closure_calls", closure_calls);
	vm_set_counter(S, result, "native_calls", native_calls);
	vm_set_counter(S, result, "instructions", instructions);
	vm_set_counter(S, result, "max_stack_slots", max_stack_slots);
	vm_set_counter(S, result, "max_call_depth", max_call_depth);
	return 1;
}

static const elf_Binding l_vm[] = {
	{"counters", l_vm_counters},
};

static elf_Table *elf_lib_vm(elf_State *state)
{
	return new_binding_table(state, l_vm, ARRAY_COUNT(l_vm));
}
