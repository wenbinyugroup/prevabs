@echo off
setlocal
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-unit-tests.ps1" %*
exit /b %ERRORLEVEL%
