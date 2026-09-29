# Bootstrap tools

elf uses Manny to interpret `build.elf`. A verified Manny executable is committed
here so a fresh Windows checkout can build without installing Manny separately.

The executable is a bootstrap seed, not the source of truth. Manny's source lives
in its own repository. Update the seed deliberately after rebuilding and
testing Manny against the elf revision it embeds, then update the neighboring
manifest and SHA-256 hash.

`build.bat` invokes the platform-specific bootstrap executable while keeping
the repository root as the working directory.
