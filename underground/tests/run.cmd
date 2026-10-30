@echo off
setlocal
if "%~1"=="" (
  echo Usage: tests\run.cmd "path\to\SPEED2.EXE"
  exit /b 1
)
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
set "GAME_EXE=%~f1"
pushd "%~dp0.."
if not exist build mkdir build
cl /nologo /O2 /MD /EHsc tests\rumble_tests.cpp /Fobuild\rumble-tests.obj /Febuild\rumble-tests.exe /link /BASE:0x400000 /DYNAMICBASE:NO /FIXED:NO user32.lib
if errorlevel 1 (popd & exit /b 1)
build\rumble-tests.exe "%GAME_EXE%"
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
