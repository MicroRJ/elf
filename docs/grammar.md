# elf Language Grammar

This document describes the syntax accepted by the current elf parser. It is
both a grammar reference and a guide to the behavior represented by that
grammar. The language is still evolving; lexer-only and reserved syntax is
listed at the end rather than presented as implemented language syntax.

## Notation

The grammar uses a small EBNF notation:

```text
"text"   literal source text
item?    zero or one item
item*    zero or more items
item+    one or more items
a | b    either a or b
```

Names such as `identifier`, `integer`, and `string` represent lexer tokens.
Whitespace generally separates tokens. Both `// line comments` and
`/* block comments */` are ignored by the lexer.

## Source Files and Statements

```ebnf
file              ::= statement* eof
block             ::= "{" statement* "}"

statement         ::= block
                    | expr_statement
                    | if_statement
                    | while_statement
                    | range_for
                    | c_for
                    | defer_statement
                    | return_statement
                    | break_statement
                    | continue_statement

if_statement      ::= "if" if_tail
if_tail           ::= expr "?" statement
                      (("elif" if_tail) | ("else" statement))?

while_statement   ::= "while" expr "?" statement
defer_statement   ::= "defer" statement
return_statement  ::= "ret" tuple_expr?
break_statement   ::= "break" expr?
continue_statement ::= "continue" expr?
```

A block is a statement, but constructs that expect a statement do not require
a block. These are both valid:

```elf
if ready ? run()

if ready ? {
	run()
	log("finished")
}
```

The `?` separates the condition or loop header from its body.

### Conditionals

`elif` recursively introduces another condition. `else` takes any statement:

```elf
if score > 100 ? {
	ret "high"
}
elif score > 50 ? {
	ret "medium"
}
else {
	ret "low"
}
```

### While Loops

The predicate is evaluated before every iteration:

```elf
i := 0
while i < 10 ? {
	i += 1
}
```

### Return, Break, Continue, and Defer

`ret` is the return statement spelling. The old `-->` spelling is not part of
the current parser grammar. A return may carry a tuple of values:

```elf
ret
ret value
ret left, right
```

`break` exits the nearest loop and `continue` starts its next iteration. Both
currently accept an optional expression syntactically. In a C-style loop,
`continue` transfers control to the update clause before testing the predicate
again.

`defer` takes one statement. Use a block to defer several statements:

```elf
defer close(file)

defer {
	flush(file)
	close(file)
}
```

## Declarations, Assignments, and Tuples

```ebnf
expr_statement    ::= tuple_expr (assignment_op tuple_expr)?
assignment_op     ::= ":=" | "::=" | "=" | "?="
                    | "+=" | "-=" | "*=" | "/=" | "%="
                    | "^=" | "<<=" | ">>="

tuple_expr        ::= expr ("," expr)*
```

`:=` and `::=` declare names. They are accepted as separate tokens but
currently produce the same declaration behavior:

```elf
name := "elf"
left, right := 10, 20
```

`=` assigns, `?=` performs nil assignment, and the remaining forms are
compound assignments:

```elf
count = 10
cached ?= calculate()
count += 1
flags ^= mask
```

Assignment is parsed at the statement layer. It is not an expression that can
be embedded inside another expression.

Commas construct an `AST_TUPLE`. Parentheses only group a single expression;
they do not construct a tuple:

```elf
values := 1, 2, 3       // tuple expression
value := (1 + 2) * 3    // grouped expression
```

## Expressions

```ebnf
expr              ::= binary_expr
binary_expr       ::= postfix_expr (binary_op postfix_expr)*

postfix_expr      ::= unary_expr postfix*
postfix           ::= "." identifier
                    | "." "[" expr "]"
                    | "." "(" identifier ("," identifier)* ")"
                    | "[" index_expr ("," index_expr)* "]"
                    | ":" identifier
                    | call_args

index_expr        ::= expr
                    | expr? "..." expr?

call_args         ::= table_expr
                    | "(" ","? (expr ("," expr)*)? ")"

unary_expr        ::= identifier
                    | literal
                    | "..."
                    | "~" expr
                    | "-" expr
                    | "+" expr
                    | "load" load_args
                    | "json" json_value
                    | "recurse"
                    | "#get_mem" "(" expr ")"
                    | table_expr
                    | "(" expr ")"
                    | function_expr
```

