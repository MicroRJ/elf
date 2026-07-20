# Elf public C API

Elf exposes two C interfaces while the embedding API is being migrated:

- The legacy view API returns `elf_ValueView`, `elf_Table *`, and
  `elf_String *`. It remains available so existing hosts keep building.
- The versioned stack API, identified by `ELF_STACK_API_VERSION`, is the
  preferred interface for new integrations.

The two APIs may be mixed during migration. New code should keep values on the
stack or in an `elf_Ref` and avoid retaining raw runtime pointers.

## Why the stack model

Elf's important embedding use case is a host such as Bob exchanging structured
configuration, functions, and nested tables with a script. A typed accessor API
does not scale well for that job. It tends to grow a separate operation for
every combination, such as `table_set_integer`, `table_set_string`, and
`table_set_table`, followed by the same matrix for globals, array elements, and
return values.

The stack API has one representation for every Elf value. Code pushes any value
and then applies a generic operation to it. Adding a future value type therefore
does not require another family of table and global functions.

The model also matches Elf's ownership rules:

- A value on the stack is reachable by the garbage collector.
- A value needed after it leaves the stack is stored in a state-owned `elf_Ref`.
- Borrowed string data remains valid only while its Elf value is reachable.
- Hosts do not normally retain, release, or compare raw runtime pointers.

The cost is explicit stack discipline. Host code should save a top checkpoint,
perform its work, and restore that checkpoint before returning control.

## Stack addressing

Elf keeps its existing zero-based frame layout. The stack API does **not** use
Lua's one-based argument convention.

- Index `0` is the receiver, conventionally called `this`.
- Index `1` is the first explicit argument.
- Index `2` is the second explicit argument, and so on.
- Index `-1` is the value at the top of the stack.
- Index `-2` is the value immediately below it.

`elf_stack_arg_count` includes `this`. A native call with `this` and two
explicit arguments therefore reports three arguments.

`elf_stack_get_top` returns a zero-based frame offset, not merely an argument
count. A host should treat its initial value as an opaque checkpoint:

```c
elf_i32 checkpoint = elf_stack_get_top(S);

/* Push values, inspect results, and call Elf here. */

elf_stack_set_top(S, checkpoint);
```

The current native callback ABI keeps arguments and reserved frame slots below
a protected floor. `elf_stack_pop` and `elf_stack_set_top` cannot remove that
prefix. Native results and temporary values are pushed above it. This preserves
the behavior of existing `ELF_FUNCTION` callbacks while the public API migrates.

Use `elf_stack_abs_index` when an operation will push more values above a table:

```c
elf_stack_new_table(S);
elf_i32 result = elf_stack_abs_index(S, -1);

elf_push_cstr(S, "hello");
elf_stack_set_field(S, result, "message"); /* consumes "hello" */
```

An invalid absolute conversion returns `-1`.

## Native callback example

The following callback receives a directory-like object as `this`, reads its
first explicit argument, and returns a table:

```c
static ELF_FUNCTION(list_files)
{
    elf_StrSlice pattern;
    if (!elf_stack_to_str(S, 1, &pattern)) {
        /* Report an argument error using the host's current policy. */
        return 0;
    }

    elf_stack_new_table(S);
    elf_i32 result = elf_stack_abs_index(S, -1);

    /* For each file found by the host: */
    elf_push_cstr(S, "src/main.c");
    elf_stack_add(S, result);              /* consumes the string */

    return 1;
}
```

The legacy `ELF_FUNCTION` signature remains:

```c
int function(elf_State *S, int nargs, int nrets);
```

The returned integer is the number of values the callback pushed above its
entry frame. Existing `nargs` and `nrets` parameters are unchanged.

## Values and conversions

The existing push functions are shared by both APIs:

```c
elf_push_nil(S);
elf_push_int(S, 42);
elf_push_num(S, 3.5);
elf_push_cstr(S, "text");
elf_push_str(S, data, size);
elf_push_fun(S, callback);
```

