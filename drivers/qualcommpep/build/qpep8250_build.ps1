# Build and test-sign the SM8250 QualcommPEP port included in this repository.
$ErrorActionPreference = 'Stop'
$buildRoot = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $buildRoot '..\..\..')).Path
$src = Join-Path $repoRoot 'drivers\qualcommpep\QualcommPEP'
$out = Join-Path $buildRoot 'out8250'
New-Item -ItemType Directory -Force (Join-Path $buildRoot 'logs'), $out | Out-Null
Start-Transcript -Path (Join-Path $buildRoot 'logs\qpep8250_build.log') -Force | Out-Null
try {
  if (-not (Test-Path (Join-Path $src 'QcPep8250.sln'))) { throw "SM8250 solution not found: $src" }
  $vswhere = "\${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  if (-not $vs) { throw 'Visual Studio with MSBuild was not found.' }
  $msbuild = Join-Path $vs 'MSBuild\Current\Bin\amd64\MSBuild.exe'
  & $msbuild (Join-Path $src 'QcPep8250.sln') /m /p:Configuration=Release /p:Platform=ARM64 /p:SignMode=Off /v:minimal
  Write-Host "MSBuild exit code: $LASTEXITCODE"
  if ($LASTEXITCODE -ne 0) { throw 'build failed - inspect logs\qpep8250_build.log' }
  $files = Get-ChildItem -Recurse (Join-Path $src 'ARM64') -Include qcpep.sys,qcpep8250.inf,qcpep.cat,*.inf
  $files | ForEach-Object { Write-Host "OUTPUT: $($_.FullName) $($_.Length)"; Copy-Item $_.FullName $out -Force }
  $signtool = Get-ChildItem "\${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe | Where-Object FullName -match '\\x64\\' | Select-Object -Last 1
  $cert = Join-Path $repoRoot 'pipa_test_cert.pfx'
  foreach ($f in @('qcpep.sys','qcpep.cat')) {
    $p = Join-Path $out $f
    if ((Test-Path $p) -and (Test-Path $cert) -and $signtool) { & $signtool.FullName sign /fd sha256 /f $cert /p pipa $p; Write-Host "signed $f exit $LASTEXITCODE" }
  }
  Write-Host "DONE -> $out"
} catch { Write-Host "ERROR: $_" -ForegroundColor Red; throw }
finally { Stop-Transcript | Out-Null }
Read-Host 'Enter'
