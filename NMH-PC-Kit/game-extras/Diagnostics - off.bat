@echo off
rem Turn all diagnostics off (saves the current values for undo). Close the game first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-diagnostics.ps1" -Mode off
echo.
pause
