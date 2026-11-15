# Phase 4c (Claude): READ-ONLY raw dump of changed LUN0 areas while UEFI Mass Storage is active.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
Start-Transcript -Path (Join-Path $root 'logs\phase4c.log') -Force | Out-Null
$UFS_SIZE = 253461790720
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk) { Write-Host 'ERROR: tablet disk not visible (is the green Mass Storage screen still up?)'; Stop-Transcript|Out-Null; Read-Host 'Enter'; exit 1 }
$out = Join-Path $root 'changed_now'; New-Item -ItemType Directory -Force $out | Out-Null
$fs = [IO.File]::Open("\\.\PHYSICALDRIVE$($disk.Number)", 'Open', 'Read', 'ReadWrite')
function Dump($name, [int64]$off, [int64]$len) {
  $fs.Position = $off; $buf = New-Object byte[] $len; $r = 0
  while ($r -lt $len) { $n = $fs.Read($buf, $r, [int][Math]::Min(4MB, $len - $r)); if ($n -le 0) { break }; $r += $n }
  [IO.File]::WriteAllBytes((Join-Path $out $name), $buf); Write-Host ("{0,-22} off={1} len={2} read={3}" -f $name,$off,$len,$r)
}
Dump 'gpt_primary.bin' 0 (6*4096)
Dump 'gpt_backup_tail.bin' ($UFS_SIZE - 5*4096) (5*4096)
foreach ($pn in 11,13,15,22,26) {
  $p = Get-Partition -DiskNumber $disk.Number -PartitionNumber $pn
  Dump ("p{0}.img" -f $pn) $p.Offset $p.Size
}
$fs.Close()
Get-Partition -DiskNumber $disk.Number | Select-Object PartitionNumber,Offset,Size,GptType,IsHidden,IsSystem,NoDefaultDriveLetter,DriveLetter | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
Write-Host "`nPHASE 4C COMPLETE - you can press Volume Up on the tablet now" -ForegroundColor Green
Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
