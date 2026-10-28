@echo off
rem Rebuilds only nmh_launcher.exe and copies it into the game folder ("No More Heroes PC" next to this kit,
rem or pass -OutDir <game folder>). Needs a completed BUILD.bat first. Close the launcher first.
set GIT_CONFIG_COUNT=1
set GIT_CONFIG_KEY_0=safe.directory
set GIT_CONFIG_VALUE_0=*
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -LauncherOnly %*
echo.
pause
