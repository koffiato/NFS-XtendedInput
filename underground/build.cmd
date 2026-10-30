@echo off
setlocal
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
set "GAME_DEFINES=/DGAME_UG2"
set "OUTPUT=build\UG2"
if /I "%~1"=="UG2-Win7" (
  set "GAME_DEFINES=/DGAME_UG2 /DXINPUT_OLD /arch:SSE"
  set "OUTPUT=build\UG2-Win7"
) else if /I "%~1"=="UG" (
  set "GAME_DEFINES=/DGAME_UG"
  set "OUTPUT=build\UG"
) else if not "%~1"=="" if /I "%~1" NEQ "UG2" (
  echo Usage: build.cmd [UG2^|UG2-Win7^|UG]
  exit /b 1
)
pushd "%~dp0"
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
cl /nologo /LD /O2 /MD /EHsc /W3 /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /D_CRT_SECURE_NO_WARNINGS %GAME_DEFINES% dllmain.cpp /Fo"%OUTPUT%\dllmain.obj" /link /OUT:"%OUTPUT%\NFSU_XtendedInput.asi" /MACHINE:X86 /SUBSYSTEM:WINDOWS user32.lib winmm.lib
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
