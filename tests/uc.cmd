@echo off
call "%~dp0feedback.cmd" UC %*
exit /b %errorlevel%
