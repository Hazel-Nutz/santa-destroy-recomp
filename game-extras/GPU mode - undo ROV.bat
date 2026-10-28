@echo off
rem Undo ROV: back to the default GPU mode (RTV). Close the game first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-gpu-mode.ps1" -Mode auto
echo.
pause
