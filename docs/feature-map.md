# elf feature map

This is the single maintained map of elf's language, runtime, compiler, and
embedding surface. elf is currently `0.2.0-dev`; syntax and APIs may still
change. The implementation and tests remain the final authority.

## A small program

```elf
make_adder :: fun(base) {
	ret fun(value) {
		ret base + value
	}
}

values := [10, 20, 30]
add_ten := make_adder(10)
sum := 0

for value := values[...] ? {
	sum += add_ten(value)
}

ret sum
```

An elf source is the body of its entry function. Top-level executable
statements run when that function is called.

## Current feature status

| Area | Status |
| --- | --- |
| Integers, numbers, strings, arrays, tables, buffers, and `nil` | Working |
| Mutable, constant, and static declarations | Working |
| Functions, closures, captures, recursion, and variadic parameters | Working |
| Default and named arguments for statically known elf functions | Working |
| Conditionals, loops, ranges, `break`, `continue`, and `defer` | Working |
| String interpolation and inline JSON values | Working |
| Stack-based C embedding API and native functions | Working |
| Filesystem, path, environment, JSON, random, and time libraries | Working |
| Imports, exports, and source namespaces | Experimental |
| Parameter annotations and general type checking | Parsed, not enforced |
| Exceptions, enums, and several reserved keywords | Not implemented |

“Working” means the feature is exercised by the current test suite, not that
its interface is frozen.

## Values and data structures

- `nil` is the absence of a value.
- Integers are signed 64-bit values. `true` and `false` are the integers `1`
  and `0`.
- Numbers are double-precision floating-point values.
- Strings are immutable counted byte strings.
- Arrays are mutable, zero-indexed sequences.
- Tables are mutable key/value collections used for records and maps.
- Buffers are mutable byte sequences intended for binary data and I/O.
- Functions and native C functions are callable values. Nested elf functions
  become closures when they capture their environment.
- Errors are ordinary inspectable values. APIs adopting the value-error policy
  return an error value instead of aborting; use `elf.is_error(value)`.
- User objects let native libraries attach host-owned data and a destructor to
  an elf value. Files are currently user objects.

Arrays and tables are distinct runtime types:

```elf
items := [10, 20, 30]
user := {
	name = "Ada",
	["favorite"] = "compiler",
	[7] = "lucky",
}

items[0] = 11
user.name = "Grace"
dynamic := user["favorite"]
```

Fields use `.`, while `:` reads from the value's type metatable. Calls through
either form pass the value on the left as the receiver (`this`):

```elf
items:add(40)
last := items:pop()
```

## Language basics

### Declarations and assignment

```elf
count := 0          // mutable declaration
limit ::= 60        // constant local declaration
answer :: 40 + 2    // statically resolved declaration

count = 1
count += 2
count -= 1
cached ?= make_value() // assign only when the destination is nil

left, right := 10, 20
left, right = right, left
```

`::` declarations are collected and resolved by the checker before lowering.
They currently support compile-time values such as constants, functions, and
imports. `export` is only valid on a static declaration.

### Literals

```elf
nothing := nil
yes := true
decimal := 42
binary := 0b101010
hex := 0x2a
fraction := 3.5
letter := 'E'
text := "hello\nelf"
message := f"answer: ${answer}"
array := [1, 2, 3]
table := { name = "elf", [7] = "seven" }
```

Strings use `"` and stay on one source line. Backtick strings and multiline
string blocks are not part of the language. Interpolated strings use `f"..."`
and `${expression}`.

Inline JSON uses JSON syntax and produces elf arrays, tables, strings,
numbers, integers, and `nil`:

```elf
config := json {
	"name": "elf",
	"window": [1280, 720],
	"optional": null
}
```

### Functions and calls

```elf
combine :: fun(a, b = 20, c = 3) {
	ret a * 100 + b * 10 + c
}

first := combine(1, 2, 3)
second := combine(4)
third := combine(5, c = 7)
```

Named arguments must follow positional arguments, appear in parameter order,
and may not bind a parameter twice. Defaults are evaluated only when the
argument is omitted. These rules currently require the checker to know the
callee's elf function definition.

The final parameter may be variadic:

```elf
collect := fun(first, ...) {
	ret first, elf.nvargs(), elf.varg(0)
}
```

Use `recurse` to call the currently running elf function:

```elf
factorial :: fun(n) {
	if n <= 1 ? ret 1
	ret n * recurse(n - 1)
}
```

Functions may return zero, one, or several values with `ret`. Missing values
in a multi-assignment become `nil`.

### Control flow

```elf
if score > 100 ? {
	result = "high"
}
elif score > 50 ? {
	result = "medium"
}
else {
	result = "low"
}

while ready == 0 ? {
	poll()
}

for i := 0; i < 10; i += 1 ? {
	if i == 3 ? continue
	if i == 8 ? break
}

for i := 0 ... 3 ? {
	use(i) // 0, 1, 2
}

for value := values[...] ? {
	use(value)
}
```

Ranges are half-open. Array and string iteration may use `[...]`,
`[start ... end]`, `[start ...]`, or `[... end]`. Several loop variables consume
several values per iteration.

`defer` runs a statement when its lexical scope exits, including through
`ret`, `break`, and `continue`:

```elf
file := elf.fs.open("output.bin", "w")
if elf.is_error(file) ? ret file
defer file:close()
```

### Operators

From highest binary precedence to lowest:

```text
**
* / %
+ -
<< >>
< <= > >=
== !=
&
^
|
&&
|| ??
...
```

Unary `+`, `-`, and `~` are implemented. `&&` and `||` short-circuit and return
`0` or `1`. `??` evaluates and returns its right operand only when the left is
`nil`. Compound assignment supports `+=`, `-=`, `*=`, `/=`, `%=`, `^=`,
`<<=`, and `>>=`.

### Compact grammar

This sketch covers the stable center of the current parser. Newlines normally
separate statements; semicolons separate the clauses of a C-style `for`.

```text
source       := statement*
statement    := block | declaration | assignment | expression
              | if | while | for | defer statement
              | "ret" expressions? | "break" | "continue"
block        := "{" statement* "}"

declaration  := names (":=" | "::=") expressions
static_decl  := "export"? identifier "::" expression
assignment   := expressions ("=" | "?=" | compound_assign) expressions

if           := "if" expression "?" statement
                ("elif" expression "?" statement)*
                ("else" statement)?
while        := "while" expression "?" statement
for          := "for" names (":=" | "::=") expressions
                ("?" statement
                | ";" expression ";" expression "?" statement)

function     := "fun" "(" parameters? ")" statement
parameter    := identifier (":" identifier)? ("=" expression)?
parameters   := parameter ("," parameter)* ("," "...")? | "..."
call         := expression "(" arguments? ")"
argument     := expression | identifier "=" expression

array        := "[" (expression ","?)* "]"
table        := "{" [table_entry ("," table_entry)* [","]] "}"
table_entry  := identifier "=" expression
              | "[" expression "]" "=" expression
              | expression ("=" expression)?
```

Comments use `//` to end of line or `/* ... */`.

## Runtime libraries

Every state installs the core `elf` table and type methods. The host-facing
libraries are also installed by the default state constructor.

| Surface | Purpose |
| --- | --- |
| `elf` | assertions, reflection, conversions, errors, argument inspection |
| string methods | slicing, searching, splitting, trimming, case conversion, replacement, joining |
| array methods | mutation, slicing, cloning, mapping, filtering, sorting |
| table methods | size, membership, deletion, cloning, keys |
| `elf.buffer` and buffer methods | mutable bytes, slicing, splicing, conversion to string |
| `elf.math` | numeric utilities and common math functions |
| `elf.gc`, `elf.vm` | diagnostic counters |
| `elf.fs` | file handles, paths, directories, copying, moving, and removal |
| `elf.path` | pure path joining and component extraction |
| `elf.env` | environment variable access |
| `elf.json` | JSON parse, stringify, file loading, and JSON Lines loading |
| `elf.random`, `elf.time` | pseudorandom numbers and clocks/sleep |
| `elf.print`, `elf.println` | host output |

The filesystem handle API is buffer-based:

```elf
file := elf.fs.open("data.bin", "r+")
if elf.is_error(file) ? ret file
defer file:close()

data := elf.buffer()
count := file:read(data, 4096)
if elf.is_error(count) ? ret count

file:seek(0, "begin")
written := file:write(data)
```

`elf.fs.open` accepts `r`, `w`, `a`, `r+`, `w+`, or `a+`. File methods are
`read`, `write`, `seek`, `tell`, `size`, and `close`. Explicit flushing is not
available yet.

## Imports and exports

Imports are intentionally not a headline feature yet. The current checker can
load a source namespace and expose its exported static declarations:

```elf
// math.elf
offset :: 2
export add :: fun(value) { ret value + offset }

// root.elf
math :: import "math.elf"
ret math.add(40)
```

Path resolution, source loading behavior, namespace semantics, and the public
shape of this system are still experimental.

## Compiler model

The implementation currently flows through these representations:

```text
source -> tokens -> AST -> checked scopes/entities -> IR -> bytecode prototypes -> VM
```

- Tokens are temporary parser input and are discarded after parsing.
- The AST preserves source structure and carries checker annotations.
- The checker creates persistent lexical scopes and entities, resolves names,
  static values, calls, imports, and captures.
- IR makes control flow, locals, closures, and evaluation order explicit.
- The backend emits self-contained bytecode prototypes and installs them
  through the runtime target interface.
- The VM executes slot-based frames; the garbage collector owns runtime
  strings, arrays, tables, buffers, closures, errors, and user objects.

These structures are implementation boundaries, not public serialization
formats.

## C embedding

The supported C interface is declared in [`include/elf.h`](../include/elf.h).
Its basic shape is:

1. Create an `elf_State` with `elf_create_state`.
2. Push source with `elf_push_code_source` or `elf_push_code_file`.
3. Push arguments and invoke the callable with `elf_call`.
4. Exchange values through the stack, tables, arrays, globals, and references.
5. Destroy the state with `elf_destroy_state`.

Native functions use `ELF_FUNCTION(name)`, read arguments from the current
frame, push results, and return the result count. Index `0` is the receiver,
index `1` is the first explicit argument, and `-1` is the top stack value.
`elf_Ref` keeps a runtime value alive after it leaves the stack.

The header is the source of truth for exact signatures and error returns; the
embedding API is still evolving.

## Known gaps and intentionally absent features

- Imports and exports are experimental.
- Parameter annotations are syntax only; elf does not yet have general static
  type checking.
- `try`, `catch`, `finally`, `enum`, `global`, `default`, `do`, and `then` are
  reserved but do not provide working statements.
- Unary `!`, `=>`, `->`, standalone `...`, and `!!` are not supported language
  expressions.
- Backtick strings, multiline string blocks, trailing-closure calls, process
  helpers, and stream helpers were deliberately removed.
- Runtime failures do not yet have a complete protected-execution model.
- Files do not yet expose an explicit flush operation.

## Runnable examples

The end-to-end scripts are the least ambiguous examples of current behavior:

- [`test/compiler/scripts/named_arguments.elf`](../test/compiler/scripts/named_arguments.elf)
- [`test/compiler/scripts/file_io.elf`](../test/compiler/scripts/file_io.elf)
- [`test/compiler/smoke/closure_capture.elf`](../test/compiler/smoke/closure_capture.elf)
- [`test/compiler/smoke/for_range.elf`](../test/compiler/smoke/for_range.elf)
- [`test/compiler/smoke/defer.elf`](../test/compiler/smoke/defer.elf)
- [`test/compiler/smoke/interpolated_strings.elf`](../test/compiler/smoke/interpolated_strings.elf)
- [`test/compiler/smoke/script_assert_tables.elf`](../test/compiler/smoke/script_assert_tables.elf)
- [`test/compiler/smoke/script_assert_array_lib.elf`](../test/compiler/smoke/script_assert_array_lib.elf)
- [`test/compiler/smoke/script_assert_buffer.elf`](../test/compiler/smoke/script_assert_buffer.elf)
- [`test/compiler/smoke/script_assert_json.elf`](../test/compiler/smoke/script_assert_json.elf)

Run the complete suite with:

```bat
build.bat test
```