The stack additions are:

```c
elf_stack_push_value(S, index);  /* duplicate a value */
elf_stack_type(S, index);
elf_stack_is_nil(S, index);
elf_stack_is_numeric(S, index);
elf_stack_is_callable(S, index);
elf_stack_to_int(S, index, &integer);
elf_stack_to_num(S, index, &number);
elf_stack_to_str(S, index, &slice);
elf_stack_equal(S, left, right);
```

Conversions return false when the index or type is wrong. They do not perform a
fatal typed argument check. `elf_stack_to_num` accepts either an Elf integer or
number; `elf_stack_to_int` is intentionally strict and accepts only an integer.

`elf_stack_to_str` returns a borrowed `elf_StrSlice`. Copy it if the bytes must
outlive the stack value or its owning `elf_Ref`.

`elf_stack_type` returns `ELF_VALUE_TYPE_COUNT_` for an invalid index.

## Tables

Table operations take the table's stack index. Set and add operations consume
the value currently at the top on success. Get operations push a value and push
`nil` for a missing field or positional index.

```c
elf_stack_new_table(S);
elf_i32 table = elf_stack_abs_index(S, -1);

elf_push_int(S, 12);
elf_stack_set_field(S, table, "workers");

elf_push_cstr(S, "build/main.c");
elf_stack_add(S, table);

elf_stack_get_field(S, table, "workers");
elf_Integer workers;
if (elf_stack_to_int(S, -1, &workers)) {
    /* use workers */
}
elf_stack_pop(S, 1);
```

The table functions are:

```c
elf_stack_new_table(S);
elf_stack_length(S, table, &length);
elf_stack_get_field(S, table, "name");
elf_stack_set_field(S, table, "name");
elf_stack_get_index(S, table, index);
elf_stack_set_index(S, table, index);
elf_stack_add(S, table);
```

Elf tables have one value store. A keyed field aliases a value in that store,
so keyed and positional values contribute to the same length and storage index
space. `elf_stack_set_index` overwrites an existing index or appends when the
index equals the current length; a larger index is rejected.

Iteration uses an external zero-initialized cursor and pushes a key followed by
its value:

```c
elf_u32 cursor = 0;
while (elf_stack_next(S, table, &cursor)) {
    /* key is -2; value is -1 */
    elf_stack_pop(S, 2);
}
```

Each stored value is visited once. If it has a keyed alias, iteration returns
that key; otherwise it returns its integer storage index.

## Globals and nested modules

Globals use ordinary stack values:

```c
elf_stack_new_table(S);
elf_i32 module = elf_stack_abs_index(S, -1);

elf_push_fun(S, list_files);
elf_stack_set_field(S, module, "list_files");

elf_stack_set_global(S, "host"); /* consumes the module table */
```

More deeply nested modules are built the same way: create a child table, fill
it, then set it as a field of its parent. This replaces source injection and
private global helper names with normal Elf values and direct function calls.

`elf_stack_get_global` always pushes the result, using `nil` when the name is
not present. `elf_stack_set_global` consumes the top value on success.

## Long-lived references

Use `elf_Ref` when native state must hold an Elf value across calls:

```c
elf_Ref task = elf_stack_create_ref(S, table_index);
if (task == ELF_NO_REF) {
    /* invalid index, nil value, or exhausted reference identifiers */
}

if (elf_push_ref(S, task)) {
    /* the referenced value is now at -1 and rooted on the stack */
}

elf_stack_release_ref(S, task);
```

References belong to the `elf_State` that created them. They are opaque integer
handles, keep their values reachable by the collector, and must eventually be
released. A failed `elf_push_ref` does not modify the stack. Releasing an
invalid or already released reference returns false.

## Current limitation: error boundaries

The stack API makes value exchange and ownership explicit, but it does not yet
add protected execution. Runtime failures still follow Elf's existing runtime
error policy. A future protected-call API should be designed around a real VM
unwind boundary rather than layered over `elf_call` as a cosmetic wrapper.
