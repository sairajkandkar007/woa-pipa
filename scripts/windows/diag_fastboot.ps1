# Claude: READ-ONLY fastboot diagnostics (battery, slot, partition state).
$fb='C:\platform-tools\fastboot.exe'
$log = Join-Path $PSScriptRoot 'logs\diag_fastboot.log'
"=== $(Get-Date) ===" | Out-File $log -Append
Write-Host 'Waiting for fastboot (hold Power + Vol Down)...'
for ($i=0; $i -lt 120; $i++) { if ((& $fb devices 2>&1) -match 'fastboot') { break }; Start-Sleep 2 }
foreach ($v in 'battery-voltage','battery-soc-ok','charger-screen-enabled','off-mode-charge','current-slot','slot-successful:a','slot-unbootable:a','slot-retry-count:a','unlocked','product','version-bootloader','is-userspace') {
  cmd /c "`"$fb`" getvar $v 2>&1" | Select-Object -First 1 | Tee-Object -FilePath $log -Append
}
cmd /c "`"$fb`" getvar all 2>&1" | Out-File $log -Append
Write-Host "`nSaved to $log"
Read-Host 'Press Enter to close'
