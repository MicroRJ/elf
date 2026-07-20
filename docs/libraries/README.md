# Standard library

Elf installs its standard libraries beneath the global `elf` table. Functions
directly on that table form the core library. More focused libraries occupy
their own fields, such as `elf.fs` and `elf.debug`.

The library API is still evolving. These pages document the behavior of the
current implementation rather than promising long-term compatibility.

## Library reference

- [Core](core.md) — assertions, value inspection, conversions, caller
  introspection, source loading, and standard output.
- [Filesystem](fs.md) — regular text files, metadata, directories, and the
  process working directory.

The remaining sublibraries are not documented yet.
