param(
  [Parameter(Mandatory = $true)]
  [string]$InstallerPath
)

$ErrorActionPreference = "Stop"
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$installDir = Join-Path $env:LOCALAPPDATA "OnclawTdxThs"
$agent = Join-Path $installDir "onclaw-tdx-ths-agent.exe"
$uninstaller = Join-Path $installDir "uninstall.exe"
$protocolKey = "Registry::HKEY_CURRENT_USER\Software\Classes\onclaw-tdx-ths"

if (Test-Path -LiteralPath $protocolKey) {
  throw "Refusing to replace an existing onclaw-tdx-ths protocol registration"
}
if (Test-Path -LiteralPath $installDir) {
  throw "Refusing to replace an existing Agent installation at $installDir"
}

try {
  $install = Start-Process -FilePath $installer -ArgumentList "/S" -WindowStyle Hidden -Wait -PassThru
  if ($install.ExitCode -ne 0) {
    throw "Installer exited with code $($install.ExitCode)"
  }
  if (-not (Test-Path -LiteralPath $agent) -or -not (Test-Path -LiteralPath $uninstaller)) {
    throw "Installer did not create the expected Agent and uninstaller"
  }

  & $agent --verify-registration
  if ($LASTEXITCODE -ne 0) {
    throw "Installed Agent failed protocol registration verification with code $LASTEXITCODE"
  }

  $commandKey = Join-Path $protocolKey "shell\open\command"
  $actualCommand = (Get-Item -LiteralPath $commandKey).GetValue("")
  $expectedCommand = '"' + $agent + '" "%1"'
  if ($actualCommand -ne $expectedCommand) {
    throw "Protocol command mismatch: $actualCommand"
  }
} finally {
  if (Test-Path -LiteralPath $uninstaller) {
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList "/S" -WindowStyle Hidden -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) {
      Write-Error "Uninstaller exited with code $($uninstall.ExitCode)"
    }
  }
}

$deadline = (Get-Date).AddSeconds(10)
while (((Test-Path -LiteralPath $protocolKey) -or (Test-Path -LiteralPath $installDir)) -and
       (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 200
}
if (Test-Path -LiteralPath $protocolKey) {
  throw "Uninstaller left the custom protocol registration behind"
}
if (Test-Path -LiteralPath $installDir) {
  throw "Uninstaller left the Agent installation directory behind"
}

Write-Host "Agent installer registration lifecycle: OK"
