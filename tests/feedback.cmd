@echo off
setlocal
if /I "%~1"=="all" (
  for %%G in (MW CB UC PS) do (
    call "%~f0" %%G
    if errorlevel 1 exit /b 1
  )
  exit /b 0
)
set "TEST_NAME="
if /I "%~1"=="MW" set "TEST_NAME=mw"
if /I "%~1"=="CB" set "TEST_NAME=cb"
if /I "%~1"=="UC" set "TEST_NAME=uc"
if /I "%~1"=="PS" set "TEST_NAME=prostreet"
if not defined TEST_NAME (
  echo Usage: tests\feedback.cmd ^<MW^|CB^|UC^|PS^|all^> [mapped-image]
  exit /b 1
)
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
call "%~dp0..\scripts\target.cmd" %1
if errorlevel 1 exit /b 1
set "TEST_ARGUMENT="
if not "%~2"=="" set TEST_ARGUMENT="%~f2"
pushd "%~dp0.."
call scripts\sources.cmd
if not exist "build\tests\%TEST_NAME%" mkdir "build\tests\%TEST_NAME%"
cl /nologo /Od /RTC1 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /D%GAME% /Isrc\inc /Isrc\_vs tests\%TEST_NAME%_rumble_tests.cpp %RUNTIME_SOURCES% /Fo"build\tests\%TEST_NAME%/" /Fe"build\tests\%TEST_NAME%.exe" /link /BASE:0x2000000 /DYNAMICBASE /MACHINE:X86 /SUBSYSTEM:CONSOLE user32.lib winmm.lib
if errorlevel 1 (popd & exit /b 1)
build\tests\%TEST_NAME%.exe %TEST_ARGUMENT%
set "RESULT=%errorlevel%"
popd
if not "%RESULT%"=="0" exit /b 1
exit /b 0
