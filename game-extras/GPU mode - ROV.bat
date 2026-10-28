@echo off
rem Switch this game to the ROV GPU mode (often lower GPU load). Close the game first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-gpu-mode.ps1" -Mode rov
echo.
pause