Postfix operations may be chained:

```elf
world.players[0].name
object:method(argument)
make_config {debug = true}
```

The table form after a callable is call shorthand, so `make_config { ... }`
passes one table argument.

### Fields and Indexes

The postfix forms distinguish array indexes, hash fields, and metafields:

```elf
table[index]       // array/storage index
table.name         // hash field using the atom "name"
table.[expr]       // hash field using a computed key
table.(a, b, c)    // tuple containing table.a, table.b, table.c
value:length()     // lookup a metafield and call it
```

Comma-separated array indexes are chained rather than combined into a tuple:

```elf
matrix[row, column]
// Equivalent AST shape to matrix[row][column].
```

### Operator Precedence

From highest to lowest precedence:

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

Operators on the same level currently associate from left to right. The
ellipsis operator constructs a half-open range; it does not eagerly produce a
runtime collection.

## For Loops

The left side of both `for` forms is an identifier list rather than a general
tuple expression:

```ebnf
identifier_list   ::= identifier ("," identifier)*
for_bind          ::= ":=" | "::="

range_for         ::= "for" identifier_list for_bind tuple_expr
                      "?" statement

c_for             ::= "for" identifier_list for_bind tuple_expr
                      ";" expr
                      ";" expr_statement
                      "?" statement
```

Consequently, headers such as `for 1 := ...` and `for a + b := ...` are parser
errors.

### Range Loops

A range loop has no semicolons:

```elf
for i := 0 ... 10 ? {
	log(i)
}
```

Its right-hand tuple must contain exactly one expression, which must be either
a numeric range or an indexed range. Numeric ranges are half-open: `0 ... 10`
visits `0` through `9`. Equal endpoints produce zero iterations, and a start
greater than the end currently also produces zero iterations.

An indexed range visits values from a half-open portion of a collection:

```elf
values := {5, 7, 11, 13, 17}

for value := values[1 ... 4] ? {
	log(value) // 7, 11, 13
}
```

The collection and explicit endpoints are evaluated once before iteration.
Indexed ranges may omit either endpoint. A missing lower bound becomes zero; a
missing upper bound becomes the cached collection's length:

```elf
for value := values[...] ? {}       // 0 ... values:length()
for value := values[2 ...] ? {}     // 2 ... values:length()
for value := values[... 3] ? {}     // 0 ... 3
```

Omitted endpoints are currently supported for indexed ranges, not standalone
numeric ranges. Range indexes are consumed by range loops; they are not general
slice values.

Multiple loop names consume consecutive positions and advance by the name
count:

```elf
for x, y, z := 0 ... 9 ? {
	// (0, 1, 2), (3, 4, 5), (6, 7, 8)
}
```

Current limitation: if the range length is not divisible by the number of loop
names, the final iteration can extend beyond the upper bound. Indexed ranges
can therefore attempt an out-of-bounds access in that final partial group.

### C-Style Loops

A C-style loop has a declaration, predicate, and update:

```elf
sum := 0
for i := 0; i < 10; i += 1 ? {
	sum += i
}
```

The initializer executes once and declares the loop names. The predicate is
checked before each iteration. The update runs after the body and after a
`continue`.

Unlike C, the current grammar does not permit omitted initializer, predicate,
or update clauses.

## Tables

Tables are elf's primary compound value:

```ebnf
table_expr        ::= "{" table_entry* "}"
table_entry       ::= identifier "=" expr ","?
                    | "[" expr "]" "=" expr ","?
                    | expr ("=" expr)? ","?
```

