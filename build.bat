@REM this just builds an object
@REM see elf-lang/elven @ github

@ECHO OFF
@SET SRCIN=               ^
src/platform/system.c     ^
src/internal_main.c       ^
src/userapi.c             ^
src/compiler/compiler.c

@SET INC= ^
/Isrc ^
/Isrc/libs    ^
/Isrc/platform ^
/Isrc/compiler ^
/Iinclude ^
/Istb

@SET FLAGS=%1


clang-cl /nologo -Od -Zi /c %FLAGS% %SRCIN% %INC% -DPLATFORM_DESKTOP -D_DEBUG
lib /nologo /out:elf.lib system.obj internal_main.obj compiler.obj userapi.obj
