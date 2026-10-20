# elf

elf is a small scripting language for programs that want C to remain in charge.
It is useful for configuration, build descriptions, automation, and data-heavy
logic that benefits from quick iteration.

elf is currently `0.2.0-dev`. Its language, embedding API, and module behavior
are still evolving.

```elf
make_person := fun(name: str) {
	ret {
		name = name,
		greet = fun(greeting = "hello") {
			elf.println(f"${greeting}, ${this.name}")
		},
	}
}

people := [make_person("Ada"), make_person("Grace")]

for person := people[...] ? {
	person.greet("hello")
}
```

elf has integers, numbers, strings, arrays, tables, buffers, closures, native
functions, errors, and host-owned user objects. Arrays are zero-indexed and
tables provide named and computed fields.

## Language tour

### Values, declarations, and assignment

```elf
count := 0                 // mutable local
limit ::= 60               // constant local
answer :: 40 + 2           // statically resolved declaration

count += 1
count ?= load_cached()      // assign only when count is nil

left, right := 10, 20
left, right = right, left

items := [10, 20, 30]
user := {
	name = "Ada",
	["favorite"] = "compiler",
	[7] = "lucky",
}
```

Table entries are assignments and require commas between entries. A key may be
a field name, a bracketed expression, or an expression followed by `=`.

### Functions, calls, and `this`

```elf
scale :: fun(value: numeric, factor = 2) {
	ret value * factor
}

doubled := scale(10)
tripled := scale(10, factor = 3)

collector := fun(first, ...) {
	ret first, elf.nvargs(), elf.varg(0)
}
```

Parameters may have defaults and runtime-checked annotations. The current type
names are `int`, `num`, `numeric`, `str`, `arr`, `tab`, `buf`, `err`,
`non_nil`, and `callable`. Default and named arguments require a statically
known elf function.

Every call has an implicit receiver available as `this`. Calling a table field
or type method passes the value on the left as that receiver:

```elf
counter := {
	value = 0,
	increment = fun() {
		this.value += 1
		ret this.value
	},
}

counter.increment()
items:add(40)
```

### Control flow and iteration

```elf
if score > 100 ? {
	label = "high"
}
elif score > 50 ? {
	label = "medium"
}
else {
	label = "low"
}

while pending ? {
	poll()
}

for i := 0 ... 10 ? {
	if i == 3 ? continue
	if i == 8 ? break
}

for value := items[...] ? {
	use(value)
}
```

Ranges are half-open. Arrays and strings can be iterated with `[...]`,
`[start ... end]`, `[start ...]`, or `[... end]`.

`defer` runs a statement when its lexical scope exits, including through
`ret`, `break`, and `continue`:

```elf
file := elf.fs.open("output.bin", "w")
if elf.is_error(file) ? ret file
defer file:close()
```

### Strings, returns, and statement endings

```elf
name := "elf"
message := f"hello ${name}"

pair := fun() {
	ret 10, 20
}

left, right := pair()
```

Strings use `"..."`; interpolated strings use `f"..."` and `${expression}`.
Functions may return zero, one, or several values. Newlines normally separate
statements, and semicolons may be used as optional terminators. Comments use
`//` or `/* ... */`.

## Builtin surface

Every state exposes an `elf` table together with methods on the builtin value
types.

| Surface | What it provides |
| --- | --- |
| `elf` | printing, assertions, conversions, error inspection, and argument access |
| string methods | slicing, searching, splitting, trimming, replacement, and joining |
| array methods | mutation, slicing, cloning, mapping, filtering, and sorting |
| table methods | size, membership, deletion, cloning, and keys |
| `elf.buffer` | mutable byte storage and string conversion |
| `elf.math` | numeric utilities and common math functions |
| `elf.fs`, `elf.path`, `elf.env` | files, directories, paths, and environment variables |
| `elf.json` | JSON parsing, serialization, files, and JSON Lines |
| `elf.random`, `elf.time` | pseudorandom values and clocks |
| `elf.gc`, `elf.vm` | runtime diagnostic counters |

Errors returned as values can be checked with `elf.is_error(value)`.

## Examples

The scripts used by the test suite are the best source of current examples:

- [`test/scripts/`](test/scripts/) contains automatically discovered assertion
  scripts, including [named arguments](test/scripts/named_arguments.elf) and
  [file I/O](test/scripts/file_io.elf).
- [`test/compiler/smoke/`](test/compiler/smoke/) contains focused language and
  library examples.
- Start with [`method_this.elf`](test/compiler/smoke/method_this.elf),
  [`for_range.elf`](test/compiler/smoke/for_range.elf),
  [`defer.elf`](test/compiler/smoke/defer.elf), and
  [`interpolated_strings.elf`](test/compiler/smoke/interpolated_strings.elf).

## Building

The supported build environment is Windows x64 with Git, `clang-cl`, and
Microsoft's `lib.exe`. Clone the repository with its `dy-core` submodule:

```bat
git clone --recursive https://github.com/MicroRJ/elf.git
cd elf
```

Build [Manny](https://github.com/MicroRJ/Manny#build-from-source) and make sure
`manny` is available on `PATH`.

```bat
build.bat build
build.bat test
build.bat lib
build.bat optimized
build.bat benchmark
```

Build products are written beneath `build/`.

## Embedding

The public C interface is declared in [`include/elf.h`](include/elf.h). An
embedder creates an `elf_State`, pushes source or a source file, calls the
resulting function, exchanges values through the stack API, and destroys the
state when finished. Native C functions use the same stack and receive the
implicit receiver at argument index zero.

## License

elf is available under the [MIT License](LICENSE).
