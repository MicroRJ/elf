# Tables

Tables are arrays and maps at the same time. Their methods use `:`.

```elf
items := {10, 20, 30}
person := { name = "Ada", score = 42 }
```

## Look around

```elf
items:length()          // 3
items:size()            // same as length
person:has("name")      // 1
person:haskey("name")   // same as has
```

Keys, values, and pairs come back as new tables:

```elf
keys := person:keys()
values := person:values()
pairs := person:pairs() // { {key, value}, ... }
```

## Map work

```elf
copy := person:clone()

combined := person:merge({
	score = 100,
	active = true,
})

old_name := person:delete("name")
person:clear()
```

`clone` and `merge` make new tables. `delete` returns the removed value or
`nil`. `clear` empties the receiver and returns it.

## Array work

Negative indexes count from the end.

```elf
items:get(0)       // 10
items:get(-1)      // 30
items:set(1, 25)   // items is now {10, 25, 30}
```

`idx` aliases `get`. `repl` aliases `set`.

Grow and shrink:

```elf
first_new := items:add(40, 50)
items:push(60)             // alias for add
items:insert(1, 15)
last := items:pop()
removed := items:remove(1)
items:remove(1, 2)         // remove two values
items:extend({70, 80}, {90})
```

`add` returns the index of the first added value. `insert`, `set`, and `extend`
return the receiver. `pop` returns `nil` when empty.

Copy a half-open slice:

```elf
middle := items:slice(1, 3)
tail := items:slice(2)
all := items:slice()
```

Shuffle in place:

```elf
items:swap(0, -1)
items:reverse()
```

## Transform

Callbacks receive `(value, index)` and the table as `this`.

```elf
doubled := {1, 2, 3}:map(fun(value, index) {
	ret value * 2
})

even := {1, 2, 3, 4}:filter(fun(value, index) {
	ret value % 2 == 0
})
```

`map` and `filter` make new tables.

Sort in place with a three-way comparison:

```elf
numbers := {3, 1, 2}
numbers:sort(fun(left, right) {
	if left < right ? ret -1
	if left > right ? ret 1
	ret 0
})
```

The comparison returns a negative integer, zero, or a positive integer.

## Complete list

```text
length size has haskey delete clear keys values pairs clone merge
get idx set repl add push insert pop remove slice extend swap reverse
map filter sort
```
