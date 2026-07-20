# Elf batteries

This directory contains the optional official host libraries. Compile
`batteries.c` alongside an embedding program and call:

```c
elf_State *state = elf_create_state();
elf_open_batteries(state);
```

The batteries currently install file loading, printing, serialization,
filesystem, process, and script-visible time operations. `elf.lib` does not
contain or register them.
