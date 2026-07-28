# Core library

The core library consists of the functions stored directly on the global
`elf` table:

```elf
elf.println("Hello, Elf")
type := elf.type_of(value)
```

Its implementation is registered in `src/core/libs/l_core.c` and split by
responsibility among the neighboring `l_core_*.c` files.

## API summary

| Function | Result | Purpose |
| --- | --- | --- |
| `elf.assert(condition, message?)` | none | Stop execution when a numeric condition is zero. |
| `elf.metatable(value)` | table or `nil` | Obtain the shared metatable for a supported value type. |
| `elf.freeze(value)` | value | Recursively make a table graph readonly. |
| `elf.is_readonly(value)` | integer boolean | Test an object's readonly flag. |
| `elf.type_of(value)` | string | Return the runtime type name. |
| `elf.to_string(value)` | string | Convert a value to its display text. |
| `elf.is_atom(value)` | integer boolean | Test whether a value is a string/atom. |
| `elf.is_numeric(value)` | integer boolean | Test whether a value is an integer or number. |
| `elf.to_number(value)` | number | Convert a numeric value to a floating-point number. |
| `elf.to_integer(value)` | integer | Convert a numeric value to an integer. |
| `elf.arg(index)` | value or `nil` | Read an argument from the calling bytecode frame. |
| `elf.nargs()` | integer | Return the caller's total argument count. |
| `elf.varg(index)` | value or `nil` | Read one of the caller's variadic arguments. |
| `elf.nvargs()` | integer | Return the caller's variadic argument count. |
| `elf.nrets()` | integer | Return how many results the caller's call site requested. |
| `elf.load_file(path, arguments...)` | loaded file results | Compile and tail-call an Elf source file. |
| `elf.const_expr(source)` | value | Parse a value composed only of constant syntax. |
| `elf.print(values...)` | integer | Print values and return the bytes written. |
| `elf.println(values...)` | integer | Print values followed by a newline. |
| `elf.printl(values...)` | integer | Compatibility spelling of `println`. |

Unless a function explicitly documents an out-of-range result, a wrong
argument count or type is a runtime error.

## Assertions

### `elf.assert(condition, message?)`

`condition` must be numeric. Zero triggers a runtime error; any nonzero value
passes. This function does not currently accept arbitrary values according to
the language's conditional rules.

The message is optional and must be a string when supplied:

```elf
elf.assert(count >= 0)
elf.assert(file != nil, "could not open file")
```

The default error message is `"assertion failed"`.

## Metatables and readonly objects

### `elf.metatable(value)`

Returns the metatable shared by all values of the supplied runtime type.
Metatables currently exist for:

- strings/atoms
- tables
- integers
- numbers

Other types return `nil`. The returned table is shared runtime state: changing
it changes field and method lookup for every value of that type.

```elf
table_meta := elf.metatable({})
table_meta.describe = fun() {
	ret "a table"
}
```

### `elf.freeze(value)`

Marks an object readonly and returns the same value, allowing it to be used in
an initializer or expression:

```elf
settings := elf.freeze({
	width = 1280,
	height = 720,
})
```

For tables, freezing is recursive. Both table keys and stored values are
visited. Cyclic table graphs are supported because each object is marked
before its references are followed.

Tables, closures, and user objects are accepted. Mutation prevention is
currently observable primarily on tables: assigning into a frozen table raises
a runtime error.

### `elf.is_readonly(value)`

Returns `1` when the supplied table, closure, or user object has been marked
readonly, and `0` otherwise. Supplying a non-object value is a type error.

## Type inspection and conversion

### `elf.type_of(value)`

Returns one of the current runtime type names:

| Value kind | Result |
| --- | --- |
| `nil` | `"nil"` |
| integer | `"integer"` |
| floating-point number | `"number"` |
| string/atom | `"string"` |
| table | `"table"` |
| native function | `"function"` |
| Elf closure | `"closure"` |
| user object | `"resource"` |

