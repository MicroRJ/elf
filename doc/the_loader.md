

### The Loader

The loader is the native program
that is charge of loading elf
code.


* For the sake of clarity, whenever
I say 'load' I'm referring to the
process of loading the elf file from
disk and or generating bytecode from
elf code and running that bytecode.


If you've embedded elf into your
application, then you are effectively
what I refer to as the loader.

The loader has complete access over
elf and its inner workings... since
you can literally modify the source
code.

Consequently, the loader may also
provide a different feature set
or extend the elven one.

### the elven loader

elf.c is the loader that comes
by default with elf and supports
executing elf files for several
platforms including the web.


Note that the 'elf.' feature set
is not some additional file that
requires linking against, but it
is implicitly available since it
it is used internally by the elf
runtime code, so no matter which
loader you're using, 'elf.' will
always be available, unless the
source is directly modified, in
which case we can't do anything
about that.


All elf source files are platform
agnostic and the loader can
optionally provide functions
for platform dependent endpoints.


When you compile elf.c for a
desktop platform you can then
run on the command line:

'elf.exe hello.elf' and it will
load that file.

By default if no file is specified
'launch.elf' is automatically looked
for.

So if you want to use elf.c for the
web ensure that you pre-load a file
named 'launch.elf' so that you don't
have to modify the loader itself and
recompile it to look for the file
you want.
Otherwise this could turn into a very
tedious process since emcc can be
quite slow...

In launch.elf you can then load the
file you actually want using
'elf.loadfile' or 'load'.

Alternatively you can tell emcc
(the compiler used for targeting the web)
to not call main and call main yourself
from JS.

There's also the option of building
elf-web.c, which simply exposes some
functions made specifically for the
web.
In other words, it's a wrapper around
the elf API that's easy to use from
the web.


### the elven feature set

elf already comes with a pretty minimal
feature set, mostly elf related functions
but also some platform abstractions.

For instance, elf comes a system file
which compiles for the target platform
and does the job of exposing a standard
platform API.

These functions are used internally
but elf exposes all of them to the
end user as well.

So you can open,read,write files
and do many more things directly
from elf.

This platform layer is very minimal
though, so you will have to rely on
some other loader or library for
more specific or intricate stuff.


It is incredibly easy to embed
elf into your game or project
since elf already provides all
of the functions required to
load and execute elf itself.


Even if you wanted to process
command line arguments like
elf.c does, there's a cli file
that can help you do so.


You can check out elf.c to see
how simple the elf loader is and
modify it to fit your needs.


### Writing A New Loader Isn't
### Always Needed

Sometimes is best to just write
a loader that automatically links
in whatever feature set yo want.

For instance if you're targeting the web,
it is simpler, faster, less error prone,
and much more convenient for the whole
module to be compiled into a single
executable.

For instance, the 'elf-ray' loader
provides bindings for raylib.

The bindings are in ray.c, this file
can be compiled into a dll.

Then you can call 'elf.loadlib' to
load this dll and either call the
'elf_raylib_lib_load_functions' function which links in
all the symbols, or import what you
need manually.

Now this would be pretty tedius to
do for the web, you'd have to compile
a side module and do a bunch of silly
stuff for dlopen to work, which is how
the dll is loaded.

So instead, elf-ray offers a loader that
automatically links in the symbols from
elf.ray, and runs your elf code.

You can also modify this loader to fit
your needs better.

Now you can simply compile elf-ray once
and forget about ever compiling it again,
unless required...