@ECHO OFF
@SETLOCAL

@REM Always build from the repository root so paths in build.elf are stable.
@PUSHD "%~dp0"
@"%~dp0bootstrap\windows-x64\bob.exe" %*
@SET "ELF_BUILD_EXIT=%ERRORLEVEL%"
@POPD

@EXIT /B %ELF_BUILD_EXIT%
