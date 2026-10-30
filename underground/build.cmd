@echo off
call "%~dp0..\build.cmd" %*
exit /b %errorlevel%
