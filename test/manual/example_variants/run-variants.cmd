@echo off
setlocal
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-variants.ps1" %*
exit /b %ERRORLEVEL%
