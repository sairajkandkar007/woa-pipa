# Phase 4b (Claude): Tablet is in Mu UEFI "Mass Storage" mode. READ-ONLY integrity check of LUN0 vs backups,
# then copy Windows logs (mounts p35 briefly, removes letter after).
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
Start-Transcript -Path (Join-Path $root 'logs\phase4b.log') -Force | Out-Null
$UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
Write-Host 'Waiting for the tablet disk (UEFI Mass Storage)...'
$disk=$null
for ($i=0; $i -lt 90 -and -not $disk; $i++) { Update-HostStorageCache; $disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }; if (-not $disk) { Start-Sleep 2 } }
if (-not $disk) { Write-Host 'ERROR: disk not found. Do NOT initialize any disk.'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }
$disk | Format-List Number,FriendlyName,Size,LogicalSectorSize,PartitionStyle,NumberOfPartitions | Out-String | Write-Host
if ($disk.PartitionStyle -ne 'GPT') { Write-Host 'ERROR: not GPT. STOP.'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }

# ---- Integrity: hash each backed-up sda partition straight from the disk ----
$names = @{}
Get-Content (Join-Path $root 'backup\partition_list_64bit.txt') | ForEach-Object { $f = $_ -split ' '; if ($f[1] -match '^sda(\d+)$') { $names[[int]$Matches[1]] = $f[0] } }
$bkmd5 = @{}
Get-Content (Join-Path $root 'backup\backup_results.txt') | ForEach-Object { $f = $_ -split '\s+'; if ($f.Count -ge 6) { $bkmd5[$f[0]] = $f[5] } }
$fs = [IO.File]::Open("\\.\PHYSICALDRIVE$($disk.Number)", 'Open', 'Read', 'ReadWrite')
$md5 = [Security.Cryptography.MD5]::Create()
$changed = 0
Write-Host "`n### LUN0 partitions vs backup"
foreach ($p in (Get-Partition -DiskNumber $disk.Number | Sort-Object PartitionNumber)) {
  $nm = $names[[int]$p.PartitionNumber]
  if (-not $nm -or -not $bkmd5.ContainsKey($nm)) { continue }
  $fs.Position = $p.Offset; $buf = New-Object byte[] $p.Size; $read = 0
  while ($read -lt $p.Size) { $r = $fs.Read($buf, $read, [int][Math]::Min(4MB, $p.Size - $read)); if ($r -le 0) { break }; $read += $r }
  $h = ($md5.ComputeHash($buf, 0, $read) | ForEach-Object { $_.ToString('x2') }) -join ''
  $st = if ($h -eq $bkmd5[$nm]) { 'same' } else { $changed++; 'CHANGED' }
  "{0,3} {1,-18} {2,12} {3}" -f $p.PartitionNumber,$nm,$p.Size,$st | Write-Host
}
# GPT entries vs sgdisk backup (entries = 16384 bytes at LBA2 / backup file offset 1536)
$fs.Position = 2*4096; $ent = New-Object byte[] 16384; [void]$fs.Read($ent,0,16384)
$bk = [IO.File]::ReadAllBytes((Join-Path $root 'backup\gpt_sda.bin'))
$same = $true; for ($i=0; $i -lt 16384; $i++) { if ($ent[$i] -ne $bk[1536+$i]) { $same=$false; break } }
Write-Host "GPT partition entries identical to backup: $same"
$fs.Close()
Write-Host "CHANGED partitions: $changed"

# ---- Copy Windows logs ----
$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$p35 = Get-Partition -DiskNumber $disk.Number -PartitionNumber 35
if ($p35.Size -eq $WIN_SIZE) {
  Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"; Start-Sleep 2
  $out = Join-Path $root ("windows_logs_" + (Get-Date -Format 'yyyyMMdd_HHmm')); New-Item -ItemType Directory -Force $out | Out-Null
  Copy-Item "${W}:\pipa-logs\*" $out -Force -ErrorAction SilentlyContinue
  foreach ($f in 'Windows\INF\setupapi.dev.log','Windows\INF\setupapi.offline.log','Windows\Panther\setupact.log','Windows\System32\winevt\Logs\System.evtx') { Copy-Item "${W}:\$f" $out -Force -ErrorAction SilentlyContinue }
  Copy-Item "${W}:\Windows\Minidump" (Join-Path $out 'Minidump') -Recurse -Force -ErrorAction SilentlyContinue
  reg load HKLM\PIPA_SYS "${W}:\Windows\System32\config\SYSTEM" | Out-Null
  reg export HKLM\PIPA_SYS\ControlSet001\Enum (Join-Path $out 'enum.reg') /y | Out-Null
  [gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SYS | Out-Null
  Get-ChildItem $out -Recurse | Select-Object Name,Length | Format-Table -AutoSize | Out-String | Write-Host
  Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"
}
Write-Host "`nPHASE 4B COMPLETE" -ForegroundColor Green
Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
