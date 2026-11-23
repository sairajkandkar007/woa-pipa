# Build the unmodified upstream SM8845 branch as a QualcommPEP toolchain check.
$ErrorActionPreference = 'Stop'
$buildRoot = $PSScriptRoot
$driverRoot = Split-Path $buildRoot -Parent
$src = Join-Path $driverRoot 'upstream-sm8845'
New-Item -ItemType Directory -Force (Join-Path $buildRoot 'logs') | Out-Null
Start-Transcript -Path (Join-Path $buildRoot 'logs\qpep_build.log') -Force | Out-Null
try {
  if (-not (Test-Path $src)) {
    if (Get-Command git -ErrorAction SilentlyContinue) { git clone --branch sm8845 https://github.com/NikkaGames/QualcommPEP $src }
    else {
      $zip = Join-Path $buildRoot 'qpep.zip'
      Invoke-WebRequest https://github.com/NikkaGames/QualcommPEP/archive/refs/heads/sm8845.zip -OutFile $zip
      Expand-Archive $zip $buildRoot -Force
      Move-Item (Join-Path $buildRoot 'QualcommPEP-sm8845') $src
      Remove-Item $zip
    }
  }
  $repoRoot = (Resolve-Path (Join-Path $buildRoot '..\..\..')).Path
  $pfx = Join-Path $repoRoot 'pipa_test_cert.pfx'
  if (-not (Test-Path $pfx)) {
    $c = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=pipa test signing' -CertStoreLocation Cert:\CurrentUser\My -NotAfter (Get-Date).AddYears(10)
    $pw = ConvertTo-SecureString 'pipa' -AsPlainText -Force
    Export-PfxCertificate -Cert $c -FilePath $pfx -Password $pw | Out-Null
    Export-Certificate -Cert $c -FilePath (Join-Path $repoRoot 'pipa_test_cert.cer') | Out-Null
    Write-Host "created $pfx"
  }
  $inx = Join-Path $src 'inf\qcpep.inx'
  $t = Get-Content $inx -Encoding Unicode
  $t = $t | ForEach-Object { if ($_ -match '^\s*HKR\s*,\s*(Resources|SystemLibResources|DcvsConfig)\s*,') { ';' + $_ } else { $_ } }
  Set-Content $inx $t -Encoding Unicode
  $vswhere = "\${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  if (-not $vs) { throw 'Visual Studio with MSBuild was not found.' }
  $msbuild = Join-Path $vs 'MSBuild\Current\Bin\amd64\MSBuild.exe'
  Write-Host "MSBuild: $msbuild"
  Get-ChildItem "\${env:ProgramFiles(x86)}\Windows Kits\10\Include" | Select-Object -ExpandProperty Name | Write-Host
  & $msbuild (Join-Path $src 'QcPep8845.sln') /m /p:Configuration=Release /p:Platform=ARM64 /p:SignMode=Off /v:minimal
  Write-Host "MSBuild exit code: $LASTEXITCODE"
  Get-ChildItem -Recurse (Join-Path $src 'ARM64') -Include *.sys,*.inf,*.cat -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "OUTPUT: $($_.FullName) $($_.Length)" }
} catch { Write-Host "ERROR: $_" -ForegroundColor Red; throw }
finally { Stop-Transcript | Out-Null }
Read-Host 'Enter'
