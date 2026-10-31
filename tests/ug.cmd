@echo off
setlocal
if "%~1"=="" (
  echo Usage: tests\ug.cmd "path\to\Speed.exe"
  exit /b 1
)
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
set "GAME_EXE=%~f1"
pushd "%~dp0.."
if not exist build\tests mkdir build\tests
cl /nologo /O2 /MD /EHsc tests\ug_rumble_tests.cpp /Fobuild\tests\ug-rumble.obj /Febuild\tests\ug-rumble.exe /link /BASE:0x20000000 /DYNAMICBASE user32.lib
if errorlevel 1 (popd & exit /b 1)
build\tests\ug-rumble.exe "%GAME_EXE%"
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
