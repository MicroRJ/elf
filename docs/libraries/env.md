# env

Read a variable:

```elf
home := elf.env.get("USERPROFILE")
```

Missing variables return `nil`:

```elf
if elf.env.has("MY_TOOL_PATH") == 0 {
	elf.println("not configured")
}
```

Set or remove one for this process and its children:

```elf
elf.env.set("MY_TOOL_MODE", "fast")
elf.env.unset("MY_TOOL_MODE")
```
