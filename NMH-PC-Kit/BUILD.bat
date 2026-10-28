@echo off
rem Builds No More Heroes: Heroes' Paradise for PC (portable: any x64 CPU with AVX, Ivy Bridge or newer).
rem Put your disc ISO (or an extracted disc\ folder) in the folder that contains this kit, or pass -GameFolder <path>.
rem First build: SDK 30-90 min + game 20 min to several hours. If it fails, read build.log.
set GIT_CONFIG_COUNT=1
set GIT_CONFIG_KEY_0=safe.directory
set GIT_CONFIG_VALUE_0=*
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -CpuArch ivybridge %*
echo.
pause
