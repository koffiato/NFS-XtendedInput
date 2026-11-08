@echo off
setlocal
if "%~2"=="" (
  echo Usage: scripts\generate-config.cmd TARGET OUTPUT_DIRECTORY
  exit /b 1
)
call "%~dp0target.cmd" %1
if errorlevel 1 exit /b 1
set "CONFIG_OUTPUT=%~f2"
pushd "%~dp0.."
if not exist "%CONFIG_OUTPUT%" mkdir "%CONFIG_OUTPUT%"
cl /nologo /O2 /MT /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS /D%GAME% tools\configgen.cpp /Fo"%CONFIG_OUTPUT%\configgen.obj" /Fe"%CONFIG_OUTPUT%\configgen.exe"
if errorlevel 1 (popd & exit /b 1)
"%CONFIG_OUTPUT%\configgen.exe" "%CD%" "%CONFIG_OUTPUT%"
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
