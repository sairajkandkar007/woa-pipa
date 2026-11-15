param([string]$Img = 'Mu-pipa-main-fix3-konaIDs-gpio1.img', [string]$ExpectMd5 = '5D2F674A5807FE91220DD24242517C4B', [switch]$Restore)
# Phase 6 (Claude): FLASH the UEFI into boot_a (slot a only). -Restore writes the original boot_a backup back.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
New-Item -ItemType Directory -Force (Join-Path $root 'logs') | Out-Null
Start-Transcript -Path (Join-Path $root 'logs\phase6_flash.log') -Append | Out-Null
$adb = 'C:\platform-tools\adb.exe'; $fb = 'C:\platform-tools\fastboot.exe'
function Stop-Here($m) { Write-Host "ERROR: $m" -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host 'Nothing was flashed. Press Enter'; exit 1 }
if ($Restore) { $Img = 'backup\boot_a.img'; $ExpectMd5 = 'A9D6909D00429EE2DBD4985E23FC0130' }
$img = Join-Path $root $Img
if (-not (Test-Path $img)) { Stop-Here "image not found: $img" }
$md5 = (Get-FileHash -Algorithm MD5 $img).Hash
$size = (Get-Item $img).Length
Write-Host "Image: $img`nSize : $size bytes`nMD5  : $md5"
if ($md5 -ne $ExpectMd5) { Stop-Here "MD5 mismatch (expected $ExpectMd5)" }
if ($size -gt 201326592) { Stop-Here 'image larger than boot_a (192 MiB)' }
$st = & $adb get-state 2>&1
if ($st -match 'recovery|device') { & $adb reboot bootloader }
Write-Host 'Waiting for fastboot (if the tablet is off/in Windows: hold Power to switch off, then Power + Volume Down)...'
$ok = $false
for ($i=0; $i -lt 90; $i++) { if ((& $fb devices 2>&1) -match 'fastboot') { $ok = $true; break }; Start-Sleep 2 }
if (-not $ok) { Stop-Here 'no fastboot device found' }
$v = cmd /c "`"$fb`" getvar all 2>&1" | Out-String
$prod = [regex]::Match($v, 'product:\s*(\S+)').Groups[1].Value
$unl  = [regex]::Match($v, 'unlocked:\s*(\S+)').Groups[1].Value
$slot = [regex]::Match($v, 'current-slot:\s*(\S+)').Groups[1].Value
$psz  = [regex]::Match($v, 'partition-size:boot_a:\s*(\S+)').Groups[1].Value
Write-Host "product=$prod unlocked=$unl current-slot=$slot boot_a size=$psz"
if ($prod -ne 'pipa') { Stop-Here 'not a pipa' }
if ($unl -ne 'yes') { Stop-Here 'bootloader not unlocked' }
if ($slot -ne 'a') { Stop-Here "current slot is '$slot', expected 'a'" }
if ($psz -and ([Convert]::ToInt64($psz, 16) -ne 201326592)) { Stop-Here "unexpected boot_a size $psz" }
$what = if ($Restore) { 'the ORIGINAL boot_a backup (XDA TWRP)' } else { 'the Mu-Silicium UEFI' }
Write-Host "`nThis will write $what to boot_a. boot_b, vbmeta and everything else stay untouched." -ForegroundColor Yellow
if ((Read-Host 'Type FLASH to continue') -ne 'FLASH') { Stop-Here 'cancelled by user' }
cmd /c "`"$fb`" flash boot_a `"$img`" 2>&1"
if ($LASTEXITCODE -ne 0) { Write-Host 'Flash FAILED. The tablet is still in fastboot; run RUN_PHASE6_RESTORE_BOOT_A.bat or ask Claude.' -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host 'Enter'; exit 1 }
Write-Host 'Flash OK. Rebooting...' -ForegroundColor Green
cmd /c "`"$fb`" reboot 2>&1"
Write-Host "`nDONE. The tablet should now show the Silicium logo and boot Windows by itself on every power-on."
Write-Host 'Recovery: Power + Volume Down at power-on always reaches fastboot (fastboot is not touched).'
Stop-Transcript | Out-Null
Read-Host 'Press Enter to close'
