# Changelog

This file tracks language features and other changes visible to elf users. Internal compiler and runtime refactors are intentionally left out.

## Unreleased

### Language

- Added trailing closure calls: `run -> { ... }` passes a zero-parameter function, while `items.each -> (item) { ... }` declares closure parameters. An existing call may appear on the left to retain ordinary arguments.
- Added `export name :: expression` for exposing selected static declarations through an imported source namespace. Static declarations are private to their source unless explicitly exported.
- Added the unary measure operator, `*value`. It returns the byte length of strings and buffers, the element count of arrays, the entry count of tables, and `0` for `nil`.
- Added default parameter values for static functions.
- Added named arguments when calling a statically known function. Positional arguments must come first, and named arguments must follow parameter order.
- Removed backtick strings and triple-quoted string blocks. Strings now use `"..."` exclusively, with `\n` for embedded newlines and `f"..."` for interpolation.
- Removed implicit table calls such as `render {width = 20}`. Use `render({width = 20})` instead.

### Standard library

- Added `elf.json.load_jsonl(path)`, which loads a JSON Lines file into an array containing one decoded value per line.
