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

A batch file is provided that builds elf.

```bat
build.bat
```
You must have the developer environment setup.

If you have 'Bob', just do:

```bat
bob
```

## More

Syntax, internals, and APIs are still moving quickly while the language
architecture settles.


