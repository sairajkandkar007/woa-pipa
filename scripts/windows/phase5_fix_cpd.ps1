# Claude: fix the CRITICAL_PROCESS_DIED loop after the PEP attempts.
# 1) remove the PEP provisioning package (PPMSettings-8250Profiles.wd.ppkg) that the registry undo cannot remove
# 2) restore the registry snapshot taken BEFORE the first PEP install (config_snapshot_20261116_1448)
param([string]$Snapshot = 'config_snapshot_20261116_1448')
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
Start-Transcript -Path (Join-Path $root 'logs\phase5_fix_cpd.log') -Force | Out-Null
$bk = Join-Path $root $Snapshot
if (-not (Test-Path "$bk\SYSTEM")) { Write-Host "ERROR: snapshot $bk not found"; Stop-Transcript | Out-Null; Read-Host; exit 1 }
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).Size -ne $WIN_SIZE) { Write-Host 'ERROR: tablet disk not found (Mass Storage active?)'; Stop-Transcript | Out-Null; Read-Host; exit 1 }
$existing = (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing } else {
  $used = (Get-PSDrive -PSProvider FileSystem).Name; $W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
  Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" ; Start-Sleep 2 }
if (-not (Test-Path "${W}:\Windows\System32\config\SYSTEM")) { Write-Host 'ERROR: Windows not found on p35'; Stop-Transcript | Out-Null; Read-Host; exit 1 }

$keep = Join-Path $root 'removed_from_tablet'; New-Item -ItemType Directory -Force $keep | Out-Null
$pk = "${W}:\Windows\Provisioning\Packages\PPMSettings-8250Profiles.wd.ppkg"
if (Test-Path $pk) {
  takeown /F $pk /A | Out-Null
  icacls $pk /grant '*S-1-5-32-544:F' | Out-Null
  Copy-Item $pk $keep -Force
  Remove-Item $pk -Force -ErrorAction SilentlyContinue
  if (Test-Path $pk) { Write-Host "ERROR: could not remove $pk" -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host; exit 1 }
  Write-Host "Removed PEP provisioning package (copy kept in $keep)" -ForegroundColor Green
} else { Write-Host 'PEP provisioning package already gone' }

Write-Host "Restoring registry snapshot $bk"
robocopy $bk "${W}:\Windows\System32\config" /MIR /B /R:1 /W:1 /NFL /NDL /NP | Out-Null
Write-Host "robocopy exit $LASTEXITCODE (1-7 = files copied OK, 0 = nothing to copy, 8+ = error)"
Set-Content (Join-Path $root 'LAST_CONFIG_SNAPSHOT.txt') $bk
if (-not ($existing -and [int][char]$existing -ne 0)) { Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" }
Write-Host 'FIX DONE' -ForegroundColor Green; Stop-Transcript | Out-Null; Read-Host 'Enter'
