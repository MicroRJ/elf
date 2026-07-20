# Elf C API

> Elf is still early. This describes the current embedding model, not a
> compatibility promise. `include/elf.h` remains the source of truth.

Elf uses a stack API because hosts such as Bob exchange arbitrary values,
nested tables, and functions with scripts. Generic stack operations avoid a
growing matrix of typed table functions. Values on the stack are also visible
to the garbage collector without exposing runtime object pointers to the host.

## Addressing

Elf retains its zero-based frame convention:

- `0` is the receiver, or `this`.
- `1` is the first explicit argument.
- `-1` is the topmost stack value.
- `-2` is the value immediately below it.

`elf_stack_arg_count` includes `this`. Save the current top before temporary
work and restore it afterward:

```c
elf_i32 checkpoint = elf_stack_get_top(S);
/* Work with stack values. */
elf_stack_set_top(S, checkpoint);
```

Use `elf_stack_abs_index` before pushing values above a table:

```c
elf_stack_new_table(S);
elf_i32 table = elf_stack_abs_index(S, -1);

elf_push_int(S, 4);
elf_stack_set_field(S, table, "workers");
```

Set and add operations consume the top value. Get operations push their result,
using `nil` for a missing field or index.

## Native functions

```c
static ELF_FUNCTION(host_list)
{
	elf_StrSlice root;
	if (!elf_stack_to_str(S, 1, &root)) {
		elf_push_nil(S);
		return 1;
	}

	elf_stack_new_table(S);
	elf_i32 result = elf_stack_abs_index(S, -1);
	elf_push_cstr(S, "src/main.c");
	elf_stack_add(S, result);
	return 1;
}
```

Libraries are ordinary nested tables built with `elf_stack_new_table` and
`elf_stack_set_field`, then published with `elf_stack_set_global`.

## References

An `elf_Ref` keeps a value alive after it leaves the stack:

```c
elf_Ref reference = elf_stack_create_ref(S, -1);
elf_stack_pop(S, 1);

if (elf_push_ref(S, reference)) {
	/* Referenced value is now at -1. */
	elf_stack_pop(S, 1);
}

elf_stack_release_ref(S, reference);
```

References belong to the state that created them and must be released. String
slices returned by `elf_stack_to_str` are borrowed; copy their bytes if they
must outlive the stack value or its reference.

The API does not yet provide protected execution. Runtime failures still use
Elf's existing fatal error path.