A plain value becomes an array-style entry. Identifier keys and computed keys
create hash-style fields:

```elf
numbers := {10, 20, 30}

player := {
	name = "Ada",
	score = 100,
	[dynamic_key] = dynamic_value,
}
```

Commas are accepted but not required between entries when token boundaries are
otherwise clear. Using commas is recommended for compact tables.

## Functions and Calls

```ebnf
function_expr     ::= "fun" "(" params? ")" statement
params            ::= param ("," param)* ("," variadic)?
                    | variadic
param             ::= identifier (":" expr)? ("=" expr)?
variadic          ::= "..."
```

Functions are expressions and may use any statement as their body:

```elf
add := fun(a, b) {
	ret a + b
}

identity := fun(value) ret value
```

Parameter syntax reserves optional type and default expressions:

```elf
draw := fun(sprite: Sprite, layer = 0, ...) {
	// ... must be the final parameter.
}
```

`recurse` refers to the currently executing function without requiring its
declared name:

```elf
factorial := fun(n) {
	if n <= 1 ? ret 1
	ret n * recurse(n - 1)
}
```

`load` is a core loading expression and accepts the same parenthesized or table
argument shapes as calls, plus a direct expression:

```ebnf
load_args         ::= table_expr
                    | "(" ","? (expr ("," expr)*)? ")"
                    | expr
```

```elf
module := load "path/to/module.elf"
```

`#get_mem(expr)` is a compiler intrinsic for obtaining the memory IR associated
with a local or parameter. It is primarily an implementation-facing feature.

## Literals and Strings

```ebnf
literal           ::= "nil" | "true" | "false"
                    | integer | number | character | string

integer           ::= decimal_integer
                    | "0b" binary_digits
                    | "0x" hex_digits
number            ::= decimal_integer "." decimal_digits
character         ::= "'" escaped_codepoint "'"

string            ::= quoted_string
                    | string_block
                    | formatted_string
                    | formatted_string_block

quoted_string     ::= '"' escaped_codepoint* '"'
string_block      ::= '"""' escaped_codepoint* '"""'
formatted_string ::= 'f"' formatted_part* '"'
formatted_string_block ::= 'f"""' formatted_part* '"""'
formatted_part   ::= escaped_codepoint | interpolation
interpolation    ::= "${" expr "}"
```

Single-line strings cannot contain raw newlines. Triple-quoted string blocks
can:

```elf
message := "hello"

paragraph := """first line
second line"""
```

Interpolation is explicit through the `f` prefix. Each `${expr}` is evaluated
and concatenated with the surrounding text:

```elf
name := "Ada"
count := 3
message := f"hello ${name}; count = ${count}"
```

Formatted blocks support the same interpolation syntax:

```elf
report := f"""user: ${name}
items: ${count}"""
```

Escapes include the common C-style escapes as well as `\xNN`, `\uNNNN`, and
`\U00NNNNNN`.

## JSON Literals

`json` parses JSON syntax directly into elf's table/value AST:

```ebnf
json_value        ::= json_object
                    | json_array
                    | string
                    | integer
                    | number
                    | "-" integer
                    | "-" number
                    | "true"
                    | "false"
                    | "null"

json_object       ::= "{" (string ":" json_value
                      ("," string ":" json_value)*)? "}"
json_array        ::= "[" (json_value ("," json_value)*)? "]"
```

```elf
config := json {
	"name": "elf",
	"debug": true,
	"sizes": [320, 480]
}
```

JSON `null` becomes elf `nil`; JSON arrays and objects become tables.

## Lexer-Only and Reserved Syntax

The lexer recognizes several words and tokens that the parser does not yet
accept as implemented language constructs. These currently include:

```text
try catch finally do then enum global default
-> ++ -- !
```

The lexer also recognizes implementation-facing macro names beyond `#get_mem`.
Their presence in the token set does not make them stable language grammar.
