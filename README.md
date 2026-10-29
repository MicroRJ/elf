# elf


### TODAY's TODO's
-


### ROADMAP

- Unified error diagnostics.

	Currently elf sort of just reports and prints errors internally, this is simply
	wrong, especially for an embeddable language.

	We're already moving towards an unified error object.

	Whenever some error happens, the VM creates an error object, captures whatever
	information is necessary, stores it in that error and return it to the host.

	The problem is that not every path does this, for instance, the compiler - which
	is really just a subsystem of the runtime collects its diagnostic messages,
	attaches each to each corresponding source, and then prints each.

	Instead, it should return some sort of error as well, and attach the diagnostics
	to it.

	Then, we'll ship a unified error diagnostics API function, that takes that error
	and actually prints it.

	Or, you can take the error and print it yourself.

	Because errors are catchable within the script itself, and reference-able by
	GC objects, they must also realistically be GC objects themselves ...

- Better C Integration

	Writing bindings for elf, is terrible, we copied the same Lua like design and it's
	just painful.

	The direction that I want to head in is:

	Have out the box (elf -> C) interfacing through JIT.

	This removes, parameter validation from C and parameter extraction.

	Just signature validation and parameter extraction alone consumes most of the code in some
	of the bindings and it is a pain in the ass to maintain and produce good errors for, especially
	at compile time.

	For objects, instead of going through the C stack API, we're gonna expose the same direct table
	"API" that we use internally, same for other objects.

	I don't think the stack model makes anything easier or safer when you factor in its own complications,
	people complain about how awkward Lua's API is regardless.

	The idea is, you borrow an object, that object is never GC'd when borrowed, you manipulate it and do
	whatever you want to it, then you return it.

	Whether this reduces GC optimization opportunities in the future, or makes it harder ...
	Not sure ...

	But if that ever becomes the case, we figure out the solution.

	So the current C embedding API should be considered provisional.



- Static type resolution.

	Currently elf is completely dynamic, the compiler has no idea what the type
	of something is at all.

	I want to move away from this, I've had my fun with it but it doesn't scale
	well.

	We don't need a full on type system.

	All we need is basic type information for a given value.

	Adding this, will immediately replace "meta-tables" which is I brought over
	from Lua.

	We'll retain the same syntax, but instead it'll be sugar coating for this:

```c
	foo:length() == length(foo)
```

	We'll rely on static function signatures to find the matching length overload.

	That is, the user can write and define their own array functions as you normally
	would and reuse the same convenient syntax if you wish.

	And this will happen entire at compile time instead of resulting in a table
	extraction, string lookup and dynamic call.

	For length specifically, we'll have a length intrinsic instruction, since array
	is a builtin VM object.

```c

	length :: inline fun(array: arr) => __arr_size(array)

```

	So meta-tables are super deprecated!

