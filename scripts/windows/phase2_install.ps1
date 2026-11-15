# Phase 2 (Claude): Install Windows 11 to pipa via TWRP mass storage.
# DESTRUCTIVE ONLY for tablet partitions 34 (esp) and 35 (win), after exact size checks + typed YES.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$ws   = Split-Path $root -Parent
Start-Transcript -Path (Join-Path $root 'logs\phase2.log') -Force | Out-Null
$adb = 'C:\platform-tools\adb.exe'
$wim = Join-Path $ws 'install_pro.wim'
$ESP_SIZE = 1073741824          # p34: 262144 x 4096
$WIN_SIZE = 241330814976        # p35: 58918656 x 4096
function Step($t) { Write-Host "`n==== $t ====" -ForegroundColor Cyan }
function Fail($t) { Write-Host "ERROR: $t" -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host 'Press Enter to exit'; exit 1 }

Step 'Check WIM'
if (-not (Test-Path $wim)) { Fail "install_pro.wim not found at $wim" }
$info = dism /English /Get-WimInfo /WimFile:$wim
$info | Write-Host
$idx = $null; $cur = $null
foreach ($l in $info) { if ($l -match 'Index : (\d+)') { $cur = $Matches[1] }; if ($l -match 'Name : Windows 11 Pro$') { $idx = $cur } }
if (-not $idx) { $idx = 1; Write-Host 'Pro not found by name; using index 1' -ForegroundColor Yellow }
Write-Host "Using image index $idx"

Step 'Start mass storage on tablet (whole sda)'
# Whole UFS LUN0 = 61880320 x 4096 = 253461790720 bytes. Anything smaller is a partition-only export.
$UFS_SIZE = 253461790720
function FindDisk { Update-HostStorageCache; Get-Disk | Where-Object { $_.LogicalSectorSize -eq 4096 -and $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' } }
$disk = FindDisk
if (-not $disk) {
  if ((& $adb get-state 2>&1) -notmatch 'recovery') { Fail 'adb cannot reach TWRP to reconfigure mass storage. Reboot the tablet into recovery (Power+Vol Up) and run this again.' }
  & $adb push (Join-Path $root 'msc_sda.sh') /tmp/msc_sda.sh
  & $adb shell 'chmod 755 /tmp/msc_sda.sh; nohup sh /tmp/msc_sda.sh > /tmp/msc.log 2>&1 &'
  for ($i=0; $i -lt 40 -and -not $disk; $i++) { Start-Sleep 2; $disk = FindDisk }
}
if (-not $disk) { Fail 'Tablet UFS did not appear as a disk. Do NOT initialize any disk Windows offers.' }
if (@($disk).Count -ne 1) { Fail 'More than one candidate disk; aborting' }
$disk | Format-List Number,FriendlyName,BusType,Size,LogicalSectorSize,PhysicalSectorSize,PartitionStyle,NumberOfPartitions | Out-String | Write-Host
if ($disk.PartitionStyle -ne 'GPT') { Fail "Disk shows as $($disk.PartitionStyle), not GPT. STOP. Never click Initialize." }
$n = $disk.Number
$p34 = Get-Partition -DiskNumber $n -PartitionNumber 34 -ErrorAction SilentlyContinue
$p35 = Get-Partition -DiskNumber $n -PartitionNumber 35 -ErrorAction SilentlyContinue
if (-not $p34 -or -not $p35) { Fail 'Partitions 34/35 not found' }
Write-Host ("p34 size {0}  type {1}" -f $p34.Size, $p34.GptType)
Write-Host ("p35 size {0}  type {1}" -f $p35.Size, $p35.GptType)
if ($p34.Size -ne $ESP_SIZE -or $p35.Size -ne $WIN_SIZE) { Fail 'Partition sizes do not match the verified layout. Aborting without changes.' }

$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$S = @('S','T','Y','O','N','M') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
Write-Host "`nAbout to FORMAT on disk $n ($($disk.FriendlyName)):" -ForegroundColor Yellow
Write-Host "  partition 35 (win, 224.7 GiB) -> NTFS 'Windows' as ${W}:" -ForegroundColor Yellow
Write-Host "  partition 34 (esp, 1 GiB)     -> FAT32 'ESP' as ${S}:" -ForegroundColor Yellow
Write-Host '  Nothing else on the disk is touched.' -ForegroundColor Yellow
if ((Read-Host 'Type YES to continue') -cne 'YES') { Fail 'Cancelled by user' }

Step 'Format + assign letters (diskpart)'
$dp = Join-Path $env:TEMP 'pipa_dp.txt'
@"
select disk $n
select partition 35
format fs=ntfs quick label=Windows
assign letter=$W
select partition 34
format fs=fat32 quick label=ESP
assign letter=$S
exit
"@ | Set-Content -Encoding ASCII $dp
diskpart /s $dp | Write-Host
if (-not (Test-Path "${W}:\") -or -not (Test-Path "${S}:\")) { Fail 'Drive letters not available after diskpart' }

Step "Apply image (index $idx) to ${W}:  -- this takes 15-40 min"
dism /English /Apply-Image /ImageFile:$wim /Index:$idx /ApplyDir:"${W}:\"
if ($LASTEXITCODE -ne 0) { Fail "DISM apply failed ($LASTEXITCODE)" }

Step 'Unattend + offline registry'
New-Item -ItemType Directory -Force "${W}:\Windows\Panther" | Out-Null
Copy-Item (Join-Path $root 'unattend.xml') "${W}:\Windows\Panther\unattend.xml" -Force
reg load HKLM\PIPA_SYS "${W}:\Windows\System32\config\SYSTEM" | Write-Host
reg add HKLM\PIPA_SYS\ControlSet001\Control\USB /v OsDefaultRoleSwitchMode /t REG_DWORD /d 1 /f | Write-Host
reg add HKLM\PIPA_SYS\ControlSet001\Control\BitLocker /v PreventDeviceEncryption /t REG_DWORD /d 1 /f | Write-Host
[gc]::Collect(); Start-Sleep 2
reg unload HKLM\PIPA_SYS | Write-Host

Step 'Boot files (bcdboot) + BCD options'
bcdboot "${W}:\Windows" /s "${S}:" /f UEFI | Write-Host
if ($LASTEXITCODE -ne 0) { Fail "bcdboot failed ($LASTEXITCODE)" }
$bcd = "${S}:\EFI\Microsoft\Boot\BCD"
bcdedit /store $bcd /set '{default}' testsigning on       | Write-Host
bcdedit /store $bcd /set '{default}' nointegritychecks on | Write-Host
bcdedit /store $bcd /set '{default}' recoveryenabled no   | Write-Host
bcdedit /store $bcd /set '{default}' bootstatuspolicy IgnoreAllFailures | Write-Host
bcdedit /store $bcd /enum all | Write-Host

Step 'Detach drive letters'
@"
select disk $n
select partition 35
remove letter=$W
select partition 34
remove letter=$S
exit
"@ | Set-Content -Encoding ASCII $dp
diskpart /s $dp | Write-Host

Write-Host "`nINSTALL COMPLETE" -ForegroundColor Green
Stop-Transcript | Out-Null
Read-Host 'Press Enter to close'
