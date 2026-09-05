@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build\Start-TSAVPreVis.ps1" %*
if errorlevel 1 pause
