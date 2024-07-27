/*
** See Copyright Notice In elf.h
** elf-obj.h
** ...
*/



/* this collector is dog ... */
#define elGC_MEM_THRESHOLD_MIN (elInteger) MEGABYTES(4)
#define elGC_MEM_THRESHOLD_MAX (elInteger) MEGABYTES(64)

#define elGC_OBJ_THRESHOLD_MIN (elInteger) ((2048)*1)
#define elGC_OBJ_THRESHOLD_MAX (elInteger) ((2048)*512)


/* 7/25/24 10:50 PM

Notes on the collection cycle:

When I was putting this thing together I thought
that I could initialize objects to black to delay
their collection for at least one more cycle,
which at the time for some reason I thought was
useful.

Since I never really got any GC related errors
and it seemed like such a benign or arbitrary
thing, I didn't realize then is that this is
actually totally wrong.

So recently, now that I've been doing
more memory intensive stuff, I've been getting
constant GC crashes it was now that after
debugging virtually every other aspect of the
interpreter that I finally made the right
connection.

CYCLES!

Here's the explanation:

Collection works in 2 phases, the starting
phase, checks reachability, this is called
marking.
In this stage a white object means it was
never encountered before and we can mark it
now and also mark its children.
Here's the important part: in THIS phase,
BLACK objects mean they've already been
marked already, and thus we ignore them
and their children entirely.

As a side note:
If you encounter a black object it simply
means you've found another path that leads
to that object.
It also means that object is referenced
multiple times, by multiple paths.

Anyways, on to the second or last phase.
After we've conducted our reachability
pass, all reachable objects are black and
those that aren't remained white.
So in this phase, we collect all WHITE
objects, and turn BLACK object WHITE
to complete the CYCLE.

So note how the last phase sets up
objects so that they are ready to
enter the first phase.
Since both of these phases run together
one right after the other, the GC is
always ready to cycle, that is, to repeat
the process once more.

Objects always enter the GC cycle
from the start, never in between.
An object is never allocated in
between any of the phases.

And this is why marking an object black
initially is simply wrong.

Because if you were to mark an object as black,
it'd be like starting up that object in the
wrong phase entirely.

If an object is black, during the first phase
of the collection cycle, the collector thinks
that it has already been marked and its
children won't be marked.

Now, I haven't really thought about this that
much further ahead, but think that if you actually
wanted to flip the cycle, (objects are black initially)
you'd have to reverse the collection cycle,
collect objects first and then mark them, which I would
assume would bring other complications.

So here's an scenario that was crashing the
GC when objects where initially marked as black.

For instance, say you're enumerating a folder,
for each file, you create a corresponding file
object or table, you set the name, the path
and some other attributes, during that, the
file object enters the GC cycle, and it gets
marked as WHITE.
Now, after that you add the file object
to a global table where you store all the
files. The table is... initially black.

So now you have something like this:

	list_of_files: (BLACK)
		file_a (WHITE)
		file_b (WHITE)
		file_c (WHITE)

So now, the GC triggers again and all objects
are passed through the GC cycle.

Since the parent object, 'list_of_files' is
black, none of its children are marked as
reachable, consequently, they are all freed.

The lesson was:

PHASES, CYCLES AND STATES!

In summary, the color of an object is not just
whether it is reachable or not, but also which
phase of the GC cycle is on, and thus must be
colored accordingly.

Objects don't have to be marked black initially
in order to "delay" their collection:

When a new object is allocated it is guaranteed
to remain valid till the next collection pass,
by that time the object should already be somewhere
reachable, like the stack or globals.

	new_object() * GC could run, however, it runs
					   before the object is added to
					   the GC list.
					   Since the object is white,
					   it is ready to enter the GC
					   cycle.

	At this point the object is safe to access

	collect()	 * GC runs and it collects the object

	At this point the object isn't safe to unless
	it was added to the stack or globals.
	If the GC triggered the object was definitely
	collected if not reachable, since white.
*/
typedef enum elGCColor {
	GC_WHITE = 0, GC_BLACK, GC_PINK, GC_RED,
} elGCColor;



/* first object tag must be OBJECT, all other
objects come after it */
#define TAGLIST(_) \
_(NIL) _(GCD) _(SYS) \
_(INT) _(NUM) _(BID) \
_(OBJ) _(CLS) _(STR) _(TAB) /* end */



typedef enum elObjType {
	OBJ_NONE = 0,
	OBJ_CLOSURE,
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_TAB,
	OBJ_CUSTOM,
} elObjType;


typedef struct elObject {
	elObjType type;
	elGCColor color;
	elTable *metatable;
	short tell;
} elObject;


typedef enum elValueTag {
#define TAGENUM(NAME) XFUSE(TAG_,NAME),
	TAGLIST(TAGENUM)
#undef TAGENUM
} elValueTag;


#define TAGENUM(NAME) XSTRINGIFY(NAME),
elf_globaldecl char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM


typedef struct elValue {
	elValueTag tag;
	union {
		elAddr p,x_ptr;
		elHandle h,x_sys;
		elBinding c;
		elInteger i,x_int;
		elNumber n,x_num;
		elClosure *f,*x_cls;
		elObject *j,*x_obj;
		elTable *t,*x_tab;
		elString *s,*x_str;
	};
} elValue;


typedef struct elClosure {
	elObject obj;
	union { elProto prototype, fn; };

	elByteId  j;
	elValue enclosure[1];
} elClosure;


elf_api elValue elf_table_value(elTable *);
elf_api elValue elf_binding_value(elBinding);
elf_api elValue elf_string_value(elString *);
elf_api elValue elf_closure_value(elClosure *);
elf_api elValue elf_integer_value(elInteger i);
elf_api elValue elf_number_value(elNumber n);
elf_api elValue elf_nil_value();



