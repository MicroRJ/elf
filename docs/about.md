# About elf

elf is a small, bytecode-interpreted scripting language designed to complement C.
It combines C-like control flow and zero-based indexing with a table-centered data
model, closures, and an intentionally small grammar.

The project is in active development. It already runs real build descriptions and
configuration programs, but its language, embedding API, and module model are not yet
stable enough for production use.

## Why elf exists

C is still my primary language. It is the right tool when I need direct control over
memory, data layout, the operating system, or the performance of a native subsystem.
It becomes less pleasant when the problem is dominated by dynamic data, strings,
configuration, automation, or logic that needs to change frequently. In those domains,
the implementation and compile-link cycle can become larger than the problem I am
trying to solve.

Scripting languages shorten that iteration cycle, but the existing options never felt
at home in the kind of C codebases I wanted to build. Python has a different surface
syntax, object model, runtime, and deployment footprint. Those are reasonable choices
for a general-purpose ecosystem, but they introduce a larger semantic and operational
shift than I want inside a modest native program.

Lua is much closer to the embedding model I want, but its surface grammar, operators,
one-based indexing, and treatment of array-like tables remain unfamiliar from a C
programmer's point of view. I wanted a language that a C programmer could read without
first changing how they parse ordinary control flow and indexed data.

elf targets the smaller intersection I actually need:

- familiar control flow and expressions;
- immediate source-to-bytecode compilation for small embedded programs;
- a compact native embedding boundary; and
- convenient handling of tables, strings, functions, and other dynamic values.

The choice is therefore not C or elf. The intended design is C and elf. C owns the host,
the runtime, and the systems that require direct control. elf handles the portions that
benefit from runtime flexibility and a shorter path from an idea to a working program.

## Design principles

### Prefer a small semantic surface

elf is not intended to accumulate a different construct for every programming style.
The language is built around a small collection of general values and operations:
tables hold data, functions hold behavior, closures preserve context, and native
functions cross into the host.

There is no class hierarchy or constructor protocol. Shared per-type metatables provide
library methods such as `items:add(value)`, but they are a lookup mechanism rather than
an object model that programs must adopt. User data can remain ordinary tables.

### Make common operations visually predictable

Mutable and constant bindings differ by one visible token:

```elf
count := 0
limit ::= 60
```

The language provides both high-level iteration and the control form familiar from C:

```elf
for value := values[...] ? {
	process(value)
}

for i := 0; i < values:length(); i += 1 ? {
	process(values[i])
}
```

Functions are values and use the same block structure:

```elf
make_adder := fun(base) {
	ret fun(value) {
		ret base + value
	}
}
```

The detailed syntax belongs in the [grammar guide](grammar.md). The design goal is not
to make every construct resemble C literally; it is to keep the rules small enough that
syntax can be understood locally and used consistently throughout the language.

## The table model

The central data structure in elf is the table. One table can act as an array, a map,
or an object without changing representation.

Internally, a table separates value storage from keyed lookup. Every stored value lives
in one contiguous array. A separate open-addressed hash table maps keys to indices in
that array; the hash entries do not contain another set of values.

An unkeyed literal creates ordinary array values:

```elf
numbers := {10, 20, 30}
numbers:add(40)
```

Named fields add values to the same storage and bind their keys to the corresponding
indices:

```elf
user := {}
user.first_name = "Jonathan"
user.last_name  = "Blow"
```

Conceptually, that table contains:

```text
values = ["Jonathan", "Blow"]

bindings = {
	"first_name" -> 0,
	"last_name"  -> 1,
}
```

Consequently, `user[0]` and `user.first_name` reach the same stored value through
different operations. Plain brackets address the contiguous value array by integer
index. A field access resolves a key binding; `user.first_name` is shorthand for
`user.["first_name"]`.

The value array remains contiguous as the table changes. Insertion and removal shift
the affected values and update any bindings whose indices moved. Programs can iterate
the values in storage order or separately inspect the table's keys and key-value pairs.

This representation is useful for the kind of programs elf currently serves. A build
task, configuration record, argument list, and result collection can all use the same
value type while retaining straightforward array iteration.

## Consistency as a design constraint

One small syntax change illustrates how I approach the language.

Early versions used brackets for keyed access, so `array[0]` meant "look up the key
`0`." Later, ranged iteration used brackets to describe a sequence of array values:

```elf
for value := array[...] ? {
	use(value)
}
```

The two meanings were individually understandable, but together they made brackets
mean two different kinds of access. That violated the rule the rest of the language was
starting to establish.

elf now reserves plain brackets for the array view and dot access for the keyed view:

```elf
array_value := value[0]
named_field := value.name
dynamic_key := value.[key]
```

This is not a large feature, but it is representative of the project. I would rather
change an existing design than preserve a local convenience that makes the language
harder to reason about as a whole.

## Compiler architecture

elf compiles source into bytecode before executing it. The pipeline has three explicit
compiler representations:

```text
source -> lexer and parser -> AST -> IR -> bytecode module -> VM
```

### Lexing and parsing

The lexer produces tokens on demand. The parser keeps the current and next token, plus
the previously consumed token, which gives the grammar the lookahead it needs without
first constructing a separate token array. Statements are parsed recursively, while
binary expressions use precedence climbing to preserve operator precedence and
associativity.

Parsing produces a complete abstract syntax tree. Every AST node retains its source
location so later stages can still report errors against the original program.
Identifiers and keywords are interned while parsing, allowing subsequent stages to
compare names by identity rather than repeatedly comparing strings.

### Lowering to IR

The AST describes the program as it was written. Lowering converts it into a smaller,
more explicit intermediate representation for code generation.

