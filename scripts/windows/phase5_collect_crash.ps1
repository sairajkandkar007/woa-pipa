# Phase 5 CRASH COLLECT (Claude): tablet in Mass Storage. READ-ONLY: copy crash dumps, event log, setupapi, SrtTrail.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976; $ESP_SIZE = 1073741824
$stamp = Get-Date -Format 'yyyyMMdd_HHmm'
Start-Transcript -Path (Join-Path $root "logs\phase5_repair_$stamp.log") -Force | Out-Null
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or $disk.PartitionStyle -ne 'GPT') { Write-Host 'ERROR: tablet disk not found (Mass Storage?)'; Read-Host; exit 1 }
$n = $disk.Number
if ((Get-Partition -DiskNumber $n -PartitionNumber 35).Size -ne $WIN_SIZE -or (Get-Partition -DiskNumber $n -PartitionNumber 34).Size -ne $ESP_SIZE) { Write-Host 'ERROR: size mismatch'; Read-Host; exit 1 }
$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$used += $W
$S = @('S','T','Y','O','N','M') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$existing = (Get-Partition -DiskNumber $n -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing; Write-Host "Reusing existing drive letter ${W}:" }
else { Add-PartitionAccessPath -DiskNumber $n -PartitionNumber 35 -AccessPath "${W}:\" }
$dp = Join-Path $env:TEMP 'pipa_dp2.txt'
"select disk $n`nselect partition 34`nassign letter=$S`nexit" | Set-Content -Encoding ASCII $dp; diskpart /s $dp | Out-Null
Start-Sleep 2
$cfg = "${W}:\Windows\System32\config"
Write-Host "### config dir before"; Get-ChildItem $cfg -Force | Where-Object { $_.Name -match '^(SYSTEM|SOFTWARE|DRIVERS)' } | Format-Table Name,Length,LastWriteTime -AutoSize | Out-String | Write-Host

# 1. Collect crash info first
$out = Join-Path $root "crash_$stamp"; New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item "${W}:\Windows\Minidump\*" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\MEMORY.DMP" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\System32\winevt\Logs\System.evtx" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\INF\setupapi.dev.log" $out -Force -ErrorAction SilentlyContinue
Copy-Item "${W}:\Windows\System32\LogFiles\Srt\SrtTrail.txt" $out -Force -ErrorAction SilentlyContinue
Get-ChildItem $out | Format-Table Name,Length -AutoSize | Out-String | Write-Host

# (hive restore / BCD changes intentionally removed: collect-only)
Remove-PartitionAccessPath -DiskNumber $n -PartitionNumber 35 -AccessPath "${W}:\"
"select disk $n`nselect partition 34`nremove letter=$S`nexit" | Set-Content -Encoding ASCII $dp; diskpart /s $dp | Out-Null
Write-Host "`nCRASH COLLECT DONE" -ForegroundColor Green; Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
