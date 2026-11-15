# Claude: READ-ONLY. Show what Windows sees for USB disks (tablet mass storage).
Start-Transcript -Path (Join-Path $PSScriptRoot 'logs\diag_disks.log') -Force | Out-Null
Update-HostStorageCache
Get-Disk | Format-Table Number,FriendlyName,BusType,Size,LogicalSectorSize,PhysicalSectorSize,PartitionStyle,OperationalStatus,HealthStatus,NumberOfPartitions -AutoSize | Out-String -Width 250 | Write-Host
foreach ($d in (Get-Disk | Where-Object BusType -eq 'USB')) {
  Write-Host "`n### Disk $($d.Number) $($d.FriendlyName)"
  $d | Format-List * | Out-String -Width 250 | Write-Host
  Get-Partition -DiskNumber $d.Number -ErrorAction SilentlyContinue | Select-Object -First 40 PartitionNumber,Offset,Size,GptType | Format-Table -AutoSize | Out-String -Width 250 | Write-Host
  try {
    $fs = [IO.File]::Open("\\.\PHYSICALDRIVE$($d.Number)", 'Open', 'Read', 'ReadWrite')
    $buf = New-Object byte[] 8192; $n = $fs.Read($buf,0,8192); $fs.Close()
    Write-Host ("raw read {0} bytes; bytes 512-519: {1}; bytes 4096-4103: {2}" -f $n, [Text.Encoding]::ASCII.GetString($buf,512,8), [Text.Encoding]::ASCII.GetString($buf,4096,8))
  } catch { Write-Host "raw read FAILED: $($_.Exception.Message)" }
}
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'USBSTOR|USB\\VID_05C6|USB\\VID_18D1|USB\\VID_045E' } | Format-Table Status,Class,FriendlyName,InstanceId -AutoSize | Out-String -Width 250 | Write-Host
Stop-Transcript | Out-Null; Read-Host 'Press Enter to close'
