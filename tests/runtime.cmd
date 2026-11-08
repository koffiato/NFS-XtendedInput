@echo off
setlocal
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
if /I "%~1"=="all" (
  for %%G in (UG UG2 MW CB UC PS W) do (
    call "%~f0" %%G
    if errorlevel 1 exit /b 1
    if not errorlevel 0 exit /b 1
  )
  exit /b 0
)
call "%~dp0..\scripts\target.cmd" %1
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist build\tests mkdir build\tests
call "scripts\generate-config.cmd" "%~1" "build\tests\config-%~1"
if errorlevel 1 (popd & exit /b 1)
rc /nologo /fo"build\tests\default-%~1.res" "build\tests\config-%~1\DefaultConfig.rc"
if errorlevel 1 (popd & exit /b 1)
set "CONFIG_RESOURCE=build\tests\default-%~1.res"
cl /nologo /Od /RTC1 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /D%GAME% /Isrc\inc /Isrc\_vs tests\runtime_tests.cpp %CONFIG_RESOURCE% /Fo"build\tests\runtime-%~1.obj" /Fe"build\tests\runtime-%~1.exe" /link /BASE:0x2000000 /DYNAMICBASE /MACHINE:X86 /SUBSYSTEM:CONSOLE user32.lib winmm.lib
if errorlevel 1 (popd & exit /b 1)
build\tests\runtime-%~1.exe
set "RESULT=%errorlevel%"
popd
if not "%RESULT%"=="0" exit /b 1
exit /b 0
