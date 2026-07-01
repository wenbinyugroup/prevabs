@echo off
setlocal
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-examples.ps1" %*
exit /b %ERRORLEVEL%
