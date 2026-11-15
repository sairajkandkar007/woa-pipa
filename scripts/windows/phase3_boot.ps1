param([string]$Img = 'Mu-pipa-upstream-fb4efb3.img')
# Phase 3 (Claude): TEMPORARY boot of upstream UEFI (nothing is flashed).
$ErrorActionPreference = 'Continue'
Start-Transcript -Path (Join-Path $PSScriptRoot 'logs\phase3_boot.log') -Append | Out-Null
$root = $PSScriptRoot
$adb = 'C:\platform-tools\adb.exe'; $fb = 'C:\platform-tools\fastboot.exe'
$img = if ([IO.Path]::IsPathRooted($Img)) { $Img } else { Join-Path $root $Img }
Write-Host "Image: $img"
Write-Host "Image MD5: $((Get-FileHash -Algorithm MD5 $img).Hash)"
$st = & $adb get-state 2>&1
Write-Host "adb state: $st"
if ($st -match 'recovery|device') { & $adb reboot bootloader; Write-Host 'Rebooting to fastboot...' }
else { Write-Host 'adb not available: put the tablet in fastboot manually (hold Power + Vol Down).' }
$ok = $false
for ($i=0; $i -lt 60; $i++) { $d = & $fb devices 2>&1; if ($d -match 'fastboot') { $ok = $true; break }; Start-Sleep 2 }
if (-not $ok) { Write-Host 'ERROR: no fastboot device found'; Stop-Transcript | Out-Null; Read-Host 'Enter'; exit 1 }
& $fb devices
cmd /c "`"$fb`" getvar product 2>&1"
Write-Host "`nfastboot boot (temporary, nothing flashed)..."
cmd /c "`"$fb`" boot `"$img`" 2>&1"
Write-Host "`nDONE. Watch the tablet screen: Mu-Silicium logo -> Windows boot. First boot can take 5-15 min and may reboot once."
Stop-Transcript | Out-Null
Read-Host 'Press Enter to close'
