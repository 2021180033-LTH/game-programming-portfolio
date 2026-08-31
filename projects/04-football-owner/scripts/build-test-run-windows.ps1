[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$QtDir = "C:\Qt\6.11.1\msvc2022_64",

    [ValidateNotNullOrEmpty()]
    [string]$CMakeBinDir = "C:\Qt\Tools\CMake_64\bin",

    [ValidateRange(1, 60)]
    [int]$LaunchSmokeSeconds = 5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path -LiteralPath (
        Join-Path -Path $PSScriptRoot -ChildPath ".."
    )).Path
$buildScript = Join-Path -Path $PSScriptRoot -ChildPath "build-windows.ps1"

if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
    throw "Build script was not found: $buildScript"
}

& $buildScript -QtDir $QtDir -CMakeBinDir $CMakeBinDir

$exePath = Join-Path -Path $projectRoot `
    -ChildPath "build\windows-msvc-debug\Debug\FootballOwner.exe"
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "FootballOwner.exe was not found after build: $exePath"
}
$exePath = (Resolve-Path -LiteralPath $exePath).Path

Write-Host ""
Write-Host "Launching Football Owner." -ForegroundColor Yellow
Write-Host "Executable: $exePath" -ForegroundColor Green

$process = Start-Process `
    -FilePath $exePath `
    -WorkingDirectory (Split-Path -Path $exePath -Parent) `
    -PassThru

$deadline = (Get-Date).AddSeconds($LaunchSmokeSeconds)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($process.HasExited) {
        if ($process.ExitCode -ne 0) {
            throw "Football Owner exited with an error during launch smoke test. Exit code: $($process.ExitCode)"
        }
        Write-Host "Football Owner launched and exited normally." -ForegroundColor Green
        exit 0
    }
}

$process.Refresh()
if ($process.HasExited) {
    if ($process.ExitCode -ne 0) {
        throw "Football Owner exited with an error during launch smoke test. Exit code: $($process.ExitCode)"
    }
    Write-Host "Football Owner launched and exited normally." -ForegroundColor Green
    exit 0
}

Write-Host "Football Owner stayed running for $LaunchSmokeSeconds seconds. Launch smoke test passed." -ForegroundColor Green

if ($process.MainWindowHandle -ne 0) {
    [void]$process.CloseMainWindow()
    if (-not $process.WaitForExit(5000)) {
        Stop-Process -Id $process.Id -Force
        Write-Host "Football Owner test process did not close from the window signal and was stopped." -ForegroundColor Yellow
    }
} else {
    Stop-Process -Id $process.Id -Force
    Write-Host "Football Owner test process had no main window handle and was stopped after smoke pass." -ForegroundColor Yellow
}