This stage tracks lexical scopes and declarations, rejects assignment to constant
bindings, resolves local and global references, discovers closure captures, and turns
structured control flow into blocks, labels, and jumps. It also emits deferred
statements at the points where their enclosing scopes exit.

Each source file becomes an IR module with an entry function and any nested functions
declared by the program. Nested functions record the values they must capture from
their enclosing scopes.

### Bytecode generation

The backend generates bytecode one function at a time. elf uses a slot-based execution
model: instructions name input and output slots in the current function frame instead
of pushing every intermediate value through an operand stack.

Code generation assigns temporary slots, calculates each function's required frame
size, emits relative branches, and patches their destinations after the labels are
known. The finalized in-memory module contains:

- one flat bytecode stream;
- function metadata describing bytecode ranges, arity, captures, and frame sizes;
- separate constant pools for integers, floating-point numbers, and strings; and
- source maps connecting bytecode ranges back to the original source.

Most compiler data is temporary. The AST, IR, and module builders live in scratch
storage and disappear after compilation. The runtime preserves the original source and
the finalized module in the owning `elf_State`, keeping constants, closures, and source
diagnostics valid for that state's lifetime.

### Execution

The module's entry function becomes a garbage-collected closure. The VM executes its
bytecode in slot-based stack frames and stores captured values directly on closures.
A closure retains a reference to the module that owns its function metadata and
bytecode.

elf closures and native C functions are both callable values. The VM can enter a native
function, let it inspect arguments and push results through the public API, and resume
bytecode execution without introducing a second language-level calling model.

## Embedding from C

The runtime builds as a static library and exposes its supported interface through
`include/elf.h`. A host creates an `elf_State`, compiles source, invokes functions, and
exchanges values through a stack API.

The stack serves two purposes. It provides one generic interface for numbers, strings,
tables, functions, and nested structures, and it keeps host-visible values reachable by
the garbage collector without exposing internal object pointers. Durable references
allow a host to retain a value after removing it from the stack.

Hosts can publish native functions and nested library tables, store arbitrary user data
on the state, inspect diagnostics, parse JSON or constant expressions, and move table
data in either direction. The complete current contract is documented in the
[C API guide](public-api.md).

## What works today

elf is currently version `0.2.0-dev`. It is not production-ready, but it is substantially
more than a syntax experiment.

Current programs can use:

- signed 64-bit integers, floating-point numbers, immutable strings, and `nil`;
- tables with contiguous values and keyed fields;
- mutable and constant bindings;
- arithmetic, comparison, logical, and bitwise operators;
- conditionals, C-style loops, `while`, and half-open ranged iteration;
- first-class functions, closures, recursion, variadic arguments, and multiple returns;
- deferred statements and interpolated strings; and
- source loading, constant-data parsing, and JSON conversion.

The core runtime installs table, string, math, diagnostic, conversion, metatable, and
readonly-data facilities. The optional batteries library adds source-file loading,
serialization, environment access, filesystem and path operations, process execution,
randomness, and time utilities. The repository also builds a command-line runner, a
bytecode inspection tool, static libraries, and benchmarks.

The test entry covers arenas, strings, tables, garbage collection, lexing, parsing, IR
lowering, bytecode generation, the VM, JSON, value serialization, the public C API, and
end-to-end programs exercising closures, loops, defers, interpolation, variadic calls,
and the bundled libraries. The current Windows x64 test suite passes with
`build.bat test`.

## Real usage

I have used elf in my own work for roughly three years. Its most substantial current
consumer is Manny, rather than a collection of isolated language examples.

Manny is the clearest example of the intended C-and-elf relationship. Manny's parallel task
scheduler, dependency tracking, and incremental build engine are written in C. Its build
descriptions are ordinary elf programs that construct task graphs from tables,
functions, loops, and strings.

Orbiter uses Manny and elf for its build description. It also stores its game-library
configuration as elf tables, replacing a separate configuration format with the same
language already used by the build. Broader runtime scripting in Orbiter and Apollo is
planned rather than complete.

These projects make elf useful today for build descriptions, configuration data, small
automation programs, and experiments embedded in native applications. They also expose
design problems that isolated language demos would not reveal: API ownership, native
error boundaries, filesystem behavior, module lifetime, and the ergonomics of moving
real nested data between C and a script.

## Current limitations

elf remains a development language with several important boundaries:

- The officially supported build is Windows x64, using the current native toolchain and
  platform layer.
- The language syntax and public C API can still change without compatibility support.
- Parameter annotations and default values parse successfully but are not enforced or
  applied by the compiler.
- Runtime failures do not yet have a protected-call boundary; they follow elf's fatal
  error path.
- Compiled modules live until their `elf_State` is destroyed. They cannot be unloaded
  independently or serialized as portable, versioned bytecode files.
- The VM has no JIT, and performance has not yet been benchmarked systematically against
  comparable scripting runtimes.
- Garbage collection is synchronous mark-and-sweep. It is simple and adequate for
  current programs, but collection pauses execution and has not been designed for large
  or latency-sensitive heaps.
- The standard library and third-party ecosystem are small, and there is no package or
  compatibility story yet.

The name also collides with the standard Executable and Linkable Format acronym. I have
kept it because the project has carried the name for years and it still fits the
language.

## Direction

The near-term goal is not to turn elf into a replacement for every scripting language.
It is to make the existing C-and-elf boundary dependable: explicit modules that can be
loaded and released, protected runtime errors, meaningful type contracts, measured
performance, and continued use in real native programs.

If elf succeeds, it will remain modest. Its value is not the number of features it can
accumulate, but how little machinery a C programmer needs to gain a practical dynamic
language.

## Further reading

- [elf by example](grammar.md)
- [C API](public-api.md)
- [Standard library](libraries/README.md)
