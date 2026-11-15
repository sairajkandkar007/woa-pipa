@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0phase3_boot.ps1" -Img "..\firmware\Mu-pipa_8GB.img"
