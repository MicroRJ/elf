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
