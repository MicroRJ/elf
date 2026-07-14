# elf

`elf` is a small scripting language and runtime for experiments, prototypes,
games, and media tools.

The project is in active recovery and redesign. The current compiler pipeline is:

```text
source -> lexer -> AST -> IR -> bytecode -> VM
```

## Current Shape

- Hand-written lexer and parser.
- AST lowering into an explicit IR.
- Bytecode generation from IR.
- Stack-based runtime with bytecode closures.
- Tables as the primary compound data structure.
- Interned atoms for identifiers and string-like immutable text.
- Mark/sweep GC for tables, closures, and atoms.
- Core libraries under `src/libs`.
- Smoke tests and focused C tests under `tools` and `smoke`.

## Build

On Windows, initialize the Visual Studio environment and run:

```bat
vcvars64
build.bat
```

Build output is written to `build`.

The build currently produces:

- `build\elf.lib`
- `build\tests.exe`
- `build\benchmarks.exe`
- `build\bytecode.exe`
- `build\elf.exe`

## Test

Run the full test harness with:

```bat
build\tests.exe
```

Run a script file with:

```bat
build\elf.exe path\to\file.elf
```

If no file is provided, `build\elf.exe` runs `main.elf`.

Dump bytecode for a script with:

```bat
build\bytecode.exe path\to\file.elf
```

Run table lookup benchmarks with:

```bat
build\benchmarks.exe
```

## Layout

```text
include/     public headers
src/base/    internal base utilities
src/core/    state, GC, values, atoms, tables, diagnostics
src/compiler frontend, IR lowering, bytecode generation
src/vm/      calls and bytecode dispatch
src/libs/    built-in libraries
src/platform platform-specific helpers
tools/       test harnesses, runner, bytecode dumper, benchmarks
smoke/       elf scripts used by smoke tests
docs/        grammar and design notes worth keeping
```

## Notes

This is not stable software yet. Syntax, internals, and APIs are still moving
quickly while the language architecture settles.
