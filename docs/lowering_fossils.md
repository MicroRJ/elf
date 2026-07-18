# Lowering Fossils

These are notes from old disabled lowering/codegen blocks that were removed from
the active source during cleanup. They are not current code, but the ideas may be
useful when re-implementing the features properly.

## Range For Lowering

Old direction: rewrite a range-style `for` into init, predicate, post-step, and a
`while` body.

```c
// Conceptual sketch, not live code.
range_lo = range->binary.x ? range->binary.x : int(0);
range_hi = range->binary.y ? range->binary.y : int(UINT_MAX);

init = assign(index, range_lo);
pred = less_than(index, range_hi);
post = assign(index, add(index, nvalues));
true_body = block(body, post);
loop = while(pred, true_body);
main = block(init, loop);
```

Open questions:
- Should range upper bounds be inclusive or exclusive?
- Should open-ended ranges lower to an infinite loop plus explicit break?
- How should multi-value `for` bindings advance?

## Function Lowering

Old direction: lower `AST_FUNCTION` into a generated function entry with implicit
`this`, optional variadic flag, parameter locals, and a lowered body.

```c
arity = IMPLICIT_PARAM_COUNT + nparams;
if (last_param_is_ellipsis) {
	tags |= FUNCTION_VARIADIC;
	arity -= 1;
}

function = add_generated_function(tags, arity);
declare_directory("elf");
declare_parameter("this");
declare_each_named_parameter();
function->body = lower_block(body);
```

Open questions:
- Should function bodies lower in their own entity scope or function scope object?
- Should captures be detected during lowering or during a separate closure pass?
- Should generated functions reference AST nodes, Ir nodes, or stable function IDs?

## Tuple Expression Lowering

Old direction: tuple expressions wanted to lower to tuple Ir for multi-return and
multi-assignment support, but no stable `IR_TUPLE` exists yet.

Open questions:
- Is tuple Ir a real runtime value or just a lowering-time multi-value list?
- Should `ret a, b` become one `IR_RETURN` with an array of values?
- Should assignment lowering own arity matching, nil-fill, and ellipsis expansion?

## Loop Control

Old code had placeholders for `for`, `break`, and `continue` lowering.
The current active path lowers `while` directly to `IR_LOOP`.

Recommended future shape:
- Add `IR_BREAK` and `IR_CONTINUE` as real control-flow Ir.
- Lower range `for` either to canonical loop Ir or through a dedicated desugar pass
  that still uses `AstContext` instead of a dummy parser.

## JSON Parser

JSON parsing should stay out of the language parser. The intended home is:

```text
src/compiler/frontend/parse_json.c
```

That file should parse JSON text into runtime values/tables when re-enabled,
without adding JSON-specific branches back into `parse.c`.
