# Strings

Strings are immutable byte strings. Their methods use `:`.

## Measure and cut

```elf
"hello":length()       // 5
"hello":size()         // same as length
"hello":byte(0)        // 104
"hello":byte(-1)       // 111
"abcdef":slice(1, 4)  // "bcd"
"abcdef":slice(3)     // "def"
```

Indexes count bytes. Negative indexes count from the end. Slice ends are
exclusive.

## Find things

```elf
path := "build/main.obj"

path:starts_with("build/") // 1
path:ends_with(".obj")     // 1
path:contains("main")      // 1
path:find("main")          // 6
path:find("main", 7)       // nil
```

`find` returns a zero-based byte index or `nil`.

Tiny glob patterns are handy for files:

```elf
"src/main.c":glob_match("*.c")
"cat":glob_match("c?t")
"green":glob_match("red|green|blue")
```

`*` matches any run, `?` matches one byte, and `|` separates alternatives.

## Split and join

```elf
parts := "a,b,c":split(",")
lines := "first\r\nsecond\nthird":lines()

csv := ",":join({"a", "b", "c"})
path := "/":join({"src", "core", "vm.c"})
```

`split` keeps empty pieces. `lines` understands `\n`, `\r`, and `\r\n`.
`join` requires a table of strings.

## Polish

```elf
"  hello \n":trim()                  // "hello"
"Elf":lower()                        // "elf"
"Elf":upper()                        // "ELF"
"one two one":replace("one", "1") // "1 two 1"
"ha":repeat(3)                       // "hahaha"
```

Case conversion is ASCII-only. `replace` changes every non-overlapping match.
The target for `replace` and separator for `split` cannot be empty.

## Complete list

```text
length size byte slice starts_with ends_with contains glob_match find
split lines trim lower upper replace repeat join
```
