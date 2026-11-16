# Claude: READ-ONLY. List files on the tablet's Windows partition changed since the first PEP install,
# both by the PC (DISM, PC clock 2026) and by the tablet itself (tablet clock is 2022-05-07).
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
Start-Transcript -Path (Join-Path $root 'logs\phase5_scan.log') -Force | Out-Null
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).Size -ne $WIN_SIZE) { Write-Host 'ERROR: tablet disk not found (Mass Storage active?)'; Stop-Transcript | Out-Null; Read-Host; exit 1 }
$existing = (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing } else {
  $used = (Get-PSDrive -PSProvider FileSystem).Name; $W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
  Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" ; Start-Sleep 2 }
$pcFrom  = [datetime]'2026-11-16 14:40'
$tabFrom = [datetime]'2022-05-07 09:00'   # tablet local clock (UTC+5:30), before the first PEP boot
$out = Join-Path $root ("scan_" + (Get-Date -Format 'yyyyMMdd_HHmm') + '.csv')
Write-Host "Scanning ${W}:\ (takes a few minutes)..."
Get-ChildItem "${W}:\" -Recurse -Force -File -ErrorAction SilentlyContinue |
  Where-Object { ($_.LastWriteTime -ge $pcFrom) -or ($_.LastWriteTime -ge $tabFrom -and $_.LastWriteTime.Year -eq 2022) -or ($_.CreationTime -ge $pcFrom) -or ($_.CreationTime -ge $tabFrom -and $_.CreationTime.Year -eq 2022) } |
  Select-Object FullName, Length, CreationTime, LastWriteTime | Export-Csv -NoTypeInformation $out
Write-Host "Wrote $out ($((Import-Csv $out).Count) files)"
if (-not ($existing -and [int][char]$existing -ne 0)) { Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" }
Stop-Transcript | Out-Null; Read-Host 'Done. Press Enter'
