@echo off
setlocal
if /I "%VSCMD_ARG_TGT_ARCH%" NEQ "x86" (
  echo Use an x86 Visual Studio developer command prompt.
  exit /b 1
)
pushd "%~dp0.."
if not exist build\tests mkdir build\tests
for %%G in (GAME_UG2 GAME_PROSTREET) do (
  if not exist "build\tests\input-%%G" mkdir "build\tests\input-%%G"
  cl /nologo /Od /RTC1 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /D%%G tests\input_helpers_tests.cpp src\input\State.cpp /Fo"build\tests\input-%%G/" /Fe"build\tests\input-%%G.exe"
  if errorlevel 1 (popd & exit /b 1)
  build\tests\input-%%G.exe
  if errorlevel 1 (popd & exit /b 1)
)
popd
