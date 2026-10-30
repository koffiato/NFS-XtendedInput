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
set "GAME="
if /I "%~1"=="UG" set "GAME=GAME_UG"
if /I "%~1"=="UG2" set "GAME=GAME_UG2"
if /I "%~1"=="MW" set "GAME=GAME_MW"
if /I "%~1"=="CB" set "GAME=GAME_CARBON"
if /I "%~1"=="UC" set "GAME=GAME_UC"
if /I "%~1"=="PS" set "GAME=GAME_PROSTREET"
if /I "%~1"=="W" set "GAME=GAME_WORLD"
if not defined GAME (
  echo Usage: build.cmd UG^|UG2^|MW^|CB^|UC^|PS^|W^|all
  exit /b 1
)
if not "%~2"=="" (
  echo This branch builds Windows 10/11 targets only.
  exit /b 1
)
pushd "%~dp0"
set "OUTPUT=build\%~1"
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
set "NAME=NFS_XtendedInput"
if /I "%~1"=="UG" set "NAME=NFSU_XtendedInput"
if /I "%~1"=="UG2" set "NAME=NFSU_XtendedInput"
cl /nologo /LD /O2 /MD /EHsc /std:c++14 /W3 /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /D_CRT_SECURE_NO_WARNINGS /D%GAME% /Isrc\inc /Isrc\_vs src\Unified.cpp /Fo"%OUTPUT%\Unified.obj" /link /IMPLIB:"%OUTPUT%\%NAME%.lib" /OUT:"%OUTPUT%\%NAME%.asi" /MACHINE:X86 /SUBSYSTEM:WINDOWS user32.lib winmm.lib
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
