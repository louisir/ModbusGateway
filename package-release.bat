@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-release.ps1" %*
exit /b %ERRORLEVEL%
