# elf

elf is a small, bytecode-interpreted scripting language designed to complement C.
It combines familiar control flow and zero-based indexing with table-centered data,
closures, and a compact native embedding API.

elf is currently `0.2.0-dev`. The language is useful in my own projects, but its syntax,
API, and module model are still evolving. It should be treated as experimental rather
than production-ready.

```elf
numbers := {10, 20, 30}

make_adder := fun(base) {
	ret fun(value) {
		ret base + value
	}
}

add_ten := make_adder(10)

for number := numbers[...] ? {
	print(add_ten(number))
}
```

## Why elf?

C remains the host language. elf handles the parts of a native program that benefit
from dynamic values and short iteration cycles: configuration, build descriptions,
automation, strings, and nested data.

The project currently provides:

- a lexer and parser with source-located diagnostics;
- AST-to-IR lowering and a bytecode backend;
- a slot-based virtual machine with first-class functions and closures;
- one table representation for array, map, and record-like data;
- a stack-based C API for embedding and native functions; and
- optional filesystem, path, process, environment, serialization, random, and time
  libraries.

Manny uses elf as its build-description language, and Orbiter uses elf programs for its
build and game-library configuration.

## Documentation

- [Project overview and design](docs/about.md)
- [Language guide](docs/grammar.md)
- [C embedding API](docs/public-api.md)
- [Standard libraries](docs/libraries/README.md)

## Building

The supported build environment is Windows x64. You need:

- Git;
- `clang-cl`; and
- Microsoft's `lib.exe` librarian.

Clone the repository with its platform-layer submodule:

```bat
git clone --recursive https://github.com/MicroRJ/elf.git
cd elf
```

If the repository is already cloned, initialize the submodule with:

```bat
git submodule update --init --recursive
```

The repository includes a Windows x64 Manny bootstrap executable, so Manny does not need to
be installed separately.

```bat
build.bat build
build.bat test
```

Additional entries build the static libraries or run optimized benchmarks:

```bat
build.bat lib
build.bat benchmark
```

Build products are written beneath `build/`. Passing no entry to `build.bat` uses Manny's
default entry.

## Repository layout

- `include/` — public C headers
- `src/compiler/` — lexer, parser, AST, IR, and bytecode generation
- `src/core/` — runtime values, tables, strings, VM, GC, and core libraries
- `src/batteries/` — optional host-facing libraries
- `test/` — C unit tests and end-to-end elf programs
- `tools/` — runner, bytecode inspector, and benchmarks

## License

elf is available under the [MIT License](LICENSE).


