[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$QtDir = "C:\Qt\6.11.1\msvc2022_64",

    [ValidateNotNullOrEmpty()]
    [string]$CMakeBinDir = "C:\Qt\Tools\CMake_64\bin",

    [ValidateNotNullOrEmpty()]
    [string]$PackageName = "FootballOwner_MVP_0.10.0",

    [switch]$SkipBuild
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

function Assert-AnyRequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Description,

        [Parameter(Mandatory = $true)]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return
        }
    }

    throw "$Description deployment file was not found. Checked paths: $($Candidates -join ', ')"
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
$windeployqtExe = Resolve-RequiredFile `
    -Path (Join-Path -Path $qtBinDir -ChildPath "windeployqt.exe") `
    -Description "windeployqt"

$existingPathEntries = @()
if (-not [string]::IsNullOrWhiteSpace($env:Path)) {
    $existingPathEntries = $env:Path -split ";" |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
}

$env:QTDIR = $QtDir
$env:Path = (@($CMakeBinDir, $qtBinDir) + $existingPathEntries |
        Select-Object -Unique) -join ";"

if (-not $SkipBuild) {
    $preset = "windows-msvc-release"
    Push-Location -LiteralPath $projectRoot
    try {
        Invoke-CheckedNativeCommand `
            -Step "Release CMake configure" `
            -FilePath $cmakeExe `
            -Arguments @("--preset", $preset, "--fresh")
        Invoke-CheckedNativeCommand `
            -Step "Release build" `
            -FilePath $cmakeExe `
            -Arguments @("--build", "--preset", $preset)
        Invoke-CheckedNativeCommand `
            -Step "Release tests" `
            -FilePath $ctestExe `
            -Arguments @("--preset", $preset)
    }
    finally {
        Pop-Location
    }
}

$sourceExe = Resolve-RequiredFile `
    -Path (Join-Path -Path $projectRoot `
        -ChildPath "build\windows-msvc-release\Release\FootballOwner.exe") `
    -Description "FootballOwner Release executable"
$qmlDir = Resolve-RequiredDirectory `
    -Path (Join-Path -Path $projectRoot -ChildPath "qml") `
    -Description "QML source"

$distRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $projectRoot -ChildPath "dist")
)
$packageDir = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $distRoot -ChildPath $PackageName)
)
$requiredPrefix = $distRoot.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar
) + [System.IO.Path]::DirectorySeparatorChar

if (-not $packageDir.StartsWith(
        $requiredPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
    throw "Package directory must stay inside the project dist directory: $packageDir"
}

if (-not (Test-Path -LiteralPath $distRoot -PathType Container)) {
    $null = New-Item -ItemType Directory -Path $distRoot
}
if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
$null = New-Item -ItemType Directory -Path $packageDir

$packageExe = Join-Path -Path $packageDir -ChildPath "FootballOwner.exe"
Copy-Item -LiteralPath $sourceExe -Destination $packageExe

Invoke-CheckedNativeCommand `
    -Step "Qt runtime deployment" `
    -FilePath $windeployqtExe `
    -Arguments @(
        "--release",
        "--compiler-runtime",
        "--qmldir", $qmlDir,
        "--dir", $packageDir,
        $packageExe
    )

Copy-Item `
    -LiteralPath (Join-Path -Path $projectRoot -ChildPath "README.md") `
    -Destination (Join-Path -Path $packageDir -ChildPath "README.md")

$runGuide = @"
Football Owner MVP 0.10.0

Double-click FootballOwner.exe to run the game.
Qt and CMake environment variables are not required for this packaged build.

Save location:
FootballOwner\saves under the Windows application data path.

Legacy 0.2.0 save:
The original football_owner_mvp.db is not deleted. On first launch, it is copied
into the named save list.
"@
$runGuide | Set-Content `
    -LiteralPath (Join-Path -Path $packageDir -ChildPath "RUN.txt") `
    -Encoding UTF8

Assert-AnyRequiredFile `
    -Description "Qt Core" `
    -Candidates @(
        (Join-Path -Path $packageDir -ChildPath "Qt6Cored.dll"),
        (Join-Path -Path $packageDir -ChildPath "Qt6Core.dll")
    )
Assert-AnyRequiredFile `
    -Description "Windows platform plugin" `
    -Candidates @(
        (Join-Path -Path $packageDir -ChildPath "platforms\qwindowsd.dll"),
        (Join-Path -Path $packageDir -ChildPath "platforms\qwindows.dll")
    )
Assert-AnyRequiredFile `
    -Description "SQLite driver" `
    -Candidates @(
        (Join-Path -Path $packageDir -ChildPath "sqldrivers\qsqlited.dll"),
        (Join-Path -Path $packageDir -ChildPath "sqldrivers\qsqlite.dll")
    )
$null = Resolve-RequiredDirectory `
    -Path (Join-Path -Path $packageDir -ChildPath "qml") `
    -Description "Packaged QML plugins"
$packageExe = Resolve-RequiredFile `
    -Path $packageExe `
    -Description "Packaged FootballOwner executable"

Write-Host ""
Write-Host "Standalone package created." -ForegroundColor Green
Write-Host "Package directory: $packageDir" -ForegroundColor Green
Write-Host "Double-click executable: $packageExe" -ForegroundColor Green
