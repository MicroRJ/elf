# elf batteries

These are just the optional platform bindings, the default elf CLI will
include these, but you don't have to.

```c
elf_State *state = elf_create_state();
elf_open_batteries(state);
```
