# Phase 5 (Claude): inject a driver batch OFFLINE into tablet Windows (tablet in UEFI Mass Storage: Vol Down at Silicium screen).
# Backs up SYSTEM/SOFTWARE/DRIVERS hives first so the batch can be undone with RUN_PHASE5_UNDO.
param([string]$Batch = 'batch1.txt', [string[]]$ReEnumerate = @(), [string]$SystemReg = '')
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$stamp = Get-Date -Format 'yyyyMMdd_HHmm'
Start-Transcript -Path (Join-Path $root "logs\phase5_$($Batch -replace '\.txt','')_$stamp.log") -Force | Out-Null
$UFS_SIZE = 253461790720; $WIN_SIZE = 241330814976
function Fail($t) { Write-Host "ERROR: $t" -ForegroundColor Red; Stop-Transcript | Out-Null; Read-Host 'Enter'; exit 1 }
Update-HostStorageCache
$disk = Get-Disk | Where-Object { $_.Size -eq $UFS_SIZE -and $_.BusType -eq 'USB' }
if (-not $disk -or $disk.PartitionStyle -ne 'GPT') { Fail 'Tablet disk not found as GPT (is Mass Storage active?). Never initialize.' }
$p35 = Get-Partition -DiskNumber $disk.Number -PartitionNumber 35
if ($p35.Size -ne $WIN_SIZE) { Fail 'p35 size mismatch' }
$used = (Get-PSDrive -PSProvider FileSystem).Name
$W = @('W','V','U','R','Q','P') | Where-Object { $used -notcontains $_ } | Select-Object -First 1
$existing = (Get-Partition -DiskNumber $disk.Number -PartitionNumber 35).DriveLetter
if ($existing -and [int][char]$existing -ne 0) { $W = [string]$existing; Write-Host "Reusing existing drive letter ${W}:" }
else { Add-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\" }; Start-Sleep 2
if (-not (Test-Path "${W}:\Windows\System32\config\SYSTEM")) { Fail 'Windows not found on p35' }

Copy-Item "${W}:\pipa-logs" (Join-Path $root "pipa_logs_$stamp") -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $root 'tablet_bins') | Out-Null
foreach ($b in 'msgpioclx.sys','spbcx.sys','acpi.sys','wdf01000.sys') { Copy-Item "${W}:\Windows\System32\drivers\$b" (Join-Path $root 'tablet_bins') -Force -ErrorAction SilentlyContinue }
# 1. Snapshot the WHOLE registry folder (hives + .LOG1/.LOG2 + txr) so undo is consistent
$cfg = "${W}:\Windows\System32\config"
$bk = Join-Path $root "config_snapshot_$stamp"; New-Item -ItemType Directory -Force $bk | Out-Null
robocopy $cfg $bk /MIR /B /R:1 /W:1 /NFL /NDL /NP | Out-Null
Write-Host "Snapshot: $bk  ($((Get-ChildItem $bk -Recurse -Force | Measure-Object Length -Sum).Sum) bytes)"
Set-Content (Join-Path $root 'LAST_CONFIG_SNAPSHOT.txt') $bk

