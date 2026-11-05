### Values

A value is the primary unit of data in elf, everything
that the elf virtual machine interacts with is stored
or wrapped in a value.

Value types include but are not limited to:

- nil: special value type meaning absence of

- int: 64 bit signed integer

- num: 64 bit floating point number

- nat: native elf function pointer

- str: references core string object

- err: references core error object
- arr: references core array object
- tab: references core table object
- fun: references core function object
- cls: references core closure object

- custom: references custom object

### Objects

A core object receives first class VM support,
these include but are not limited to:

- str: immutable interned string object
- err: error object
- arr: dynamic typeless array object
- tab: dynamic any to any map object
- fun: function object
- cls: closure object

### VM

The VM supports dynamic and direct dispatch.

```c

add(x: str, y: any) => add(x, str(y))
add(x: str, y: str) => x + y

add(x: int, y: num) => add(num(x), y)
add(x: num, y: int) => add(x, num(y))
add(x: int, y: int) => x + y
add(x: num, y: num) => x + y

sub(x: int, y: num) => sub(num(x), y)
sub(x: num, y: int) => sub(x, num(y))
sub(x: int, y: int) => x - y
sub(x: num, y: num) => x - y

div(x: int, y: num) => div(num(x), y)
div(x: num, y: int) => div(x, num(y))
div(x: int, y: int) => x / y
div(x: num, y: num) => x / y

mul(x: int, y: num) => mul(num(x), y)
mul(x: num, y: int) => mul(x, num(y))
mul(x: int, y: int) => x * y
mul(x: num, y: num) => x * y

```

### Strings

The string object is meant to store relatively small pieces of text.
All strings are interned on creation and are immutable thereon.

A string currently serves two purposes, byte storage as well as
content identity.

Some API's return a string object when a buffer is more suitable.
Buffers are currently not as standardized.

There currently isn't a practical way to address string and buffer
contents efficiently and conveniently.

For instance, one may want to split a string or buffer into lines.
One would have to implement the same functionality for both object
types.
Same goes for searching, patching, and any other operation.



### Functions

`elf` uses functions as the primary standalone unit of execution.

A function is a self-contained GC object.

It contains instructions and values.

A function may contain additional resources for
rich runtime diagnostics.

The contents of a function object are typically read-only.

### Closures

A closure allows you to capture locals implicitly.

```c
	x := 0

	fun() { y := x }

```

The slots where the captured values live are not reassignable.
Though this may change in the future.

If you wish to capture mutable state, do so with a wrapper
object, such as a table.

```c
	state := { x = 0 }

	fun() { state.x = 10 }

```

Attempting to reassign a capture results in a
compiler error.


### Declarations

A declaration creates an entity, associating a name with an entity-value (ev)
(not a VM value).

An ev may be a local value, or some other static value.

A declaration may be reassignable or not.

You can declare a local using `:=` or `::=` and a static using `::`.

Two colons `::` means not reassignable, a `=` means it's a
local otherwise it's static.

Local declarations are invisible to static declarations as a consequence
of being processed prior to locals.

```c
	x := 0

	// error: undeclared identifier 'x'
	FOO :: x
```

Same names still conflict because they are within the same scope, the local
FOO is blamed for redeclaring static 'FOO'.

```c
	FOO := 0

	FOO :: 10
```

A static value refers to any value that isn't a local. A value in this case
does not refer to a VM value, but instead a value sourced by the compiler.


### Evaluation

`elf` supports compile time execution.

Which allows you to execute code prior to generating the rest.

```c
	message :: elf.fs.read_text_file("hello_world_message.txt")

	elf.printl(message)
```

In the example above, `::` declares a `static` entity, forcing the compiler
to evaluate its expression prior to generating the rest of the of code.

Once the expression is evaluated, the result is cached and the code is
effectively discarded.

The final code only references the resulting values.

All static expressions across all imports are evaluated right at compilation time.

```c
	foo := fun() {
		read_something :: elf.fs.read_text_file("hello_world_message.txt")
	}
```



### Closures

A closure is created automatically when a function accesses locals from
the parent function.

Note that statically evaluated functions cannot capture runtime state.

Captured values are not reassignable.

```c



	// compile time error: x, y, z are undefined
	static_foo ::  fun() => print(x, ", ", y, ", ", z)
```


### This not It

One of the oldest quirks of elf, is the implicit this parameter, which is always there whether you want it or not,
and the entire calling convention relies on it. Every call introduces an additional instruction just to load the
current 'this' and add it to the call, so we needle that thing on forever.
It's more contextual than anything, the funny thing is, I've never literally used it, and we explicitly not use
it because it doesn't really add any benefits ...
So I think, like globals, 'this' should go.

### Need For Types

For larger projects, I find myself wanting more static features, even some form of basic value
classification, modules, records ... actual field checked records ...

I don't think I see elf remaining fully dynamic, per se ... I see a gradual shift in increased
awareness ...

Whether that turns into actual polymorphic type checking or just value classification, not
sure ...

I would lean towards basic value classification, it's plenty to get started and adds most
of the features I'd like.

What I am specifically looking for is features that help me structure large applications, I don't
think I need to know the actual data storage type of something for now.

Meta-tables are a hack, they come from the Lua days, lua had them, and I brought them, I've never
used them for anything other than `my_array:length()` actual type based functions.

I removed support for object meta-tables a while ago and the VM literally switches on the value
type to find the table corresponding to it.

But this is a hack, I've long realized that lua-like meta-tables are not a feature of elf, and I'll
be dropping them entirely.

We'll still preserve the syntax for now, because at this point, dropping support for it would be
catastrophic, but once we have statically known types, those lookups will no longer exist, it'll
just be a direct type checked call.

