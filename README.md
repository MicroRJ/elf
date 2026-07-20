# elf

`elf` is a small, minimal, C-Like scripting language.

The project is in active development, and it is not meant for
production use.

See ```docs\``` for an overview of the technical aspects.

```
sum := 0
count := 0
saw_end := 0

for i := 0 ... 24 ? {
	sum += i
	count += 1
	if i == 24 ? {
		saw_end = 1
	}
}

make_adder := fun(base) {
	ret fun(value) {
		ret base + value
	}
}

add_10 := make_adder(10)

```

## Build

The repository includes a Windows x64 Bob bootstrap executable. No separate
Bob installation is required:

```bat
build.bat
```

Arguments are passed through to Bob, so the test entry can be run with:

```bat
build.bat test
```

The bootstrap executable can also be invoked directly:

```bat
bootstrap\windows-x64\bob.exe
```

A compatible C compiler and linker are still required. If Bob is installed on
`PATH`, invoking `bob` directly continues to work.

## More

Syntax, internals, and APIs are still moving quickly while the language
architecture settles.


