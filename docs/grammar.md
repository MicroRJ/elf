# Elf Grammar

This is the current parser grammar, not the final language design.

## Source

```ebnf
file        ::= statement* eof
block       ::= "{" statement* "}"
statement   ::= block
              | "defer" statement
              | ("ret" | "-->") tuple_expr?
              | "break" expr?
              | "continue" expr?
              | "while" expr "?" statement
              | "for" tuple_expr (":=" | "::=") semicolon_expr "?" statement
              | "if" if_tail
              | expr_statement

if_tail     ::= expr "?" statement (("elif" if_tail) | ("else" statement))?
```

## Expressions

```ebnf
expr_statement ::= tuple_expr ((":=" | "::=" | "=" | "?=") tuple_expr)?
tuple_expr     ::= expr ("," expr)*
semicolon_expr ::= tuple_expr (";" tuple_expr)*
expr           ::= binary_expr
binary_expr    ::= postfix_expr (binary_op binary_expr)*
postfix_expr   ::= unary_expr postfix*
postfix        ::= "." identifier
                 | "." "[" expr "]"
                 | "." "(" identifier ("," identifier)* ")"
                 | "[" expr ("," expr)* "]"
                 | ":" identifier
                 | call_args
call_args      ::= table_expr
                 | "(" ","? (expr ("," expr)*)? ")"
                 | expr
unary_expr     ::= "..."
                 | identifier
                 | "nil"
                 | "true"
                 | "false"
                 | integer
                 | number
                 | string
                 | "~" expr
                 | "-" expr
                 | "+" expr
                 | "load" call_args
                 | table_expr
                 | "(" expr? ")"
                 | function_expr
function_expr  ::= ("fun" | "function") "(" params? ")" statement
params         ::= param ("," param)*
param          ::= "..." | identifier (":" expr)? ("=" expr)?
```

## Tables

```ebnf
table_expr  ::= "{" table_entry* "}"
table_entry ::= identifier "=" expr ","?
              | "[" expr "]" "=" expr ","?
              | expr ("=" expr)? ","?
```

An entry without an explicit key is stored with a null key in the AST and becomes
an array-style entry later.

Expression indexing distinguishes array storage from hash fields:

```text
table[index]       // array/storage index
table.name         // hash field by identifier
table.[expr]       // hash field by computed key
```

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

## Deferred

JSON parsing is intentionally separate from the language parser. When it comes
back, it should live in `src/compiler/frontend/parse_json.c`.
