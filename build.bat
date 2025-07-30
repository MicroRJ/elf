@echo off
@rem -fsanitize=address

@SET SRCIN=       ^
src/platform/system.c      ^
src/core/r_core.c      ^
src/compiler/c_compiler.c

clang-cl /nologo /Isrc/bindings /Isrc/platform /Isrc/core /Isrc/compiler /Iinclude /Istb /Isrc -Od -Zi /c %SRCIN% -DPLATFORM_DESKTOP
lib /nologo /out:elf.lib system.obj r_core.obj c_compiler.obj
