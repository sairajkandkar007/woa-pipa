# Phase 5 UNDO v2 (Claude): mirror back the full registry snapshot (hives + logs) taken before the last batch.
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
Start-Transcript -Path (Join-Path $root 'logs\phase5_undo2.log') -Force | Out-Null
$bk = Get-Content (Join-Path $root 'LAST_CONFIG_SNAPSHOT.txt'); Write-Host "Restoring full config snapshot from $bk"
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).Size -ne $WIN_SIZE) { Write-Host 'ERROR: disk/partition check failed'; Read-Host; exit 1 }
$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$existing = (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing; Write-Host "Reusing existing drive letter ${W}:" }
else { Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" }; Start-Sleep 2
robocopy $bk "${W}:\Windows\System32\config" /MIR /B /R:1 /W:1 /NFL /NDL /NP | Out-Null
Write-Host "robocopy exit $LASTEXITCODE (0-7 = OK)"
Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"
Write-Host 'UNDO v2 DONE' -ForegroundColor Green; Stop-Transcript | Out-Null; Read-Host 'Enter'
