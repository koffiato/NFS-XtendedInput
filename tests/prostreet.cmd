@echo off
setlocal
pushd "%~dp0.."
if not exist build\tests mkdir build\tests
cl /nologo /Od /RTC1 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /Isrc\inc /Isrc\_vs tests\prostreet_rumble_tests.cpp /Fo"build\tests\prostreet.obj" /Fe"build\tests\prostreet.exe" /link /BASE:0x2000000 /DYNAMICBASE /MACHINE:X86 /SUBSYSTEM:CONSOLE user32.lib winmm.lib
if errorlevel 1 (popd & exit /b 1)
build\tests\prostreet.exe %*
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%

