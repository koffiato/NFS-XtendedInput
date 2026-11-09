@echo off
call "%~dp0feedback.cmd" MW %*
exit /b %errorlevel%
