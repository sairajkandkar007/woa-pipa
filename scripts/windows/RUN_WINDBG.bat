@echo off
rem Claude: start WinDbg on this PC, waiting for the tablet's kernel debugger over USB.
rem Log goes to logs\windbg_<time>.log
cd /d "%~dp0"
if not exist logs mkdir logs
where windbgx >nul 2>&1 || (echo WinDbg not found - installing with winget... & winget install --id Microsoft.WinDbg -e --accept-package-agreements --accept-source-agreements)
set LOG=%~dp0logs\windbg_%RANDOM%.log
start "" windbgx -k net:port=50000,key=1.2.3.4 -logo "%LOG%" -c "sxn ld; g"
echo WinDbg started. Log file: %LOG%
pause
