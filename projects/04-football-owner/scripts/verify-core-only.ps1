[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$CMake = "cmake",

    [ValidateNotNullOrEmpty()]
    [string]$CTest = "ctest",

    [ValidateNotNullOrEmpty()]
    [string]$Ninja = "ninja"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path -LiteralPath (
        Join-Path -Path $PSScriptRoot -ChildPath ".."
    )).Path

function Import-MsvcEnvironment {
    if ($env:OS -ne "Windows_NT" -or (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        return
    }

    $vswhere = Join-Path `
        -Path ${env:ProgramFiles(x86)} `
        -ChildPath "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "MSVC compiler was not found. Run this script from a Visual Studio Developer PowerShell."
    }

    $installationPath = (& $vswhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if ([string]::IsNullOrWhiteSpace($installationPath)) {
        throw "Visual Studio C++ build tools were not found."
    }

    $devCmd = Join-Path -Path $installationPath -ChildPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path -LiteralPath $devCmd -PathType Leaf)) {
        throw "VsDevCmd.bat was not found: $devCmd"
    }

    $environmentLines = & $env:ComSpec /d /s /c `
        "call `"$devCmd`" -arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio developer environment initialization failed."
    }

    $developerPath = $null
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        if ($name -ieq "Path") {
            if ($value -match "VC\\Tools\\MSVC.+HostX64") {
                $developerPath = $value
            }
            continue
        }
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }

    if ([string]::IsNullOrWhiteSpace($developerPath)) {
        throw "Visual Studio developer PATH was not produced by VsDevCmd.bat."
    }
    $env:Path = $developerPath
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

Push-Location -LiteralPath $projectRoot
try {
    Import-MsvcEnvironment
    $ninjaCommand = Get-Command $Ninja -ErrorAction SilentlyContinue
    if ($null -eq $ninjaCommand) {
        throw "Ninja was not found. Install it or pass -Ninja with the executable path."
    }
    $ninjaPath = $ninjaCommand.Source

    Invoke-Checked -Command $CMake -Arguments @(
        "--preset",
        "core-only-debug",
        "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
    )
    Invoke-Checked -Command $CMake -Arguments @("--build", "--preset", "core-only-debug")
    Invoke-Checked -Command $CTest -Arguments @("--preset", "core-only-debug")
}
finally {
    Pop-Location
}
