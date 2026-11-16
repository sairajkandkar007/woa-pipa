@echo off
cd /d "%~dp0"
net session >nul 2>&1
if %errorlevel% neq 0 (
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0phase5_drivers.ps1" -Batch batch3c.txt -ReEnumerate "ACPI\QCOM250C\1","ACPI\QCOM252E\2&daba3ff&0","ACPI\QCOM2519\2&daba3ff&0"
