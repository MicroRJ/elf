@REM this just builds an object
@REM see elf-lang/elven @ github

@ECHO OFF
@SETLOCAL

@CALL vcvars64 >nul
@IF ERRORLEVEL 1 EXIT /B %ERRORLEVEL%

@SET TARGET=%1
@SET FLAGS=
@IF "%TARGET%"=="" SET TARGET=all
@IF /I NOT "%TARGET%"=="all" IF /I NOT "%TARGET%"=="lib" (
	@SET TARGET=all
	@SET FLAGS=%1
)

@SET SRCIN= ^
src/base/base.c ^
src/platform/system.c ^
src/core/value/value.c ^
src/compiler/backend/bytecode.c ^
src/compiler/backend/bytecode_debug.c ^
src/core/core.c ^
src/compiler/compiler.c ^
src/api/elf_api.c

@SET INC= ^
/Isrc ^
/Isrc/base ^
/Isrc/api ^
/Isrc/core ^
/Isrc/diagnostics ^
/Isrc/core/value ^
/Isrc/core/table ^
/Isrc/core/atom ^
/Isrc/platform ^
/Isrc/runtime ^
/Isrc/runtime/vm ^
/Isrc/runtime/libs ^
/Isrc/compiler ^
/Isrc/compiler/frontend ^
/Isrc/compiler/middle ^
/Isrc/compiler/backend ^
/Iinclude ^
/Istb

if not exist build mkdir build

clang-cl /nologo -Od -Zi /c %FLAGS% %SRCIN% %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fobuild\
if errorlevel 1 exit /b %errorlevel%

lib /nologo /out:build\elf.lib build\base.obj build\system.obj build\value.obj build\bytecode.obj build\bytecode_debug.obj build\core.obj build\compiler.obj build\elf_api.obj
if errorlevel 1 exit /b %errorlevel%

if /I "%TARGET%"=="lib" goto :eof

clang-cl /nologo -Od -Zi %FLAGS% tools\tests.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fobuild\tests.obj /Fe:build\tests.exe /Fd:build\tests.pdb /link build\elf.lib /PDB:build\tests.pdb /ILK:build\tests.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\benchmarks.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fobuild\benchmarks.obj /Fe:build\benchmarks.exe /Fd:build\benchmarks.pdb /link build\elf.lib /PDB:build\benchmarks.pdb /ILK:build\benchmarks.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\bytecode.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fobuild\bytecode_tool.obj /Fe:build\bytecode.exe /Fd:build\bytecode.pdb /link build\elf.lib /PDB:build\bytecode.pdb /ILK:build\bytecode.ilk
if errorlevel 1 exit /b %errorlevel%

clang-cl /nologo -Od -Zi %FLAGS% tools\elf.c %INC% -D_DEBUG -DPLATFORM_DESKTOP /Fobuild\elf.obj /Fe:build\elf.exe /Fd:build\elf.pdb /link build\elf.lib /PDB:build\elf.pdb /ILK:build\elf.ilk
if errorlevel 1 exit /b %errorlevel%
