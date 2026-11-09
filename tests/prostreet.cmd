@echo off
call "%~dp0feedback.cmd" PS %*
exit /b %errorlevel%

