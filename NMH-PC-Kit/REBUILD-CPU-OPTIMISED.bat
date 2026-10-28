@echo off
rem Builds with code optimised for THIS PC's CPU (-march=native). The result may crash on other/older CPUs:
rem never share a build made with this. Use BUILD.bat for a portable build.
set GIT_CONFIG_COUNT=1
set GIT_CONFIG_KEY_0=safe.directory
set GIT_CONFIG_VALUE_0=*
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -CpuArch native -RebuildSdk %*
echo.
pause
