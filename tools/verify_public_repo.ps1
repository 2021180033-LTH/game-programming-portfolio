param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [string]$ReportPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'docs/VERIFICATION_REPORT.md')
)

$ErrorActionPreference = 'Stop'
$rootPath = [System.IO.Path]::GetFullPath($Root)
if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) {
    throw "Repository root does not exist: $rootPath"
}

$files = Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force |
    Where-Object { $_.FullName -notmatch '[\\/]\.git[\\/]' }

$forbiddenExtensions = @(
    '.exe', '.dll', '.pdb', '.ilk', '.obj', '.o', '.lib', '.exp', '.pak',
    '.zip', '.7z', '.rar', '.db', '.sqlite', '.sqlite3', '.bak', '.dmp'
)
$forbiddenDirectoryNames = @(
    '.git', '.vs', 'Binaries', 'Intermediate', 'DerivedDataCache', 'Saved',
    'Builds', 'Debug', 'Release', 'packages', 'node_modules', '__pycache__'
)
$textExtensions = @(
    '.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.inl', '.cs',
    '.py', '.ps1', '.lua', '.sql', '.qml', '.json', '.ini', '.cfg', '.conf',
    '.xml', '.yml', '.yaml', '.toml', '.md', '.txt', '.cmake', '.vcxproj',
    '.filters', '.sln', '.uproject', '.uplugin', '.csv'
)

$largeFiles = @($files | Where-Object { $_.Length -ge 50MB })
$forbiddenFiles = @($files | Where-Object { $forbiddenExtensions -contains $_.Extension.ToLowerInvariant() })
$forbiddenDirectories = @(
    Get-ChildItem -LiteralPath $rootPath -Recurse -Directory -Force |
        Where-Object { $forbiddenDirectoryNames -contains $_.Name }
)

$secretPatterns = @(
    @{ Name = 'private key'; Regex = '-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----' },
    @{ Name = 'assigned credential'; Regex = '(?i)\b(?:password|passwd|pwd|api[_-]?key|secret|securitytoken|access[_-]?token|auth[_-]?token)\b\s*[:=]\s*["''][^"'']{4,}["'']' },
    @{ Name = 'connection credential'; Regex = '(?i)\b(?:UID|User ID|PWD|Password)\s*=\s*[^;\r\n]+' },
    @{ Name = 'absolute user path'; Regex = '(?i)\b[A-Z]:\\Users\\[^\\\r\n]+\\' }
)
$warningPatterns = @(
    @{ Name = 'private IPv4'; Regex = '\b(?:10\.(?:\d{1,3}\.){2}\d{1,3}|192\.168\.(?:\d{1,3}\.)\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.(?:\d{1,3}\.)\d{1,3})\b' },
    @{ Name = 'local DSN'; Regex = '(?i)\bDSN\s*=\s*[^;\r\n]+' },
    @{ Name = 'drive-local path'; Regex = '(?i)\b[XS]:\\[^\r\n]+' }
)

$secretHits = [System.Collections.Generic.List[object]]::new()
$warningHits = [System.Collections.Generic.List[object]]::new()

foreach ($file in $files) {
    $relativeForScan = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName).Replace('\', '/')
    if ($relativeForScan -in @(
        'tools/verify_public_repo.ps1',
        'docs/VERIFICATION_REPORT.md',
        'docs/REVIEWED_WARNINGS.md'
    )) {
        continue
    }
    $extension = $file.Extension.ToLowerInvariant()
    if (($textExtensions -notcontains $extension) -and $file.Name -notin @('CMakeLists.txt', '.gitignore', '.gitattributes')) {
        continue
    }

    try {
        $lines = Get-Content -LiteralPath $file.FullName -ErrorAction Stop
    }
    catch {
        continue
    }

    for ($index = 0; $index -lt $lines.Count; $index++) {
        $line = [string]$lines[$index]
        foreach ($pattern in $secretPatterns) {
            if ($line -match $pattern.Regex) {
                $secretHits.Add([pscustomobject]@{
                    Kind = $pattern.Name
                    Path = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName).Replace('\', '/')
                    Line = $index + 1
                })
            }
        }
        foreach ($pattern in $warningPatterns) {
            if ($line -match $pattern.Regex) {
                $warningHits.Add([pscustomobject]@{
                    Kind = $pattern.Name
                    Path = [System.IO.Path]::GetRelativePath($rootPath, $file.FullName).Replace('\', '/')
                    Line = $index + 1
                })
            }
        }
    }
}

function Format-FileRows($items) {
    if ($items.Count -eq 0) { return @('- 없음') }
    return @($items | ForEach-Object {
        $relative = [System.IO.Path]::GetRelativePath($rootPath, $_.FullName).Replace('\', '/')
        "- ``$relative`` ($([math]::Round($_.Length / 1MB, 2)) MiB)"
    })
}

function Format-HitRows($items) {
    if ($items.Count -eq 0) { return @('- 없음') }
    return @($items | Sort-Object Path, Line, Kind -Unique | ForEach-Object {
        "- $($_.Kind): ``$($_.Path):$($_.Line)``"
    })
}

$report = [System.Collections.Generic.List[string]]::new()
$report.Add('# 공개 저장소 자동 검수 결과')
$report.Add('')
$report.Add("- 검사 시각: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')")
$report.Add("- 검사 파일 수: $($files.Count)")
$report.Add("- 50 MiB 이상 파일: $($largeFiles.Count)")
$report.Add("- 금지 확장자 파일: $($forbiddenFiles.Count)")
$report.Add("- 금지 디렉터리: $($forbiddenDirectories.Count)")
$report.Add("- 비밀정보 후보: $($secretHits.Count)")
$report.Add("- 환경 의존 정보 경고: $($warningHits.Count)")
$report.Add('')
$report.Add('## 50 MiB 이상 파일')
$report.Add('')
$report.AddRange([string[]](Format-FileRows $largeFiles))
$report.Add('')
$report.Add('## 금지 확장자 파일')
$report.Add('')
$report.AddRange([string[]](Format-FileRows $forbiddenFiles))
$report.Add('')
$report.Add('## 금지 디렉터리')
$report.Add('')
if ($forbiddenDirectories.Count -eq 0) {
    $report.Add('- 없음')
}
else {
    foreach ($directory in $forbiddenDirectories) {
        $relative = [System.IO.Path]::GetRelativePath($rootPath, $directory.FullName).Replace('\', '/')
        $report.Add("- ``$relative/``")
    }
}
$report.Add('')
$report.Add('## 비밀정보 후보')
$report.Add('')
$report.AddRange([string[]](Format-HitRows $secretHits))
$report.Add('')
$report.Add('## 환경 의존 정보 경고')
$report.Add('')
$report.AddRange([string[]](Format-HitRows $warningHits))
$report.Add('')
$report.Add('> 자동 검사는 오탐과 누락이 있을 수 있으므로 공개 전 사람이 다시 검토해야 합니다.')

$reportDirectory = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
[System.IO.File]::WriteAllLines($ReportPath, $report, [System.Text.UTF8Encoding]::new($false))

$blockingCount = $largeFiles.Count + $forbiddenFiles.Count + $forbiddenDirectories.Count + $secretHits.Count
Write-Output "Scanned: $($files.Count) files"
Write-Output "Blocking findings: $blockingCount"
Write-Output "Warnings: $($warningHits.Count)"
Write-Output "Report: $ReportPath"

if ($blockingCount -gt 0) {
    exit 1
}
