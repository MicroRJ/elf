@REM this just builds an object
@REM see elf-lang/elven @ github

@ECHO OFF
@SET SRCIN=               ^
src/platform/system.c     ^
src/runtime.c             ^
src/userapi.c             ^
src/compiler.c

@SET INC =     ^
/Isrc          ^
/Isrc/libs     ^
/Isrc/platform ^
/Isrc          ^
/Iinclude      ^
/Istb

@SET FLAGS=%1


clang-cl /nologo -Od -Zi /c %FLAGS% %SRCIN% %INC% -DPLATFORM_DESKTOP -D_DEBUG
lib /nologo /out:elf.lib system.obj runtime.obj compiler.obj userapi.obj
