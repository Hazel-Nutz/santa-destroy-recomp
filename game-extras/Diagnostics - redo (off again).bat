@echo off
rem Redo: turn the diagnostics off again after an undo. Close the game first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-diagnostics.ps1" -Mode off
echo.
pause