# 1b. Keep blue screens on screen + small dumps
reg load HKLM\PIPA_SYS "$cfg\SYSTEM" | Out-Null
reg add HKLM\PIPA_SYS\ControlSet001\Control\CrashControl /v AutoReboot /t REG_DWORD /d 0 /f | Out-Null
reg add HKLM\PIPA_SYS\ControlSet001\Control\CrashControl /v CrashDumpEnabled /t REG_DWORD /d 3 /f | Out-Null
reg add HKLM\PIPA_SYS\ControlSet001\Control\CrashControl /v DisplayParameters /t REG_DWORD /d 1 /f | Out-Null
[gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SYS | Out-Null

# 1c. pipa-logs autorun: dump device state at every logon (C:\pipa-logs)
New-Item -ItemType Directory -Force "${W}:\pipa-logs" | Out-Null
Set-Content -Encoding ASCII "${W}:\pipa-logs\dump.cmd" "@echo off`r`npnputil /enum-devices /problem > C:\pipa-logs\problem-devices.txt 2>&1`r`npnputil /enum-devices /drivers > C:\pipa-logs\all-devices.txt 2>&1`r`necho %date% %time% > C:\pipa-logs\last-run.txt"
reg load HKLM\PIPA_SW "${W}:\Windows\System32\config\SOFTWARE" | Out-Null
reg add HKLM\PIPA_SW\Microsoft\Windows\CurrentVersion\Run /v PipaLogs /t REG_SZ /d "cmd /c C:\pipa-logs\dump.cmd" /f | Out-Null
[gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SW | Out-Null
Write-Host 'pipa-logs autorun installed'

# 2. Inject drivers
$list = Get-Content (Join-Path $root $Batch) | Where-Object { $_ -and -not $_.StartsWith('#') }
foreach ($rel in $list) {
  $dir = Join-Path $root "kona_bsp\Drivers\$rel"
  Write-Host "`n### Add-Driver $rel" -ForegroundColor Cyan
  dism /English /Image:"${W}:\" /Add-Driver /Driver:"$dir" /Recurse /ForceUnsigned | Select-String -Pattern 'Installing|successfully|Error|error|failed' | ForEach-Object { Write-Host $_ }
}
# 3. Mark devices for reinstall (ConfigFlags = CONFIGFLAG_REINSTALL) so Windows installs the best driver at next boot.
#    Enum keys are SYSTEM-only, so open them with SeRestorePrivilege + REG_OPTION_BACKUP_RESTORE.
$ReEnumerate = @($ReEnumerate | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($ReEnumerate.Count -gt 0) {
  if (-not ('PipaReg' -as [type])) {
  Add-Type @"
using System; using System.Runtime.InteropServices;
public static class PipaReg {
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegOpenKeyEx(UIntPtr h, string sub, int opt, int sam, out UIntPtr res);
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegSetValueEx(UIntPtr h, string name, int r, int type, byte[] data, int len);
  [DllImport("advapi32.dll")] static extern int RegCloseKey(UIntPtr h);
  [DllImport("advapi32.dll", SetLastError=true)] static extern bool OpenProcessToken(IntPtr p, int acc, out IntPtr t);
  [DllImport("advapi32.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern bool LookupPrivilegeValue(string s, string n, out long luid);
  [DllImport("advapi32.dll", SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr t, bool d, ref TP n, int l, IntPtr p, IntPtr r);
  [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
  [StructLayout(LayoutKind.Sequential, Pack=4)] struct TP { public int Count; public long Luid; public int Attr; }
  public static void Priv(string n) { IntPtr t; OpenProcessToken(GetCurrentProcess(), 0x28, out t); TP tp = new TP(); tp.Count=1; tp.Attr=2; LookupPrivilegeValue(null, n, out tp.Luid); AdjustTokenPrivileges(t, false, ref tp, 0, IntPtr.Zero, IntPtr.Zero); }
  public static int SetDword(string sub, string name, int val) {
    UIntPtr h; int rc = RegOpenKeyEx(new UIntPtr(0x80000002u), sub, 4 /*REG_OPTION_BACKUP_RESTORE*/, 0x20006 /*KEY_WRITE*/, out h);
    if (rc != 0) return rc; rc = RegSetValueEx(h, name, 0, 4, BitConverter.GetBytes(val), 4); RegCloseKey(h); return rc; }
}
"@
  }
  [PipaReg]::Priv('SeRestorePrivilege'); [PipaReg]::Priv('SeBackupPrivilege')
  reg load HKLM\PIPA_SYS "${W}:\Windows\System32\config\SYSTEM" | Out-Null
  foreach ($e in $ReEnumerate) {
    $rc = [PipaReg]::SetDword("PIPA_SYS\ControlSet001\Enum\$e", 'ConfigFlags', 0x20)
    Write-Host "Reinstall flag on $e : $(if ($rc -eq 0) {'OK'} else {"FAILED rc=$rc"})"
  }
  [gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SYS | Out-Null
}
# 3b. Import extra SYSTEM-hive settings (a .reg file whose keys start with HKEY_LOCAL_MACHINE\PIPA_SYS\ControlSet001)
if ($SystemReg) {
  reg load HKLM\PIPA_SYS "${W}:\Windows\System32\config\SYSTEM" | Out-Null
  reg import (Join-Path $root $SystemReg); Write-Host "Imported $SystemReg (exit $LASTEXITCODE)"
  [gc]::Collect(); Start-Sleep 2; reg unload HKLM\PIPA_SYS | Out-Null
}
dism /English /Image:"${W}:\" /Get-Drivers /Format:Table | Out-File (Join-Path $root "logs\drivers_after_$stamp.txt")
Remove-PartitionAccessPath -DiskNumber $disk.Number -PartitionNumber 35 -AccessPath "${W}:\"
Write-Host "`nBATCH DONE ($Batch). Hive backup: $bk" -ForegroundColor Green
Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
