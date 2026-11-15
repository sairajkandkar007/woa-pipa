@echo off
pnputil /enum-devices /problem > C:\pipa-logs\problem-devices.txt 2>&1
pnputil /enum-devices /drivers > C:\pipa-logs\all-devices.txt 2>&1
echo %date% %time% > C:\pipa-logs\last-run.txt