### `elf.is_atom(value)` and `elf.is_numeric(value)`

These predicates return integer booleans (`0` or `1`). `is_numeric` accepts
both the integer and floating-point number representations.

```elf
elf.assert(elf.is_atom("text"))
elf.assert(elf.is_numeric(42))
elf.assert(elf.is_numeric(3.5))
```

### `elf.to_number(value)` and `elf.to_integer(value)`

Both functions require a numeric input. `to_number` produces the floating-point
representation. `to_integer` discards the fractional part toward zero.

```elf
n := elf.to_number(7)       // type_of(n) == "number"
i := elf.to_integer(7.75)  // i == 7
```

These are representation conversions, not string parsers.

### `elf.to_string(value)`

Returns the same display text used by string interpolation, string-left
addition, `elf.print`, and `elf.println`:

```elf
text := elf.to_string({ answer = 42 }) // "{answer = 42}"
```

The result is intended for display and diagnostics. It is not a
round-trippable serialization format: strings inside tables are not quoted,
and functions and closures use short display labels.

## Caller introspection

The caller functions inspect the bytecode function that invoked them. They are
low-level facilities used to implement variadic functions and methods.

Every Elf call has an implicit receiver at argument index `0`. An ordinary
function call normally receives `nil` there; a method call receives its `this`
value. Explicit parameters begin at index `1`. Consequently, `nargs()` includes
the implicit receiver.

```elf
inspect := fun(first, ...) {
	elf.assert(elf.arg(0) == nil)
	elf.assert(elf.arg(1) == first)
	elf.assert(elf.nargs() == 4)
	elf.assert(elf.nvargs() == 2)
	elf.assert(elf.varg(0) == 20)
	elf.assert(elf.varg(1) == 30)
}

inspect(10, 20, 30)
```

`arg(index)` returns `nil` when the index is negative or greater than or equal
to `nargs()`. `varg(index)` uses a separate zero-based index over only the extra
variadic arguments and also returns `nil` when out of range.

`nvargs()` returns zero for a non-variadic caller. `nrets()` reports the number
of values requested by the caller's call site; a call used only as a statement
requests zero results.

Caller introspection requires an Elf bytecode caller. Calling these functions
directly from a host/native context without one is a runtime error.

## Loading and constant data

### `elf.load_file(path, arguments...)`

Compiles the source file at `path`, passes along every argument after the path,
and tail-calls the compiled file. Its results are the loaded file's results.

Given `module.elf`:

```elf
ret elf.arg(1) + 1
```

it can be invoked as:

```elf
answer := elf.load_file("module.elf", 41)
elf.assert(answer == 42)
```

### `elf.const_expr(source)`

Parses one constant value from a string. Supported constant forms are:

- `nil`
- integer literals
- number literals
- string literals
- table literals whose keys and values are themselves constant forms

```elf
answer := elf.const_expr("42")
options := elf.const_expr("{ width = 320, title = \"demo\" }")
```

This function does not evaluate operators or general Elf code. For example,
`elf.const_expr("40 + 2")` is currently an error rather than a request to
constant-fold the addition.

## Standard output

### `elf.print(values...)`

Converts each supplied value to text, concatenates the results without adding
separators, and writes them to standard output. It returns the number of bytes
written.

```elf
bytes := elf.print("score: ", score)
```

### `elf.println(values...)`

Behaves like `print`, then writes a newline. The returned byte count includes
the newline.

```elf
elf.println("score: ", score)
elf.println()
```

`elf.printl` is retained as a compatibility alias. New code should use
`elf.println`.

## Related libraries

The following tables are installed beneath `elf`, but are not part of the core
API:

- `elf.debug` — runtime diagnostic counters
- `elf.serialization` — serialization and JSON loading
- `elf.fs` — filesystem operations
- `elf.math`, `elf.random`, `elf.path`, `elf.process`, and `elf.time`
