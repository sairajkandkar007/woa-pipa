# Phase 4 (Claude): boot TWRP temporarily, expose whole sda, COPY Windows logs off the tablet (read-only use).
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot; $ws = Split-Path $root -Parent
Start-Transcript -Path (Join-Path $root 'logs\phase4_collect.log') -Force | Out-Null
$adb='C:\platform-tools\adb.exe'; $fb='C:\platform-tools\fastboot.exe'
$UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
function FindDisk { Update-HostStorageCache; Get-Disk | Where-Object { $_.LogicalSectorSize -eq 4096 -and $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' } }

if ((& $adb get-state 2>&1) -notmatch 'recovery') {
  Write-Host 'Waiting for fastboot (hold Power + Vol Down on the tablet)...'
  for ($i=0; $i -lt 90; $i++) { if ((& $fb devices 2>&1) -match 'fastboot') { break }; Start-Sleep 2 }
  cmd /c "`"$fb`" boot `"$ws\boot_partitions.img`" 2>&1"
  Write-Host 'Waiting for TWRP adb...'
  for ($i=0; $i -lt 90; $i++) { if ((& $adb get-state 2>&1) -match 'recovery') { break }; Start-Sleep 2 }
}
if ((& $adb get-state 2>&1) -notmatch 'recovery') { Write-Host 'ERROR: TWRP not reachable'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }

$disk = FindDisk
if (-not $disk) {
  & $adb push (Join-Path $root 'msc_sda.sh') /tmp/msc_sda.sh
  & $adb shell 'chmod 755 /tmp/msc_sda.sh; nohup sh /tmp/msc_sda.sh > /tmp/msc.log 2>&1 &'
  for ($i=0; $i -lt 40 -and -not $disk; $i++) { Start-Sleep 2; $disk = FindDisk }
}
if (-not $disk -or $disk.PartitionStyle -ne 'GPT') { Write-Host 'ERROR: tablet disk not found as GPT. Do NOT initialize.'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }
$p35 = Get-Partition -DiskNumber $disk.Number -PartitionNumber 35
if ($p35.Size -ne $WIN_SIZE) { Write-Host 'ERROR: p35 size mismatch'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }
$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"
Start-Sleep 2

$out = Join-Path $root ("windows_logs_" + (Get-Date -Format 'yyyyMMdd_HHmm'))
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item "${W}:\pipa-logs\*" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\INF\setupapi.dev.log" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\INF\setupapi.offline.log" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\Panther\setupact.log" (Join-Path $out 'panther_setupact.log') -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\Panther\UnattendGC\setupact.log" (Join-Path $out 'unattendgc_setupact.log') -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\System32\winevt\Logs\System.evtx" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\Minidump" (Join-Path $out 'Minidump') -Recurse -Force -ErrorAction SilentlyContinue
# Offline device enumeration from the SYSTEM hive (Enum tree: hardware IDs + driver binding)
reg load HKLM\PIPA_SYS "${W}:\Windows\System32\config\SYSTEM" | Out-Null
reg export HKLM\PIPA_SYS\ControlSet001\Enum (Join-Path $out 'enum.reg') /y | Out-Null
[gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SYS | Out-Null
Get-ChildItem $out -Recurse | Select-Object FullName,Length | Format-Table -AutoSize | Out-String | Write-Host

Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"
Write-Host "`nCOLLECT COMPLETE -> $out" -ForegroundColor Green
Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
