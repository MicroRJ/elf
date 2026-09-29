# elf C API

> elf is still early. This describes the current embedding model, not a
> compatibility promise. `include/elf.h` remains the source of truth.

elf uses a stack API because hosts such as Manny exchange arbitrary values,
nested tables, and functions with scripts. Generic stack operations avoid a
growing matrix of typed table functions. Values on the stack are also visible
to the garbage collector without exposing runtime object pointers to the host.

## Addressing

elf retains its zero-based frame convention:

- `0` is the receiver, or `this`.
- `1` is the first explicit argument.
- `-1` is the topmost stack value.
- `-2` is the value immediately below it.

`elf_arg_count` includes `this`. Save the current top before temporary
work and restore it afterward:

```c
elf_i32 checkpoint = elf_get_top(S);
/* Work with stack values. */
if (elf_set_top(S, checkpoint) != ELF_ERROR_NONE) {
	/* checkpoint was outside the current frame */
}
```

Use `elf_abs_index` before pushing values above a table:

```c
elf_new_table(S);
elf_i32 table = elf_abs_index(S, -1);

elf_push_int(S, 4);
if (elf_set_field(S, table, "workers") != ELF_ERROR_NONE) {
	/* the integer remains on the stack */
}
```

Set and add operations consume the top value. Get operations push their result,
using `nil` for a missing field or index. Fallible mutations return
`ELF_ERROR_NONE` on success or a specific `elf_ErrorCode` on failure. A failed
mutation does not consume or otherwise change stack values.

## Native functions

```c
static ELF_FUNCTION(host_list)
{
	elf_StrSlice root;
	if (!elf_to_str(S, 1, &root)) {
		elf_push_nil(S);
		return 1;
	}

	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	elf_push_cstr(S, "src/main.c");
	elf_append(S, result);
	return 1;
}
```

Libraries are ordinary nested tables built with `elf_new_table` and
`elf_set_field`, then published with `elf_set_global`.

`elf_to_cstr` returns the same borrowed string data with a guaranteed trailing
NUL for host APIs that require C strings. `elf_push_value_text` formats any
stack value and pushes the resulting elf string; the batteries use it to
implement printing without accessing elf's value representation.

Native functions can report invalid arguments or host failures with
`elf_error`. It enters elf's normal runtime-error path and does not return to
the native function.

## References

An `elf_Ref` keeps a value alive after it leaves the stack:

```c
elf_Ref reference = elf_create_ref(S, -1);
elf_pop(S, 1);

if (elf_push_ref(S, reference)) {
	/* Referenced value is now at -1. */
	elf_pop(S, 1);
}

if (elf_release_ref(S, reference) != ELF_ERROR_NONE) {
	/* reference was already released or invalid */
}
```

References belong to the state that created them and must be released. String
slices returned by `elf_to_str` are borrowed; copy their bytes if they
must outlive the stack value or its reference.

The API does not yet provide protected execution. Runtime failures still use
elf's existing fatal error path.
