# elf Grammar

This is the current parser grammar, not the final language design. The lexer
still reserves a few old or future-facing words and tokens; this file only
documents paths the parser currently accepts.

## Source

```ebnf
file          ::= statement* eof
block         ::= "{" statement* "}"

statement     ::= block
                | "defer" statement
                | "ret" tuple_expr?
                | "break" expr?
                | "continue" expr?
                | "while" expr "?" statement
                | range_for
                | c_for
                | "if" if_tail
                | expr_statement

identifier_list ::= identifier ("," identifier)*
for_bind      ::= ":=" | "::="
range_for     ::= "for" identifier_list for_bind tuple_expr "?" statement
c_for         ::= "for" identifier_list for_bind tuple_expr
                  ";" expr ";" expr_statement "?" statement
if_tail       ::= expr "?" statement (("elif" if_tail) | ("else" statement))?
```

`ret` is the return statement spelling. The old `-->` spelling and the long
`function` keyword are no longer part of the parser grammar.

## Expressions

```ebnf
expr_statement ::= tuple_expr (assignment_op tuple_expr)?
assignment_op   ::= ":=" | "::=" | "=" | "?="
                  | "+=" | "-=" | "*=" | "/=" | "%="
                  | "^=" | "<<=" | ">>="

tuple_expr      ::= expr ("," expr)*

expr            ::= binary_expr
binary_expr     ::= postfix_expr (binary_op binary_expr)*

postfix_expr    ::= unary_expr postfix*
postfix         ::= "." identifier
                  | "." "[" expr "]"
                  | "." "(" identifier ("," identifier)* ")"
                  | "[" expr ("," expr)* "]"
                  | ":" identifier
                  | call_args

call_args       ::= table_expr
                  | "(" ","? (expr ("," expr)*)? ")"

unary_expr      ::= "..."
                  | identifier
                  | "nil"
                  | "true"
                  | "false"
                  | integer
                  | number
                  | character
                  | string
                  | "~" expr
                  | "-" expr
                  | "+" expr
                  | "load" load_args
                  | "json" json_value
                  | "recurse"
                  | "#get_mem" "(" expr ")"
                  | table_expr
                  | "(" expr? ")"
                  | function_expr

load_args       ::= table_expr
                  | "(" ","? (expr ("," expr)*)? ")"
                  | expr

function_expr   ::= "fun" "(" params? ")" statement
params          ::= param ("," param)* ("," variadic)?
                  | variadic
param           ::= identifier (":" expr)? ("=" expr)?
variadic        ::= "..."
```

The variadic marker must be the final function parameter.

## Tables

```ebnf
table_expr      ::= "{" table_entry* "}"
table_entry     ::= identifier "=" expr ","?
                  | "[" expr "]" "=" expr ","?
                  | expr ("=" expr)? ","?
```

An entry without an explicit key becomes an array-style entry later.

Expression indexing distinguishes array storage from hash fields:

```text
table[index]       // array/storage index
table.name         // hash field by identifier
table.[expr]       // hash field by computed key
table.(a, b, c)    // field projection tuple: table.a, table.b, table.c
```

## For Loops

The left side of a `for` header is an identifier list, not a general tuple
expression. This makes invalid headers such as `for 1 := ...` and
`for a + b := ...` parser errors instead of deferring that validation to
lowering.

There are two distinct `for` forms. A range loop has no semicolons:

```text
for i := 0 ... 24 ? { ... }          // range step
for i := values[0 ... 24] ? { ... }  // array range step
```

Its right-hand tuple must contain exactly one expression, and that expression
must be a range or ranged index. The parser represents this form as an
`AST_FOR_RANGE`.

A C-style loop has an initializer, predicate, and update separated by
semicolons:

```text
for i := 0; i < 10; i += 1 ? { ... }
```

The initializer declares the loop identifiers from its right-hand tuple. The
predicate is checked before every iteration. The update runs after the body;
`continue` also transfers control to the update. The parser represents this
form as an `AST_FOR`.

Numeric ranges are half-open. For a multi-name range declaration, each name
receives an offset from the same iterator, and the iterator advances by the
number of names:

```text
for i, j, k := 0 ... 24 ? { ... }
```

An indexed range such as `values[first ... last]` traverses that half-open
slice. The collection and its endpoints are evaluated once before iteration.
The loop body may be any statement, including a block.

## JSON

`json` parses a JSON value directly into the same table/value AST used by elf
constant expressions.

```ebnf
json_value      ::= json_object
                  | json_array
                  | string
                  | integer
                  | number
                  | "-" integer
                  | "-" number
                  | "true"
                  | "false"
                  | "null"

json_object     ::= "{" (string ":" json_value ("," string ":" json_value)*)? "}"
json_array      ::= "[" (json_value ("," json_value)*)? "]"
```

## Literals

```ebnf
integer         ::= decimal_integer | "0b" binary_digits | "0x" hex_digits
number          ::= integer "." decimal_digits
character       ::= "'" escaped_codepoint "'"
string          ::= quoted_string | string_block | formatted_string | formatted_string_block
quoted_string   ::= '"' escaped_codepoint* '"'
string_block    ::= '"""' escaped_codepoint* '"""'
formatted_string       ::= 'f"' formatted_part* '"'
formatted_string_block ::= 'f"""' formatted_part* '"""'
formatted_part         ::= escaped_codepoint | interpolation
interpolation          ::= "${" expression "}"
```

Single-line strings cannot contain raw newlines. String blocks can contain raw
newlines. Formatted strings evaluate each `${expression}` and concatenate its
text representation. Escapes currently include the C-style escapes, `\xNN`,
`\uNNNN`, and `\U00NNNNNN`.

## Operators

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

Assignment operators are parsed at the statement layer, not as arbitrary binary
expressions.

## Lexer-Only Or Reserved

The lexer still recognizes some tokens that are not accepted by the parser as
language grammar yet, including `try`, `catch`, `finally`, `do`, `enum`,
`global`, `default`, `->`, `++`, `--`, and `!`.
