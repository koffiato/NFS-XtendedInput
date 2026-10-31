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
  )
  exit /b 0
)
call "%~dp0scripts\target.cmd" %1
if errorlevel 1 exit /b 1
if not "%~2"=="" (
  echo This branch builds Windows 10/11 targets only.
  exit /b 1
)
pushd "%~dp0"
set "OUTPUT=build\%~1"
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
cl /nologo /LD /O2 /MD /EHsc /std:c++14 /W3 /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /D_CRT_SECURE_NO_WARNINGS /D%GAME% /Isrc\inc /Isrc\_vs src\Unified.cpp /Fo"%OUTPUT%\Unified.obj" /link /IMPLIB:"%OUTPUT%\%NAME%.lib" /OUT:"%OUTPUT%\%NAME%.asi" /MACHINE:X86 /SUBSYSTEM:WINDOWS user32.lib winmm.lib
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
