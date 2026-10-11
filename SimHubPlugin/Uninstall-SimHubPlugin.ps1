$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait
    exit
}

$path = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'SimHub'
if (-not (Test-Path (Join-Path $path 'SimHub.Plugins.dll'))) {
    $path = Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'SimHub'
}
if (Get-Process -Name 'SimHubWPF' -ErrorAction SilentlyContinue) {
    Write-Host 'Close SimHub first, then run this script again.' -ForegroundColor Yellow
    Read-Host 'Press Enter to close'
    exit 2
}
$dll = Join-Path $path 'ETS2RpmUdpBridge.dll'
if (Test-Path $dll) {
    Remove-Item $dll -Force
    Write-Host 'ETS2RpmUdpBridge.dll removed. Any backup files were left untouched.' -ForegroundColor Green
} else {
    Write-Host 'Plugin DLL was not found; nothing to remove.'
}
Read-Host 'Press Enter to close'
