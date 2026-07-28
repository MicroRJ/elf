# Elf by example

Elf is still growing!

This guide describes syntax that reaches the current compiler and calls out
syntax that is only partially implemented. The parser and compiler are the
authority when this document falls behind.

## First spell

```elf
name := "world"
elf.println(f"hello, ${name}!")
```

`:=` makes a name. `=` changes it.

```elf
score := 10
score = 20
score += 5

cached := nil
cached ?= make_value() // assign only while nil
```

There are more compound assignments:

```elf
x -= 1
x *= 2
x /= 4
x %= 3
x ^= mask
x <<= 1
x >>= 1
```

`::=` makes a constant binding. The name cannot be assigned again.

```elf
answer ::= 42
left, right := 10, 20
```

Declarations, assignments, and returns may carry several values. Missing
right-hand values become `nil`.

```elf
left, right = 10 // right becomes nil
```

The binding is constant, not the value behind it:

```elf
options ::= { debug = false }
options.debug = true // okay: the table is still mutable
options = {}         // error: options cannot point somewhere else
```

## Values

```elf
nothing := nil
yes := true
no := false

decimal := 42
binary := 0b101010
hex := 0x2a
fraction := 3.5
scientific := 1.5e-2
letter := 'E'
text := "hello"
```

Integers are signed 64-bit values. Number literals containing a decimal point
or a decimal `e`/`E` exponent are floating-point values. `1.5e-2` means
`1.5 * 10 ** -2`. `true` and `false` are the integer values `1` and `0`;
character literals are integer Unicode code points.

Comments come in two flavors:

```elf
// Gone by the end of the line.

/* Gone when the block closes. */
```

## Strings

```elf
single_line := "hello\nelf"

many_lines := """hello
from a string block"""

name := "Ada"
greeting := f"hello ${name}"

report := f"""name: ${name}
score: ${score}"""
```

Escapes include `\0`, `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`, `\/`,
`\'`, `\"`, `\xNN`, `\uNNNN`, and `\UNNNNNNNN`.

Strings have methods too:

```elf
shout := " hello ":trim():upper()
```

See the [string library](libraries/string.md) for the full bag of tricks.

## Tables

One value, many jobs: array, map, object.

```elf
numbers := {10, 20, 30}

player := {
	name = "Ada",
	score = 100,
	["favorite"] = "compiler",
	[7] = "lucky",
}

key := "favorite"
player.[key] = "compiler"
```

Table entries have three forms:

```elf
values := {
	10,                   // unkeyed array item
	name = "Ada",         // identifier key; shorthand for ["name"] = "Ada"
	[make_key()] = value, // computed key
	"answer" = 42,        // expression key
}
```

Commas between table entries are accepted and strongly preferred. The current
parser also accepts adjacent entries without commas.

Read them in the same shapes:

```elf
first := numbers[0]
name := player.name
favorite := player.[key]
name, score := player.(name, score)
```

Plain brackets address the array by integer index. A dot followed by brackets
performs keyed lookup with an arbitrary expression:

```elf
array_value := values[0]
numeric_key := values.[0]
dynamic_key := values.[key]
```

`value.name` is shorthand for `value.["name"]`. Consequently, `value[0]` and
`value.[0]` are different operations.

Indexes can chain:

```elf
cell := matrix[row, column] // matrix[row][column]
```

Fields use `.`, while metatable fields use `:`:

```elf
name := player.name
numbers:add(40)
last := numbers:pop()
```

Calling either a field or metatable field passes the value on the left as the
implicit receiver, `this`. Ordinary function calls receive `nil` as `this`.

See the [table library](libraries/table.md) for the rest.

## Math and comparisons

From tightest grip to loosest:

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

All binary operators currently associate from left to right, including `**`.
Postfix operations and then unary operators bind tighter than the binary
operators.

Parentheses win arguments:

```elf
result := (2 + 3) * 4
inside := value >= minimum && value < maximum
chosen := preferred ?? fallback
```

`&&` and `||` short-circuit and produce `0` or `1`. `??` returns its left
operand unless it is `nil`; only then is the right operand evaluated.

Unary operators are small and sharp:

```elf
negative := -value
positive := +value
flipped := ~bits
```

## Choices

The `?` points from a condition to its body.

```elf
if score > 100 ? {
	elf.println("legendary")
}
elif score > 50 ? {
	elf.println("getting warm")
}
else {
	elf.println("keep going")
}
```

One statement needs no braces:

```elf
if ready ? launch()
```

## Loops

Count with a half-open range:

```elf
for i := 0 ... 3 ? {
	elf.println(i) // 0, 1, 2
}
```

Walk all or part of a table:

```elf
values := {10, 20, 30, 40}

for value := values[...] ? elf.println(value)
for value := values[1 ... 3] ? elf.println(value)
for value := values[2 ...] ? elf.println(value)
for value := values[... 2] ? elf.println(value)
```

Take several values per lap:

```elf
for x, y := 0 ... 6 ? {
	elf.println(x, ", ", y)
}
```

Bring your own setup, test, and update:

```elf
sum := 0
for i := 0; i < 10; i += 1 ? {
	if i == 3 ? continue
	if i == 8 ? break
	sum += i
}
```

Or keep looping while something is true:

