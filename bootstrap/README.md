# Bootstrap tools

Elf uses Bob to interpret `build.elf`. A verified Bob executable is committed
here so a fresh Windows checkout can build without installing Bob separately.

The executable is a bootstrap seed, not the source of truth. Bob's source lives
in its own repository. Update the seed deliberately after rebuilding and
testing Bob against the Elf revision it embeds, then update the neighboring
manifest and SHA-256 hash.

`build.bat` invokes the platform-specific bootstrap executable while keeping
the repository root as the working directory.
