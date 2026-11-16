# Claude: READ-ONLY check. Compare the tablet's registry hives with the two latest snapshots, list recent files, collect Minidump/MEMORY.DMP info.
param([switch]$RemovePpkg)
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
Start-Transcript -Path (Join-Path $root 'logs\phase5_check.log') -Force | Out-Null
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).Size -ne $WIN_SIZE) { Write-Host 'ERROR: tablet disk not found (Mass Storage active?)'; Stop-Transcript | Out-Null; Read-Host; exit 1 }
$existing = (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing } else {
  $used = (Get-PSDrive -PSProvider FileSystem).Name; $W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
  Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" ; Start-Sleep 2 }
$cfg = "${W}:\Windows\System32\config"
$snaps = Get-ChildItem $root -Directory -Filter 'config_snapshot_*' | Sort-Object Name -Descending | Select-Object -First 3
foreach ($h in 'SYSTEM','SOFTWARE','DRIVERS','SAM','SECURITY','DEFAULT') {
  $f = Get-Item "$cfg\$h" -Force
  $line = "{0,-9} tablet {1:yyyy-MM-dd HH:mm:ss} {2,10} {3}" -f $h, $f.LastWriteTime, $f.Length, (Get-FileHash "$cfg\$h" -Algorithm MD5).Hash.Substring(0,8)
  foreach ($s in $snaps) { $p = Join-Path $s.FullName $h; if (Test-Path $p) { $line += " | $($s.Name.Substring(16)) $((Get-FileHash $p -Algorithm MD5).Hash.Substring(0,8))" } }
  Write-Host $line
}
Write-Host "`nLOG files on tablet:"; Get-ChildItem "$cfg\*.LOG*" -Force | ForEach-Object { "{0,-16} {1:yyyy-MM-dd HH:mm:ss} {2}" -f $_.Name, $_.LastWriteTime, $_.Length }
$out = Join-Path $root ("crash_" + (Get-Date -Format 'yyyyMMdd_HHmm')); New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item "${W}:\Windows\Minidump\*" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\System32\winevt\Logs\System.evtx","${W}:\Windows\System32\winevt\Logs\Application.evtx" $out -Force -ErrorAction SilentlyContinue
Get-ChildItem "${W}:\Windows\MEMORY.DMP","${W}:\Windows\Minidump" -Force -ErrorAction SilentlyContinue | Format-Table Name,Length,LastWriteTime
Get-ChildItem "${W}:\Windows\Provisioning\Packages" -Force -ErrorAction SilentlyContinue | Format-Table Name,Length,LastWriteTime
Write-Host "Copied crash files to $out"
if ($RemovePpkg) {
  $pk = "${W}:\Windows\Provisioning\Packages\PPMSettings-8250Profiles.wd.ppkg"
  if (Test-Path $pk) { Move-Item $pk (Join-Path $out 'PPMSettings-8250Profiles.wd.ppkg') -Force; Write-Host "Moved leftover PEP provisioning package off the tablet: $pk" -ForegroundColor Yellow }
  else { Write-Host 'PEP provisioning package not present' }
}
if (-not ($existing -and [int][char]$existing -ne 0)) { Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" }
Stop-Transcript | Out-Null; Read-Host 'Done. Press Enter'
