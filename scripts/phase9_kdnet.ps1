# Claude: turn Windows kernel debugging over USB (KDNET-EEM, Qualcomm kd_8003_5143.dll) on/off in the tablet's BCD.
# Tablet must be in UEFI Mass Storage mode. Only the BCD debug settings of the Windows entry are changed.
param([switch]$Off)
$root = $PSScriptRoot; $UFS_SIZE = 253461790720; $ESP_SIZE = 1073741824
New-Item -ItemType Directory -Force (Join-Path $root 'logs') | Out-Null
Start-Transcript -Path (Join-Path $root 'logs\phase9_kdnet.log') -Force | Out-Null
function Fail($t) { Write-Host "ERROR: $t" -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host 'Enter'; exit 1 }
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk) { Fail 'tablet disk not found (Mass Storage active?)' }
$p34 = Get-Partition -DiskNumber $disk.Number -PartitionNumber 34
if ($p34.Size -ne $ESP_SIZE) { Fail 'p34 size mismatch' }
$had = ($p34.DriveLetter -and [int][char]$p34.DriveLetter -ne 0)
$S = if ($had) { [string]$p34.DriveLetter } else {
  $used = (Get-PSDrive -PSProvider FileSystem).Name; $l = @('S','T','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
  "select disk $($disk.Number)`r`nselect partition 34`r`nassign letter=$l`r`nexit`r`n" | Set-Content -Encoding ASCII "$env:TEMP\pipa_dp9.txt"; diskpart /s "$env:TEMP\pipa_dp9.txt" | Out-Null; Start-Sleep 3; $l }
$bcd = "${S}:\EFI\Microsoft\Boot\BCD"
if (-not (Test-Path $bcd)) { Fail "BCD not found at $bcd" }
Copy-Item $bcd (Join-Path $root ("logs\BCD_backup_" + (Get-Date -Format 'yyyyMMdd_HHmmss'))) -Force
if ($Off) {
  bcdedit /store $bcd /debug '{default}' off | Write-Host
  Write-Host 'Kernel debugging OFF' -ForegroundColor Yellow
} else {
  bcdedit /store $bcd /dbgsettings NET HOSTIP:169.254.255.255 PORT:50000 KEY:1.2.3.4 NODHCP | Write-Host
  bcdedit /store $bcd /set '{dbgsettings}' busparams 1 | Write-Host
  bcdedit /store $bcd /debug '{default}' on | Write-Host
  Write-Host 'Kernel debugging ON (USB EEM, port 50000, key 1.2.3.4)' -ForegroundColor Green
}
bcdedit /store $bcd /dbgsettings | Write-Host
bcdedit /store $bcd /enum '{default}' | Write-Host
if (-not $had) { "select disk $($disk.Number)`r`nselect partition 34`r`nremove letter=$S`r`nexit`r`n" | Set-Content -Encoding ASCII "$env:TEMP\pipa_dp9.txt"; diskpart /s "$env:TEMP\pipa_dp9.txt" | Out-Null }
Write-Host 'DONE' -ForegroundColor Green; Stop-Transcript | Out-Null; Read-Host 'Enter'
