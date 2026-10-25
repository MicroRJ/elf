# Source Organization Direction

This document records the intended organization of `src` while the codebase is
being simplified. It is a guide for refactoring decisions, not a request to
create a directory hierarchy. Keep `src` flat unless a directory would group a
real, stable unit with several members.

The main goal is to organize code around concrete input-to-output pipelines and
a small number of build units. A file should exist because it owns a useful
stage, representation, object, or library surface. Do not preserve a split only
because the code once lived in separate subsystems.

## Vocabulary

These terms describe a file's primary role. `fragment` describes how a file is
built and may be combined with another role.

| Role | Meaning |
| --- | --- |
| `base` | General C support that is not characteristic of Elf and could sensibly be reused elsewhere. |
| `core` | Elf-specific representations or rules shared across major stages. Bytecode definitions are the clearest example. |
| `runtime` | Execution state, VM behavior, garbage collection, runtime values, and services used while code runs. |
| `compilation` | The source-to-prototype pipeline: lexing, parsing, checking, IR lowering, and bytecode emission. |
| `library` | C functions and symbol tables exposed to Elf programs. Library fragments use the `l_` prefix. |
| `object` | Fundamental operations for one runtime object type, such as strings, arrays, tables, or buffers. This is normally a runtime fragment. |
| `state` | Creation, composition, mutation, or destruction of the central `elf_State`. Use this role narrowly. |
| `fragment` | A file included by an owning build unit rather than compiled independently. A fragment should have one clear owner. |
| `deprecated` | Vestigial behavior or structure that should be removed after its callers and compatibility requirements are understood. |

Do not force every role into a separate layer. For example, an object file can
also be a runtime fragment, and `elf.c` is both state composition and the public
construction boundary.

## Dependency Direction

The compilation pipeline is:

```text
source -> lexer -> parser -> checker -> IR lowering -> bytecode emitter
       -> self-contained elf_Prototype -> runtime VM
```

The lexer, parser, checker, and IR lowering stages operate on compiler-owned
representations. Runtime knowledge begins in the bytecode emitter, which writes
runtime-owned strings, sources, constants, source maps, and bytecode directly
into `elf_Prototype` objects.

The required dependency direction is:

```text
compiler backend -> core/runtime
libraries        -> runtime
elf.c            -> runtime + library installers
runtime/core     -X-> compiler
```

Do not add a packaged bytecode module, target abstraction, or other intermediate
representation between the emitter and `elf_Prototype`. Do not move
compiler-backed features into the runtime or ordinary library unit merely to
make their names look uniform.

## Current Build Units

Only files listed as library sources in `project.elf` are independent build
units. Included `.c` files are intentional unity-build fragments.

### `elf.c`: state composition

`elf_create_state` creates the runtime state, installs the core and host
libraries, and installs compiler-backed libraries. Installer declarations live
here because this is their sole consumer. Do not recreate single-consumer
installer headers.

### `elf_runtime.c`: runtime

This unit owns state storage and lifetime, allocation, runtime objects, GC,
runtime diagnostics, VM execution, and the public API implementation. Its main
fragments are:

- State and services: `elf_memory.c`, `elf_native_function.c`,
  `elf_symbols.c`, and `elf_source.c`.
- Objects: `elf_string.c`, `elf_buffer.c`, `elf_user_object.c`,
  `elf_array.c`, and `elf_table.c`.
- Execution: `elf_gc.c`, `elf_vm.c`, and `elf_api.c`.
- Runtime presentation and diagnostics: `elf_value_text.c`, `error.c`,
  `logging.c`, and `source_diagnostics.c`.

`elf_runtime.h` is a private cross-unit runtime contract. It is deliberately
available to the library and compiler backend units; it is not part of the
public API under `include`. `elf_runtime_helpers.h` is a header fragment owned
by that contract.

### `libs.c`: runtime-facing libraries

This unit assembles the core value libraries and host libraries. Files named
`l_*.c` expose native functions or construct symbol tables for Elf programs.
They are fragments, not independent modules.

