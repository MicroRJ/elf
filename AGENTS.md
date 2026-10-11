o
strategy. Measure performance-sensitive changes with the benchmark target.

Several larger ideas have been discussed but are not approved implementation
work merely because they appear here: fibers/async networking, vector values,
compile-time block wrappers, named arguments, and public module namespaces.
Treat them as possible future directions and discuss their semantics and scope
before changing the language or VM for them.

## Current Architecture

`elf_State` is the complete runtime state, not merely the VM. It owns runtime
storage, GC state, interned strings, globals, references, metatables, loaded
runtime modules, the value stack, call frames, and VM counters.

`src/core/core.c` is intentionally a unity translation unit for the runtime.
The optimized build currently uses `/O2` without LTCG, so splitting hot runtime
code into separate translation units can remove useful inlining. `vm.c` owns the
bytecode interpreter plus all call and frame machinery; there is deliberately
no separate `call.c`. `core.c` owns runtime creation/destruction and assembles
the runtime implementation. Do not split the VM into its own translation unit
without explicit approval and before/after benchmarks.

`src/core/helpers.h` currently contains shared, cross-type runtime operations
that need complete runtime types and should inline into consumers. For example,
`value_length` supports tables, strings, and buffers; string length is a byte
length. Keep representation predicates and conversions in `value/value.h`, and
avoid duplicating cross-type semantics independently in the VM and public API.

Error is a distinct value tag whose payload is a pointer to a table containing
structured error information. It is not merely an ordinary table, even though
the table supplies its storage.

The compiler owns an array of `Com_Module` records. Module index zero is the
root module, and `SourceSite.module` identifies the source module. Module names
and canonical paths are atoms. The loader uses `Com_ModuleLoadParams` for both
inputs and outputs. Plain-string imports are recursively discovered, loaded,
deduplicated by canonical path, and parsed. Only the root AST is currently
lowered and emitted; imported-module namespaces, exports, linking, and runtime
module behavior are unfinished. Do not mistake parsing imported files for a
complete module system.

`name :: expression` is a compile-time/static declaration. Static declarations
are collected and resolved per lexical scope during lowering, so references may
precede declarations within that scope and recursive static functions work.
Static functions cannot capture runtime locals. `name ::= expression` is a
runtime immutable binding and is deliberately distinct from `::`. Static
declarations remain in their normal AST blocks; do not reintroduce separate AST
static arrays or whole-tree rescanning without a demonstrated need.

## Verification

Use `.\build.bat test` for the public-header, core, compiler, VM, API, and smoke
tests. Also run `.\build.bat optimized` for runtime/compiler changes. Use
`.\build.bat benchmark` when making performance claims or changing hot paths.

## Documentation

Do not create, edit, delete, regenerate, or otherwise maintain documentation
unless the user explicitly requests documentation work in the current task.

This includes documentation directories, README files, API guides, website
content, examples maintained primarily as documentation, and changelogs. Do
not update documentation merely because a code change makes it stale; leave
that work to the repository's documentation agent.

## Commit Boundaries

An agent may develop, test, and fix one coherent change without interrupting
the work to request a commit.

Before beginning a new related feature, refactor, or other distinct follow-up
while completed relevant work is still uncommitted, the agent must stop and
explicitly warn the user that the existing work should be committed first.
The warning must identify what remains uncommitted. Do not silently accumulate
multiple completed changes in the working tree.

Check the working tree before starting substantial implementation work. If
relevant changes are already underway in the subsystem the agent is about to
modify, warn the user before editing anything. Briefly identify the overlapping
files or feature and whether the existing work appears incomplete or ready for
a commit. Do not assume ownership of, overwrite, or silently build on those
changes.

Ignore unrelated user changes, preserve them, and do not repeatedly warn about
them. The commit warning applies when the uncommitted changes overlap the
subsystem or sequence of work the agent is about to continue.

Never create a commit unless the user has explicitly authorized committing.
If the user chooses to continue without committing, proceed as directed, but
warn again before crossing the next coherent change boundary.

## Scope Boundaries

Do not change compiler internals, language syntax or semantics, parsing,
lowering, bytecode, the VM instruction set, or other unrelated subsystems
merely because a requested feature exposes a limitation in them. Stop before
making such a change, explain the limitation and the alternatives, and obtain
the user's explicit approval for that additional scope.

An earlier design discussion or agreement about API usage does not authorize
implementation changes outside the subsystem the user asked to modify. If the
agreed API cannot be implemented as written without broader changes, return to
the user before proceeding. Do not silently reinterpret the request as
permission to complete prerequisite language or architectural work.

## Code Style

Always use braces for `if` statements and `for` loops, including single-line
bodies.

Keep parenthesized conditions on one physical line. Never format an `if` like
this:

```c
if (expr_tuple->tuple.nargs == 1 &&
	expr_tuple->tuple.args[0]->kind == AST_CALL)
```

Prefer a long condition on one line. If that becomes unreasonable, simplify
the control flow by splitting the `if` or extract parts of the condition into
clearly named local variables instead of wrapping the parenthesized expression
across lines.

Keep function declarations and definitions, including their parameter lists,
on one physical line. Prefer a long line over splitting a signature across
multiple lines. Apply the same rule to function calls and their argument lists;
for example, keep `BcSlot first = emit_expr(gen, stat->ir_multi_local.expr, NO_MEMORY, locals.count);` on one physical line. As a general rule, keep
syntactically connected code on the same line rather than introducing line breaks inside it.

Prefer explicit, structured control flow over scattered early exits. Use clear
`if`/`else` structure and an obvious exit path instead of spreading `return` or
`break` statements throughout a function or loop.
