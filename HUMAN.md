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
allows elf to statically check arguments at compile time.


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

