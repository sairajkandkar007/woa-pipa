@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0phase0b_verify.ps1"
echo.
echo Finished. Tell Claude in chat.
pause
