@echo off
@rem -fsanitize=address

@SET SRCIN=               ^
src/platform/system.c     ^
src/core/internal_main.c  ^
src/core/public_api.c     ^
src/compiler/elf_compiler.c

@SET INC= ^
/Isrc ^
/Isrc/platform ^
/Isrc/core ^
/Isrc/compiler ^
/Iinclude ^
/Istb

clang-cl /nologo -Od -Zi /c %SRCIN% %INC% -DPLATFORM_DESKTOP -D_DEBUG
lib /nologo /out:elf.lib system.obj internal_main.obj elf_compiler.obj public_api.obj
