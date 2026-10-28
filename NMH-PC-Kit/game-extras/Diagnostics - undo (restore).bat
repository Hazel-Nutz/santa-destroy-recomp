@echo off
rem Undo: put the diagnostics back as they were before Diagnostics - off. Close the game first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-diagnostics.ps1" -Mode restore
echo.
pause