```elf
while queue:length() > 0 ? {
	visit(queue:pop())
}
```

Ranges are half-open. A backwards numeric range currently runs zero times.

## Functions

Functions are values.

```elf
add := fun(a, b) {
	ret a + b
}

double := fun(value) ret value * 2
answer := add(20, 22)
```

`=>` is not currently part of the grammar. The concise implemented form is
`fun(value) ret expression`.

The final parameter may be a variadic marker:

```elf
draw := fun(sprite, layer, ...) {
	elf.println(sprite, " on layer ", layer)
}
```

Parameter annotations (`name: expression`) and defaults (`name = expression`)
are accepted by the parser, but the compiler does not enforce or apply them
yet. Do not rely on either form.

`recurse` calls the function currently running:

```elf
factorial := fun(n) {
	if n <= 1 ? ret 1
	ret n * recurse(n - 1)
}
```

Return nothing, one thing, or several things:

```elf
ret
ret answer
ret left, right
```

Calls can wear a table without parentheses:

```elf
task := make_task {
	name = "compile",
	workers = 4,
}
```

Method calls pass the value as `this`:

```elf
items:add("new")

handler := {
	call = fun(value) {
		ret value
	},
}
handler.call("new")
```

The `:` form reads a metatable field. The `.` form reads a field directly from
the value. Both forms pass the left-hand value as `this` when called.

## Cleanup later

`defer` saves a statement for the way out.

```elf
file := open("notes.txt")
defer close(file)

defer {
	flush(file)
	close(file)
}
```

## Loading Elf

```elf
module := elf.load_file("modules/colors.elf")
answer := elf.load_file("modules/answer.elf", 41)
```

Loaded files can return values like functions.

## JSON without ceremony

```elf
config := json {
	"name": "elf",
	"debug": true,
	"window": [1280, 720],
	"optional": null
}
```

Objects and arrays become tables. `null` becomes `nil`.

## Syntax summary

This is a compact description of the implemented parser. It is descriptive,
not a promise that every accepted edge case will remain part of Elf.

```text
file             := statement*

statement        := block
                  | "defer" statement
                  | "ret" tuple?
                  | "break" tuple?
                  | "continue" tuple?
                  | while
                  | for
                  | if
                  | statement_expression

block            := "{" statement* "}"
while            := "while" expression "?" statement
if               := "if" expression "?" statement
                    ("elif" expression "?" statement)*
                    ("else" statement)?

for              := "for" identifiers (":=" | "::=") tuple
                    ("?" statement
                    | ";" expression ";" statement_expression "?" statement)

statement_expression
                  := tuple statement_tail?

statement_tail   := ":=" tuple
                  | "::=" tuple
                  | "=" tuple
                  | "?=" tuple
                  | compound_assign tuple

compound_assign  := "+=" | "-=" | "*=" | "/=" | "%="
                  | "^=" | "<<=" | ">>="

tuple            := expression ("," expression)*
identifiers      := identifier ("," identifier)*

function         := "fun" "(" parameters? ")" statement
parameters       := parameter ("," parameter)* ("," "...")?
                  | "..."
parameter        := identifier (":" expression)? ("=" expression)?

table            := "{" (table_entry ","?)* "}"
table_entry      := identifier "=" expression
                  | "[" expression "]" "=" expression
                  | expression ("=" expression)?

postfix          := primary postfix_part*
postfix_part     := "." identifier
                  | ".[" expression "]"
                  | ".(" identifiers ")"
                  | "[" index ("," index)* "]"
                  | ":" identifier
                  | "(" arguments? ")"
                  | table

arguments        := expression ("," expression)*

index            := expression
                  | expression? "..." expression?

primary          := literal
                  | identifier
                  | table
                  | function
                  | "recurse"
                  | "json" json_value
                  | "(" expression ")"
                  | "#get_mem" "(" expression ")"

literal          := "nil" | "true" | "false"
                  | integer | number | character | string | format_string
```

Postfix chains must remain on the same line. A table immediately following a
callable expression is a one-argument call; `make { name = "Ada" }` is
equivalent to `make({ name = "Ada" })`.

An operator at the end of a line continues the expression onto the next line.
An operator beginning a new line starts a new statement because the preceding
expression was already complete.

Operator precedence is defined by the table in
[Math and comparisons](#math-and-comparisons). Ranges are accepted only where
the compiler expects them: numeric range loops and ranged table iteration.

## Partial and reserved syntax

`#get_mem(value)` exposes compiler memory IR for implementation work.
`#line_number` becomes the current source line and `#file_name` becomes the
current source name.

The following syntax is recognized but is not usable language functionality:

- `!!` parses as a nil-aware binary operator but is not lowered.
- `...` marks a variadic function or a range. A standalone `...` expression is
  parsed but is not lowered as a variadic value.
- Parameter annotations and default expressions are stored in the AST but
  ignored by lowering.
- Values following `break` and `continue` are stored in the AST but ignored by
  lowering.
- `try`, `catch`, `finally`, `enum`, `global`, `default`, `do`, and `then` are
  reserved words without statement implementations.
- `->`, `::`, `--`, and unary `!` are tokenized but not parsed into usable
  expressions or statements.

These are sketches, not promises.
