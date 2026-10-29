# Development Ledger

## 2026-10-29

- Tightened runtime truthiness: `nil` remains false and distinct from zero, and numeric values are rejected as branch conditions. Added script coverage for equality and truthiness variants.
- Changed compiled functions to push prototypes directly instead of creating unnecessary closures.
- Attached originating sources to errors and linked the sources belonging to a compilation.
- Consolidated source loading and contracted compiler helper names and dead structures.
- Added entity buckets so multiple declarations can share a name, establishing the storage needed for overloads.
- Introduced basic compiler type values and initial checker type propagation.
- Represented injected symbols as ordinary static entities and removed the separate global-entity concept.
- Fixed array and filesystem operations whose required stack pops previously disappeared from optimized builds because they were embedded in `ASSERT` expressions.
- Made each function lowering operation use its own `IrEmitter` state.
- Added `IrFunction.parent` as the closure-environment hierarchy. Entry and static functions are independent roots; nested closures point to their enclosing IR function.
- Normalized IR emitter helper names into `ir_emit_*`, `ir_queue_*`, `ir_loop_*`, and `ir_defer_*` families.
- Made every `IR_LOCAL` record its owning `IrFunction`. Capture resolution now follows IR-local ownership and the IR parent tree instead of comparing AST functions.
- Deferred sharing the temporary IR stack across function emitters. The current implementation remains simple and correct but allocates a 4096-entry stack per function.
- Verified all 173 tests and the optimized build.
