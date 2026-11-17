# Claude: step 1 of the QualcommPEP port - download it, make a test-signing cert, build the unmodified sm8845 branch (toolchain check).
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$src  = Join-Path $root 'QualcommPEP'
New-Item -ItemType Directory -Force (Join-Path $root 'logs') | Out-Null
Start-Transcript -Path (Join-Path $root 'logs\qpep_build.log') -Force | Out-Null
try {
  if (-not (Test-Path $src)) {
    if (Get-Command git -ErrorAction SilentlyContinue) {
      git clone --branch sm8845 https://github.com/NikkaGames/QualcommPEP $src
    } else {
      $zip = Join-Path $root 'qpep.zip'
      Invoke-WebRequest https://github.com/NikkaGames/QualcommPEP/archive/refs/heads/sm8845.zip -OutFile $zip
      Expand-Archive $zip $root -Force; Rename-Item (Join-Path $root 'QualcommPEP-sm8845') $src; Remove-Item $zip
    }
  }
  # test-signing certificate (self-signed, local; tablet runs with testsigning on)
  $pfx = Join-Path $root 'pipa_test_cert.pfx'
  if (-not (Test-Path $pfx)) {
    $c = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=pipa test signing' -CertStoreLocation Cert:\CurrentUser\My -NotAfter (Get-Date).AddYears(10)
    $pw = ConvertTo-SecureString 'pipa' -AsPlainText -Force
    Export-PfxCertificate -Cert $c -FilePath $pfx -Password $pw | Out-Null
    Export-Certificate -Cert $c -FilePath (Join-Path $root 'pipa_test_cert.cer') | Out-Null
    Write-Host "created $pfx"
  }
  # toolchain check only: this WDK's infverif rejects service-key values outside Parameters (error 1323) -> comment them out in the local copy
  $inx = Join-Path $src 'inf\qcpep.inx'
  $t = Get-Content $inx -Encoding Unicode
  $t = $t | ForEach-Object { if ($_ -match '^\s*HKR\s*,\s*(Resources|SystemLibResources|DcvsConfig)\s*,') { ';' + $_ } else { $_ } }
  Set-Content $inx $t -Encoding Unicode
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  $msbuild = Join-Path $vs 'MSBuild\Current\Bin\amd64\MSBuild.exe'
  Write-Host "MSBuild: $msbuild"
  Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" | Select-Object -ExpandProperty Name | Write-Host
  & $msbuild (Join-Path $src 'QcPep8845.sln') /m /p:Configuration=Release /p:Platform=ARM64 /p:SignMode=Off /v:minimal
  Write-Host "MSBuild exit code: $LASTEXITCODE"
  Get-ChildItem -Recurse (Join-Path $src 'ARM64') -Include *.sys,*.inf,*.cat -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "OUTPUT: $($_.FullName) $($_.Length)" }
} catch { Write-Host "ERROR: $_" -ForegroundColor Red }
Stop-Transcript | Out-Null
Read-Host 'Enter'
