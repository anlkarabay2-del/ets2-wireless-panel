param(
    [string]$SimHubPath = ""
)
$ErrorActionPreference = 'Stop'

function Wait-And-Exit([int]$Code) {
    Write-Host ""
    Read-Host "Press Enter to close this window"
    exit $Code
}

# Re-launch elevated because SimHub is usually installed under Program Files (x86).
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    if (-not [String]::IsNullOrWhiteSpace($SimHubPath)) {
        $arguments += " -SimHubPath `"$SimHubPath`""
    }
    Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait
    exit
}

if ([String]::IsNullOrWhiteSpace($SimHubPath)) {
    $candidates = @(
        (Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'SimHub'),
        (Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'SimHub')
    )
    $SimHubPath = $candidates | Where-Object { Test-Path (Join-Path $_ 'SimHub.Plugins.dll') } | Select-Object -First 1
}

if ([String]::IsNullOrWhiteSpace($SimHubPath) -or -not (Test-Path (Join-Path $SimHubPath 'SimHub.Plugins.dll'))) {
    Write-Host "ERROR: Could not find SimHub.Plugins.dll. Re-run with -SimHubPath 'C:\Program Files (x86)\SimHub'." -ForegroundColor Red
    Wait-And-Exit 1
}
$SimHubPath = (Resolve-Path $SimHubPath).Path

if (Get-Process -Name 'SimHubWPF' -ErrorAction SilentlyContinue) {
    Write-Host "Close SimHub completely, then run Install-SimHubPlugin.bat again." -ForegroundColor Yellow
    Wait-And-Exit 2
}

$source = Join-Path $PSScriptRoot 'ETS2RpmUdpBridge.cs'
if (-not (Test-Path $source)) {
    Write-Host "ERROR: Source file missing: $source" -ForegroundColor Red
    Wait-And-Exit 1
}

$compilerCandidates = @(
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'),
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe')
)
$compiler = $compilerCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $compiler) {
    Write-Host "ERROR: .NET Framework C# compiler (csc.exe) was not found." -ForegroundColor Red
    Write-Host "Install Visual Studio Build Tools with .NET desktop build tools, or the .NET Framework developer tools, then retry."
    Wait-And-Exit 1
}

function Find-FrameworkReference([string]$Name) {
    $pf86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $pf64 = [Environment]::GetFolderPath('ProgramFiles')
    $dirs = @(
        $SimHubPath,
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'),
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\WPF'),
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319'),
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\WPF'),
        (Join-Path $pf86 'Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8'),
        (Join-Path $pf86 'Reference Assemblies\Microsoft\Framework\.NETFramework\v4.7.2'),
        (Join-Path $pf64 'Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8'),
        (Join-Path $pf64 'Reference Assemblies\Microsoft\Framework\.NETFramework\v4.7.2')
    )
    foreach ($dir in $dirs) {
        $candidate = Join-Path $dir $Name
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

$referenceNames = @(
    'SimHub.Plugins.dll',
    'GameReaderCommon.dll',
    'SimHub.Logging.dll',
    'System.dll',
    'System.Core.dll',
    'WindowsBase.dll',
    'System.Xaml.dll',
    'PresentationCore.dll',
    'PresentationFramework.dll'
)
$references = @()
foreach ($name in $referenceNames) {
    $ref = Find-FrameworkReference $name
    if (-not $ref) {
        Write-Host "ERROR: Required assembly not found: $name" -ForegroundColor Red
        Write-Host "SimHub path: $SimHubPath"
        Wait-And-Exit 1
    }
    $references += $ref
}

$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$outputDll = Join-Path $buildDir 'ETS2RpmUdpBridge.dll'
$cscArgs = @('/nologo', '/target:library', '/optimize+', '/warn:4', "/out:$outputDll")
foreach ($ref in $references) { $cscArgs += "/reference:$ref" }
$cscArgs += $source

Write-Host "Compiling ETS2 RPM UDP Bridge..."
& $compiler @cscArgs
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $outputDll)) {
    Write-Host "BUILD FAILED. The compiler output above contains the error." -ForegroundColor Red
    Wait-And-Exit 1
}

$destination = Join-Path $SimHubPath 'ETS2RpmUdpBridge.dll'
if (Test-Path $destination) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    Copy-Item $destination ("$destination.backup-$stamp") -Force
}
Copy-Item $outputDll $destination -Force
Write-Host ""
Write-Host "SUCCESS: Plugin compiled and copied to:" -ForegroundColor Green
Write-Host $destination
Write-Host "Next: start SimHub, open Additional Plugins, and enable 'ETS2 RPM UDP Bridge' if prompted."
Write-Host "Then start ble_udp_rpm_bridge.py before connecting the ESP32 BLE panel."
Wait-And-Exit 0
