## Verification

Use `.\build.bat test` for the public-header, core, compiler, VM, API, and smoke
tests. Also run `.\build.bat optimized` for runtime/compiler changes. Use
`.\build.bat benchmark` when making performance claims or changing hot paths.

For language and runtime behavior that can be asserted from Elf, add a `.elf`
file directly under `test/scripts`. The runner in `test/script_tests.c`
automatically discovers every `.elf` file in that directory, installs
`test_assert`, compiles the script, and executes it. Use `test_assert` for the
expected behavior; no C registration function or sentinel return value is
needed.

Do not hesitate to add assertion scripts. New language or runtime features,
bug fixes, suspected regressions, and behavior that is uncertain during an
investigation should normally gain focused coverage under `test/scripts` when
the behavior can be expressed from Elf. Keep useful diagnostic scripts as
regression tests instead of discarding them after the immediate issue is
understood.

Keep scripts under `test/compiler/smoke` when a C test must inspect the returned
value or runtime state directly, and register those explicitly in
`test/compiler/vm_tests.c`. Add lower-level parser, checker, IR, backend, core,
or API coverage to the corresponding C test suite.

## Documentation

Do not create, edit, delete, regenerate, or otherwise maintain documentation
unless the user explicitly requests documentation work in the current task.

This includes documentation directories, README files, API guides, website
content, examples maintained primarily as documentation, and changelogs. Do
not update documentation merely because a code change makes it stale; leave
that work to the repository's documentation agent.

## Architecture Direction

Runtime and core must never depend on the compiler. The compiler backend may
target runtime objects directly, but the frontend and middle stages remain
runtime-independent: lexer, parser, checker, and IR lowering operate on
compiler-owned representations. Runtime knowledge begins at the bytecode
emitter.

The bytecode emitter directly creates and initializes self-contained
`elf_Prototype` objects. Do not reintroduce `BcModule`, a target abstraction,
or an intermediate packaged module merely to create a conceptually clean
boundary. `elf_State` owns the growable prototype table and runtime sources.
Each prototype owns its bytecode, constants, runtime string references, source
map, and source reference.

Compiler sources may reference runtime-owned `elf_Source` objects, while
parsing, checking, and lowering continue using compiler representations such
as atoms, AST nodes, entities, and IR nodes. Preserve the one-way dependency:
compiler backend to core/runtime, never core/runtime to compiler.

Avoid copying source, bytecode, constants, or other compiled data through
additional temporary module representations without a concrete need.
Architectural boundaries should reduce work and clarify ownership, not create
APIs solely for conceptual purity.

## Error Propagation

New code, especially runtime and core code, must report failure through an
explicit return path and let callers return or recover gracefully. Never call
`report_runtime_error` and rely on its current process exit to prevent the
function from continuing. Treat `report_runtime_error` and
`abort_after_runtime_error` as deprecated legacy control flow, not as patterns
to extend. When this pattern is encountered in existing code, point it out to
the user and account for it in the proposed work instead of silently copying
it.

Keep failure reporting separate from failure detection. Low-level containers
and helpers return a status without logging or terminating; an owning boundary
turns that status into a compiler diagnostic, runtime error value, VM error, or
public API error exactly once. Callers must check and propagate failure before
using outputs or continuing mutation. Use `ASSERT` only for impossible internal
invariants, never for invalid input, allocation failure, or another recoverable
runtime condition.

Use the result convention belonging to the layer:

- Compiler status-only operations return `Com_Status`. Compiler functions whose
  primary result is a token, AST, entity, value, or IR node use that type's
  existing error representation and preserve the underlying status where the
  representation supports it.
- Internal runtime and core operations return `ErrorCode`, with `ERROR_NONE` on
  success. Return payloads through out parameters when a function needs both a
  value and a status. Mutating operations that can fail must not return `void`.
- Public ELF API operations return `elf_ErrorCode`, with `ELF_ERROR_NONE` on
  success. Translate an internal `ErrorCode` at the public boundary rather than
  exposing compiler or internal error types.
- Use `b32` or `elf_Bool` only for predicates, lookups, and probes where `false`
  is an ordinary, unambiguous outcome and callers do not need an error reason.
  Do not use a boolean as a lossy substitute for a meaningful failure code.
- An `ELF_FUNCTION` return value is the number of values returned to Elf, not a
  status code. Recoverable script-facing failures should push an `elf_Error`
  value and return it, following the filesystem library pattern. VM execution
  failures must use a graceful VM error-propagation path; if that path does not
  yet exist for the requested work, stop and agree on it rather than adding a
  new crash-dependent call to `report_runtime_error`.

Initialize out parameters to a safe value before work begins when practical,
document whether they are valid on failure, and keep status checks close to the
call that produced them. Do not introduce a generic result wrapper when one of
the layer-specific conventions above already represents the operation clearly.

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

Use the project's string types for string data. Use `dy_String` for ordinary
borrowed text and names inside the implementation, `elf_StrSlice` at public ELF
API boundaries, `Atom *` for interned compiler names and paths, `Com_String` for
strings owned by compiled output, and `elf_String *` for runtime strings. Do not
introduce `const char *` parameters or fields for ordinary string data, especially
when the implementation immediately calls `strlen`. Reserve raw C strings for
string literals, format strings, and direct CRT, platform, or external C API
boundaries that specifically require null termination; convert at that boundary.

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
