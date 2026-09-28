# TODO

This is the single project backlog. Top-level checkboxes describe outcomes; the
indented bullets record scope and acceptance criteria. Keep implementation notes
here instead of scattering `TODO` comments through the source.

## Release readiness

- [x] Make state destruction complete and leak-free.
  - Free every GC object, including table entry and array storage.
  - Free both GC reference arrays, the interned-string bucket array, diagnostic
    storage, and the arena.
  - Add a repeated create/use/destroy test and destroy every state created by the
    test suite and command-line tools.

- [ ] Add a protected runtime-call boundary.
  - Runtime failures in elf code or native functions must unwind to the host
    instead of terminating the process.
  - Return a status and a durable diagnostic, while restoring the stack and call
    frames to a documented state.
  - Cover failures raised by bytecode, core libraries, and `elf_error()`.

- [ ] Turn user-controlled limits into diagnostics rather than assertions or
  process aborts.
  - Reject source and strings that do not fit the runtime's 32-bit size fields.
  - Handle limits on constants, functions, captures, stack slots, jump distance,
    labels, jump patches, scopes/entities, and interpolation nesting.
  - Replace fixed compiler arrays where a practical dynamic representation exists;
    otherwise report the limit precisely and test it.

- [ ] Stabilize the public C API before documenting it as a contract.
  - Make `elf.h` self-contained by including the header that defines `size_t`.
  - Make borrowed string data const and document every pointer's ownership and
    lifetime.
  - Use `elf_Bool` only for Boolean results, `elf_ErrorCode` for fallible
    mutations, and add an explicit invalid value type for failed lookups.
  - Decide whether `elf_Ref` should remain an integer handle or become a stronger
    public type.
  - Define the compatibility meaning of `ELF_API_VERSION`, or remove it until a
    stable contract exists.
  - Review names, result types, failure behavior, and stack effects as one API;
    compile-test the public headers without internal headers.

- [ ] Define and enforce the supported language surface.
  - Audit every keyword, token, parsed AST form, lowering case, and bytecode path.
  - Implement or reject reserved-but-unused keywords, parameter annotations and
    defaults, unlowered operators/expressions, incomplete multi-return behavior,
    and values following `break` or `continue`.
  - Decide whether named field access on strings is supported; do not silently
    return `nil` for a parsed operation with no implementation.
  - Add success or rejection tests before describing a construct in the grammar.

- [ ] Remove the `platform` submodule dependency.
  - Add the required environment, process, process-ID, exit, and OS-error services
    to elf's OS layer.
  - Migrate the environment and process batteries, tests, and tools.
  - Remove platform includes, library/build tasks, and the submodule after the last
    caller is gone.

- [ ] Establish reproducible debug and release builds.
  - Build core, batteries, and the CLI in both configurations.
  - Ensure release artifacts are optimized and contain no `_DEBUG` behavior.
  - Add a strict-warning configuration and fix actionable warnings.
  - Add Windows CI that builds both configurations and runs the tests.

- [ ] Define and verify the distributable package.
  - Package the public headers, core library, optional batteries, CLI, and license.
  - Hide or prefix undocumented external symbols in static libraries.
  - Validate the package with a standalone embedding example rather than internal
    include paths.

- [ ] Make expected test diagnostics unambiguous.
  - Parser and lexer rejection tests must not look like test-runner failures.
  - Suppress expected diagnostics or label them clearly when their text is part of
    the test.

## Documentation pass

- [ ] Finish the public documentation after the release-readiness contracts settle.
  - Add one complete C example that creates a state, compiles and calls code, reads
    results, handles failure, and destroys the state.
  - Document the path, process, serialization, random, and time batteries; the
    environment battery already has a page.
  - Correct the obsolete 65,535-byte filesystem/string limit.
  - Replace the `printl` example in `main.elf` with `println`; retain compatibility
    documentation only where the alias is intentionally discussed.
  - Choose and apply one capitalization style for the project name, then manually
    verify README and documentation examples against the tested behavior.

## Engineering follow-ups

- [ ] Separate immutable IR from bytecode-generation state.
  - Move bytecode labels and assigned local slots out of IR nodes if IR reuse,
    inspection, or concurrent compilation is a real requirement.
  - Do this as one ownership change rather than moving the two fields separately.

- [ ] Simplify frontend collection and interpolation state.
  - Return and use `AstArray` consistently instead of passing separate pointer/count
    pairs where practical.
  - Revisit whether formatted-string modes belong in the lexer, and replace the
    fixed nesting stack or give it a checked limit.

- [ ] Clarify internal module boundaries.
  - Move atom-to-runtime-string conversion out of the atom-table implementation.
  - Replace `elf_Binding` only as part of a clearer native-library registration
    boundary, not as a standalone rename.

- [ ] Measure representation changes before committing to them.
  - Compare eager and lazy string hashing with realistic string sizes.
  - Measure the cost and benefit of caching live table-entry counts.
  - Profile `StackFrame` and `Bytecode` size before compacting either structure.

- [ ] Centralize temporary and host allocation policy.
  - Use scratch storage for short-lived filesystem buffers where its lifetime is
    sufficient.
  - Add configurable host allocation only with explicit ownership, failure, and
    alignment rules across arenas, GC objects, tables, strings, and batteries.

## Future capabilities

- [ ] Support independent module unloading.
- [ ] Add versioned bytecode serialization.
- [ ] Add comparative performance benchmarks.
- [ ] Support additional platforms.
- [ ] Decide whether user-object field overloading belongs in the language.
- [ ] Decide whether a JIT would serve elf's actual use cases.
