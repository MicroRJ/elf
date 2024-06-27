There is one notable difference between elif
and else if.

Doing else if like this:

if ... {
	...
} else if ...? {
}


is equivalent to:


if ... {
	...
} else {
	if ...? {
	}
}

Notice how a child 'if' statement is created.

When using 'elif' however, a new conditional
branch is concatenated to the if statement
directly, this makes it so that multiple
targets, 'if' or 'elif' point to an optional
'then' branch.

if x ? {
	...
} elif y ? {
	...
} elif z ? {
	...
} then {
	...
}

Here then is triggered for x y and z.

This same behavior couldn't be achieved
with 'else if'.

