# Phase 0b (Claude): READ-ONLY. Finish + verify partition backups, print partition tables.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$bk   = Join-Path $root 'backup'
Start-Transcript -Path (Join-Path $root 'logs\phase0b.log') -Force | Out-Null
$adb = 'C:\platform-tools\adb.exe'
Write-Host "STATE: $(& $adb get-state 2>&1)"

# Remove the partial whole-disk dump (it was never meant to be taken)
$partial = Join-Path $bk 'sda.img'
if (Test-Path $partial) { Remove-Item $partial -Force; Write-Host 'Removed partial sda.img' }

# Partition tables (this TWRP's sgdisk lacks -p; use long options + parted)
foreach ($l in 'a','b','c','d','e','f') {
  Write-Host "`n### LUN sd$l"
  & $adb shell "sgdisk --print /dev/block/sd$l 2>&1; echo ---; parted -s /dev/block/sd$l unit s print 2>&1"
}

# Partition list with 64-bit-safe sizes computed on the PC: name dev sectors512
$raw = & $adb shell 'cd /dev/block/by-name && for p in *; do d=$(readlink -f $p); n=${d##*/}; echo "$p $n $(cat /sys/class/block/$n/size) $(cat /sys/class/block/$n/partition 2>/dev/null)"; done'
$parts = foreach ($line in $raw) {
  $f = $line -split '\s+'; if ($f.Count -lt 4) { continue }   # 4th field present only for real partitions (not whole disks)
  [pscustomobject]@{ Name=$f[0]; Dev=$f[1]; Size=([int64]$f[2])*512 }
}
$parts | ForEach-Object { "{0} {1} {2}" -f $_.Name,$_.Dev,$_.Size } | Set-Content (Join-Path $bk 'partition_list_64bit.txt')

$skip = @('userdata','esp','win','super','cache','metadata','sda34','sda35','cust')
$max = 512MB   # includes modem_a/b (448 MiB) for a complete firmware backup
$results = @()
foreach ($p in $parts) {
  if ($skip -contains $p.Name -or $p.Size -le 0 -or $p.Size -gt $max) { continue }
  $out = Join-Path $bk "$($p.Name).img"
  if (-not ((Test-Path $out) -and ((Get-Item $out).Length -eq $p.Size))) {
    cmd /c "`"$adb`" exec-out `"dd if=/dev/block/$($p.Dev) bs=1048576 2>/dev/null`" > `"$out`""
  }
  $got = (Get-Item $out).Length
  $devmd5 = ((& $adb shell "md5sum /dev/block/$($p.Dev)") -split '\s+')[0]
  $pcmd5  = (Get-FileHash -Algorithm MD5 $out).Hash.ToLower()
  $st = if (($got -eq $p.Size) -and ($devmd5 -eq $pcmd5)) {'OK'} else {'MISMATCH'}
  $r = "{0,-20} {1,-7} {2,11} {3,11} {4} {5}" -f $p.Name,$p.Dev,$p.Size,$got,$st,$pcmd5
  Write-Host $r; $results += $r
}
$results | Set-Content (Join-Path $bk 'backup_results.txt')
Write-Host ("`nTOTAL {0}  MISMATCH {1}" -f $results.Count, ($results | Where-Object { $_ -match 'MISMATCH' }).Count)
Write-Host "DONE"
Stop-Transcript | Out-Null
