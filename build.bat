@echo off
@rem -fsanitize=address

@SET SRCIN=               ^
src/platform/system.c     ^
src/core/r_core.c         ^
src/compiler/elf_compiler.c

clang-cl /nologo /Isrc/bindings /Isrc/platform /Isrc/core /Isrc/compiler /Iinclude /Istb /Isrc -Od -Zi /c %SRCIN% -DPLATFORM_DESKTOP -D_DEBUG
lib /nologo /out:elf.lib system.obj r_core.obj elf_compiler.obj
