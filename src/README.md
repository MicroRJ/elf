This Document Is No Longer Valid

### Welcome to elf, the programming language.


### Quick Usage:
To embed elf simply include elf.h, see elf.c
(the compiler) for an example.


### Please Note The Following:
This project is unreleased, meaning that
it is not meant for public consumption,
yet... aka use at your own risk.

There are many incomplete features, bugs,
and spurious code sprinkled here and there.


This page (elf) contains only the files that
pertain to the elf programming language and
the basic runtime itself which is under the
elf namespace and those necessary to build
the compiler and run elf code.


There are no dependencies and the code
is meant to be platform agnostic (eventually).

If you wish to use elf alongside other
libraries or runtimes such as raylib,
you can head over to their independent
repos listed here:

https://github.com/MicroRJ/elf-ray


### About

Unlike many other languages, which are in their
proper right to be as they wish, elf is meant
to be a whimsical programming language,
that attempts to recapture the magic and fun
of programming.

That being said, this project is also ideal for
those interested in a minimalist, embeddable
scripting language, written in vanilla C.



### Features
elf has pretty standard and minimalist syntax,
ideal for getting stuff done quickly.

```
table = {1,2,3}
// call builtin pf, printf
pf(table) \\ {1,2,3}

add3 = fun(x,y,z) ? (x + y + z)
add3(1,2,3)
```

We have fairly primitive support for
the typical stuff you'd expect from
a dynamically typed language.


### Building


You should be able to build or integrate
elf with pretty much every compiler or
tool-chain there is.

elf adopts a simplistic unit build approach,
consolidating all source files into a single
header file for compilation.
This unified header simplifies integration.

You can also build elf as a shared or
static library.


### TODO:
- Incremental GC
- Tracing JIT!
- Multi threading!
