# Elf by example

Elf is still growing!

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
letter := 'E'
text := "hello"
```

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

Escapes include `\n`, `\r`, `\t`, `\\`, `\"`, `\xNN`, `\uNNNN`, and
`\UNNNNNNNN`.

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
}

key := "favorite"
player.[key] = "compiler"
```

Read them in the same shapes:

```elf
first := numbers[0]
name := player.name
favorite := player.[key]
name, score := player.(name, score)
```

Indexes can chain:

```elf
cell := matrix[row, column] // matrix[row][column]
```

Table methods live behind `:`:

```elf
numbers:add(40)
last := numbers:pop()
```

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
&& !!
|| ??
...
```

Parentheses win arguments:

```elf
result := (2 + 3) * 4
inside := value >= minimum && value < maximum
chosen := preferred ?? fallback
```

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

Parameters can carry a rule, a default, or a variadic tail:

```elf
draw := fun(sprite: Sprite, layer = 0, ...) {
	elf.println(sprite, " on layer ", layer)
}
```

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
```

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
module := load "modules/colors.elf"
answer := load("modules/answer.elf", 41)
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

## A few odd tools

`...` by itself is the variadic value expression. `#get_mem(value)` exposes
compiler memory IR for implementation work. Neither should be treated as
settled design yet.

The lexer also knows some words the parser does not use yet, including `try`,
`catch`, `finally`, `enum`, and `global`. They are sketches, not promises.