Core libraries cover language and runtime values. Host libraries cover process,
filesystem, path, environment, randomness, time, and console I/O. Keeping both
sets in one build unit is intentional; the distinction matters during symbol
installation, not as a C linkage boundary.

### `elf_com.c`: compilation

This unit owns the source-to-`elf_Prototype` pipeline and includes:

- `atom.c`, `lexer.c`, and `parser.c`.
- `entity.c` and `checker.c`.
- `ir_emitter.c`, which owns the `ir.c` representation fragment.
- `bc_emitter.c`, where runtime knowledge begins.
- Compiler-backed library code currently held in `lib_com.c`.

The representation headers `compiler.h`, `token.h`, `ast.h`, `entity.h`, and
`ir.h` remain useful even though the compiler is a unity build. They define
contracts shared by several compiler stages. Do not inline them merely to lower
the file count.

### `bytecode.c`: current standalone support unit

`bytecode.h` is a real core contract shared by the compiler and runtime. The
implementation in `bytecode.c`, however, formats runtime prototypes and is used
by runtime diagnostics, tests, and tools. Its status as a separate build unit
should be reconsidered; see the work queue below.

## File and Header Rules

- Prefer a few explicit build units with included implementation fragments.
- Group by an operation or pipeline, not by an abstract noun or historical
  subsystem.
- Add a header when separate build units need a contract. Do not add a header
  just to declare functions used by one composition file.
- A header used physically by one unity unit can still be meaningful when
  several fragments depend on the representation it defines.
- Keep public declarations in `include/elf.h`. Files under `src` are private,
  even when they are headers.
- Preserve the one-way compiler-to-runtime dependency. If a move would make
  runtime code call compiler code, keep the feature in `elf_com.c` or revisit
  the feature boundary.
- Do not mix semantic changes with file moves and naming cleanups. The VM error
  model is still evolving and should be changed in focused commits.

## Completed Organization Work

- Flattened the former hierarchical source layout.
- Consolidated compiler stages under the `elf_com.c` unit build.
- Made the bytecode emitter construct `elf_Prototype` objects directly.
- Moved runtime and library composition out of the former monolithic core
  entrypoint and into `elf.c`, `elf_runtime.c`, and `libs.c`.
- Removed the single-consumer `libs.h` and `lib_com.h` installer headers.
- Renamed the former `elf_core` runtime unit and its test suite to `runtime`.
- Kept `core` terminology for actual Elf-wide representations and the core
  library exposed to programs.

## Ordered Work Queue

Treat each item as a separate change and commit boundary.

1. **Reassess `bytecode.c` as a build unit.** `bytecode.h` must remain the
   shared instruction contract. The implementation is runtime-aware formatting
   and can likely become a fragment of `elf_runtime.c`, reducing the library to
   four top-level C units.
2. **Remove the dead `elf.const_expr` surface.** `l_core.c` currently publishes
   a stub that always returns `nil`. Confirm that compatibility is not required,
   then remove the binding and its unused helper.
3. **Give `lib_com.c` a concrete owner and name.** Most of the file is the JSON
   string/value pipeline and should become `l_json.c`, still owned by
   `elf_com.c` because parsing uses compiler internals. Move `l_core_load_file`
   beside `elf_push_code_file`, since both belong to the source-file to compiled
   call pipeline.
4. **Review shared diagnostic fragments after the larger moves settle.**
   `logging.c` and `source_diagnostics.c` are included by both runtime and
   compiler units. Keep them as base fragments unless a concrete ownership or
   state problem justifies another compiled unit.
5. **Narrow `elf_runtime.h` only in response to real consumers.** It is broad,
   but splitting it speculatively would recreate the header and dependency
   friction this reorganization is removing.

Before starting an item, inspect the working tree and the current contents.
This queue records direction, not permission to overwrite concurrent work or
to cross the commit and scope boundaries in `AGENTS.md`.
