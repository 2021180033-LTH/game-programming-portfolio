[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$QtDir = "C:\Qt\6.11.1\msvc2022_64",

    [ValidateNotNullOrEmpty()]
    [string]$CMakeBinDir = "C:\Qt\Tools\CMake_64\bin"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RequiredDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Description directory was not found: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Resolve-RequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description file was not found: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Invoke-CheckedNativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Step,

        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    Write-Host ""
    Write-Host "[$Step] $FilePath $($Arguments -join ' ')" -ForegroundColor Cyan
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        throw "$Step step failed. Exit code: $exitCode"
    }
}

$projectRoot = Resolve-RequiredDirectory `
    -Path (Join-Path -Path $PSScriptRoot -ChildPath "..") `
    -Description "Project root"
$QtDir = Resolve-RequiredDirectory -Path $QtDir -Description "Qt"
$CMakeBinDir = Resolve-RequiredDirectory -Path $CMakeBinDir -Description "CMake"

$qtBinDir = Resolve-RequiredDirectory `
    -Path (Join-Path -Path $QtDir -ChildPath "bin") `
    -Description "Qt bin"
$null = Resolve-RequiredFile `
    -Path (Join-Path -Path $QtDir -ChildPath "lib\cmake\Qt6\Qt6Config.cmake") `
    -Description "Qt 6 CMake config"
$cmakeExe = Resolve-RequiredFile `
    -Path (Join-Path -Path $CMakeBinDir -ChildPath "cmake.exe") `
    -Description "CMake"
$ctestExe = Resolve-RequiredFile `
    -Path (Join-Path -Path $CMakeBinDir -ChildPath "ctest.exe") `
    -Description "CTest"

$existingPathEntries = @()
if (-not [string]::IsNullOrWhiteSpace($env:Path)) {
    $existingPathEntries = $env:Path -split ";" |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
}

$env:QTDIR = $QtDir
$env:Path = (@($CMakeBinDir, $qtBinDir) + $existingPathEntries |
        Select-Object -Unique) -join ";"

$preset = "windows-msvc-debug"
$exePath = Join-Path -Path $projectRoot `
    -ChildPath "build\windows-msvc-debug\Debug\FootballOwner.exe"

Write-Host "Football Owner Windows development environment" -ForegroundColor Yellow
Write-Host "Project: $projectRoot"
Write-Host "Qt:        $QtDir"
Write-Host "CMake:     $CMakeBinDir"

Push-Location -LiteralPath $projectRoot
try {
    Invoke-CheckedNativeCommand `
        -Step "CMake configure" `
        -FilePath $cmakeExe `
        -Arguments @("--preset", $preset, "--fresh")
    Invoke-CheckedNativeCommand `
        -Step "Build" `
        -FilePath $cmakeExe `
        -Arguments @("--build", "--preset", $preset)
    Invoke-CheckedNativeCommand `
        -Step "Tests" `
        -FilePath $ctestExe `
        -Arguments @("--preset", $preset)
}
finally {
    Pop-Location
}

$exePath = Resolve-RequiredFile -Path $exePath -Description "FootballOwner executable"

Write-Host ""
Write-Host "Build and tests completed." -ForegroundColor Green
Write-Host "Executable: $exePath" -ForegroundColor Green
