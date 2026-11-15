@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0phase3_boot.ps1" -Img "Mu-pipa-2.3-automsc-B.img"
