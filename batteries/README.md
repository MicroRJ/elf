# Elf batteries

This directory contains the optional official host libraries. Include
`elf_batteries.h`, link `elf_batteries.lib` alongside `elf.lib`, and call:

```c
elf_State *state = elf_create_state();
elf_open_batteries(state);
```

The batteries currently install file loading, printing, serialization,
filesystem, process, and script-visible time operations. `elf.lib` does not
contain or register them.
