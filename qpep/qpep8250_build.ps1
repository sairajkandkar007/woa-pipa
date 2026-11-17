# Claude: build the SM8250 port of QualcommPEP (pep/QualcommPEP in this repo) and test-sign it.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$out  = Join-Path $root 'out8250'
New-Item -ItemType Directory -Force (Join-Path $root 'logs'), $out | Out-Null
Start-Transcript -Path (Join-Path $root 'logs\qpep8250_build.log') -Force | Out-Null
try {
  # fresh copy of the repo (main branch) - contains pep\QualcommPEP with the 8250 target
  $zip = Join-Path $root 'woa-pipa.zip'; $repo = Join-Path $root 'woa-pipa-main'
  if (Test-Path $repo) { Remove-Item -Recurse -Force $repo }
  Invoke-WebRequest https://github.com/nishant713/woa-pipa/archive/refs/heads/main.zip -OutFile $zip
  Expand-Archive $zip $root -Force; Remove-Item $zip
  $src = Join-Path $repo 'pep\QualcommPEP'
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  $msbuild = Join-Path $vs 'MSBuild\Current\Bin\amd64\MSBuild.exe'
  & $msbuild (Join-Path $src 'QcPep8250.sln') /m /p:Configuration=Release /p:Platform=ARM64 /p:SignMode=Off /v:minimal
  Write-Host "MSBuild exit code: $LASTEXITCODE"
  if ($LASTEXITCODE -ne 0) { throw 'build failed - send logs\qpep8250_build.log' }
  $files = Get-ChildItem -Recurse (Join-Path $src 'ARM64') -Include qcpep.sys,qcpep8250.inf,qcpep.cat,*.inf
  $files | ForEach-Object { Write-Host "OUTPUT: $($_.FullName) $($_.Length)"; Copy-Item $_.FullName $out -Force }
  $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe | Where-Object FullName -match '\\x64\\' | Select-Object -Last 1
  foreach ($f in @('qcpep.sys','qcpep.cat')) {
    $p = Join-Path $out $f
    if (Test-Path $p) { & $signtool.FullName sign /fd sha256 /f (Join-Path $root 'pipa_test_cert.pfx') /p pipa $p; Write-Host "signed $f exit $LASTEXITCODE" }
  }
  Write-Host "DONE -> $out"
} catch { Write-Host "ERROR: $_" -ForegroundColor Red }
Stop-Transcript | Out-Null
Read-Host 'Enter'
