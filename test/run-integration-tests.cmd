@echo off
setlocal
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-integration-tests.ps1" %*
exit /b %ERRORLEVEL%