We'll see how tables interplay with records.
A record is essentially the equivalent of a C struct, the only difference is that it's an actual
object ... made up of values ...
But, we'll have statically known field offsets, fields will be packed, like arrays.

If records reduce table usage significantly, and as a consequence, '.' syntax on tables isn't as
prevalent, we'll drop the ':method()' syntax entirely, and then all type functions can just use '.'
directly ...

### Embed-ability

There is a lot of back and forth between elf and C, and that requires maintaining tons of glue code,
parameter checking along often time dwarfs basic operations, which is just ridiculous ...

So investing type in creating some sort of JIT engine to specifically create the glue to call a native
function would be paramount and one of the highest long investment yields, leading to possibly full JIT.

I've already done x64 machine code generation with Jasper, another elf like language but this one is fully
typed, and well ... very B like, though much better ...

Either way, I've relied on AI to create the binding code between elf and C, but I just know that if I had
to do that manually I'd probably not use elf at all, it's just the experience one should go through.

That being said, whether this makes elf harder to embed or not, not sure, the question is, do I really
care?

I think elf can still be embeddable, if someone what's to use the shitty Lua like API to link their stuff,
that's fine too ... but we gotta find a better solution that doesn't force anyone to go through this and
allows elf to statically check args at compile time.


### Trailing Closures

I rely heavily on closures, and trailing closures would one of the nicest ergonomic additions for one
my projects, this would have to rely on statically typed function definitions to make it safe, though
I have found a clever way to do so dynamically without ambiguity, it isn't as clean but it works I guess.


So typically, you'd do this:

```c
Vert(props, fun() {

})

What I propose is to allow for detached bodies when the function is the last argument:

```c
Vert(props, fun()) {

}
```

It reads a bit nicer, and the idea came because I kept accidentally typing that over and over again,
what's nice about this is that I can still specify the argument names to the detached function.

```c
Vert(props, fun(a, b, c)) {
	a, b, c = 1, 2, 3
}
```

What would be interesting is to see how this plays out with 'default' parameters,
we could do "explicit" captures ... if making captures explicit something we're
ever interested in.

```c
my_other_thing := 777

Vert(props, fun(a, b, c, copy = my_other_thing)) {
	a, b, c = 1, 2, 3
}
```


### The Error Rule

The return behavior is what makes error handling a feature I would say.

So we should note that error propagation is not property of failure itself,
but simply an emergent consequence of the error rule.

Sort of like playing hot potato, no one wants to keep it so they throw it
to the nearest person.

So we can, I guess more formally define that any expression can produce
either any number of non-error values, or one singular error value.

When an expression returns an <error> the caller or the receiver is the one
responsible for either keeping that error or raising it.

Since the VM is literally the one executing each instruction, by default,
it returns the result immediately to the parent frame, unless that
instruction allows for bypassing this behavior.


```c
	c := a + b
```
When the VM executes an 'add' it does this:

```c
	add(x: any, y: any):
		__add(x: int, y: int) -> x + y
		__add(x: num, y: num) -> x + y
		__add(x: int, y: num) -> __add(num(x), y)
		__add(x: num, y: int) -> __add(x, num(y))
		return error("invalid operands for add")

```

The error object itself is immutable, and at creation time captures execution state,
how much of that state I don't necessarily know yet, but it'd have to be enough to
reconstruct the call trace.


### The Error Of Errors

Currently we abort on whatever error we get, the problem with that is that it kills the host,
and has a bunch of nasty implications ...

So this was an early language design pattern that we used because it was simple ...

Instead, we need proper error handling, what should happen when the VM fails?

For instance, suppose the VM encounters '1 / 0', that's clearly an error the VM should signal,
the question is, how?

Do we stop execution entirely and return to the nearest native call?
Do we use long jumps to return to the nearest setup error handling point?

Based off of my experience with error handling models, and what people actually tend to prefer
and or complain about, I think there needs to be a better, uniform way to handle errors than
what something like Lua does.

The question isn't what constitutes an error of any kind, or how that error should be stored,
but what the propagation model is.

Suppose there existed a programming language that was entirely explicit, such that each
operation or expression or function, would either output a regular result or an error,
an error being a special signaling value.

For instance:

a + b = c ; when no error occurs

a + b = <error> ; when an error occurs

Same for functions as well, when the function succeeds, return your regular result, when the
function fails, return an <error>

So now, you're tasked with doing stuff like this:


	```
	c := a + b
	if c is error ? {
		ret c
	}
	```

Now, this is annoying, but it has one really nice property, and that is that you're always
in control of the execution flow, which isn't true for a lot of other languages, and one
of the biggest complaints I see.

So to make this easier, instead of making it so that you have to manually check, we flip
it, so that:

	```
	c := a + b
	```

Automatically returns on error, and if you want to catch the error, you do:

	```
	c := try a + b
	if c is error ? {
		...
	}
	```







## Compilers and Atoms:

Dear globals:

I've used elf for many years - long enough to know one thing: globals aren't really needed anymore...

I think, once more, I will embark on another character-defining journey, this time to slay the global dragon.

The addition of static modules has already given me a path forward. Well, I just don't see globals where I'm going; they shall remain part of the old world, never to be implicitly referred to again, leaving behind that which continually causes oh so much anguish.

Here's the itinerary:

* Drop support for implicit globals; all global symbols at compilation time will become real compiler entities.
* You will no longer be able to assign directly.
* Instead, the global registry will remain as a temporary way to register an "environment," as it were. This should remain compatible with existing projects, or at least mostly.
* It will also perhaps come with a gratuitous performance boost.

But most notably of all: no more implicit globals.

