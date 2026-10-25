## Notes on the error handling model:

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

