@REM this just builds an object
@REM see elf-lang/elven @ github

@ECHO OFF
@SET SRCIN=    ^
src/base/base.c ^
src/platform/system.c ^
src/core/value/value.c ^
src/compiler/backend/bytecode.c ^
src/compiler/backend/bytecode_debug.c ^
src/core/core.c ^
src/compiler/compiler.c ^
src/api/userapi.c

@SET INC=      ^
/Isrc                         ^
/Isrc/base                    ^
/Isrc/api                     ^
/Isrc/core                    ^
/Isrc/core/table              ^
/Isrc/core/atom               ^
/Isrc/platform                ^
/Isrc/vm                      ^
/Isrc/libs                    ^
/Isrc/compiler                ^
/Isrc/compiler/frontend       ^
/Isrc/compiler/middle         ^
/Isrc/compiler/backend        ^
/Iinclude                     ^
/Istb

@SET FLAGS=%1

if not exist build mkdir build

clang-cl /nologo -Od -Zi /c %FLAGS% %SRCIN% %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fo:build\
if errorlevel 1 exit /b %errorlevel%

lib /nologo /out:build\elf.lib build\base.obj build\system.obj build\value.obj build\bytecode.obj build\bytecode_debug.obj build\core.obj build\compiler.obj build\userapi.obj
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\tests.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fo:build\tests.obj /Fe:build\tests.exe /Fd:build\tests.pdb /link build\elf.lib /PDB:build\tests.pdb /ILK:build\tests.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\benchmarks.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fo:build\benchmarks.obj /Fe:build\benchmarks.exe /Fd:build\benchmarks.pdb /link build\elf.lib /PDB:build\benchmarks.pdb /ILK:build\benchmarks.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\bytecode.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fo:build\bytecode.obj /Fe:build\bytecode.exe /Fd:build\bytecode.pdb /link build\elf.lib /PDB:build\bytecode.pdb /ILK:build\bytecode.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\elf.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fo:build\elf.obj /Fe:build\elf.exe /Fd:build\elf.pdb /link build\elf.lib /PDB:build\elf.pdb /ILK:build\elf.ilk
if errorlevel 1 exit /b %errorlevel%
