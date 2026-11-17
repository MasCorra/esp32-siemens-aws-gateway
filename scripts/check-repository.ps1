#requires -Version 7.0
param([string]$Root = ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))))
$ErrorActionPreference = 'Stop'
$Root = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Root)
$failures = [Collections.Generic.List[string]]::new()

function Add-Failure([string]$Code, [string]$Message) {
    $failures.Add("[$Code] $Message")
}

function Test-Hashes([string]$Directory, $Entries, [string]$Code) {
    $base = [IO.Path]::GetFullPath((Join-Path $Root $Directory))
    foreach ($entry in $Entries.PSObject.Properties) {
        $path = [IO.Path]::GetFullPath((Join-Path $base $entry.Name))
        if (-not $path.StartsWith($base + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            Add-Failure $Code "Invalid manifest path: $($entry.Name)"
        } elseif (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Add-Failure $Code "Missing file: $Directory/$($entry.Name)"
        } elseif ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            Add-Failure $Code "Unexpected link: $Directory/$($entry.Name)"
        } elseif ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Value) {
            Add-Failure $Code "File differs from the preserved copy: $Directory/$($entry.Name)"
        }
    }
}

$gitRoot = & git -C $Root rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($gitRoot) -ne [IO.Path]::GetFullPath($Root)) {
    throw '[GIT_ROOT] Run the checks at a Git repository root.'
}
$files = @(& git -C $Root -c core.quotePath=false ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw '[GIT_FILES] Cannot read candidate files.' }

foreach ($required in @('LICENSE','THIRD_PARTY_NOTICES.md','README.md','dependencies.lock.json',
    '.github/workflows/ci.yml','firmware/PLC_AWS_Gateway/PLC_AWS_Gateway.ino',
    'firmware/PLC_AWS_Gateway/secrets.h.example','archive/README.md','archive/sha256.json',
    'third_party/Settimino/licenses/GPL-3.0.txt','third_party/Settimino/licenses/LGPL-3.0.txt')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Root $required) -PathType Leaf)) {
        Add-Failure 'REQUIRED_FILE' "Required file missing: $required"
    }
}

$activeSketches = @(Get-ChildItem -LiteralPath (Join-Path $Root 'firmware') -Filter '*.ino' -File -Recurse)
if ($activeSketches.Count -ne 1 -or $activeSketches[0].BaseName -ne 'PLC_AWS_Gateway') {
    Add-Failure 'LAYOUT' 'firmware must contain only the main gateway.'
}
if (Test-Path -LiteralPath (Join-Path $Root 'experiments')) {
    if (@(Get-ChildItem -LiteralPath (Join-Path $Root 'experiments') -File -Recurse).Count) {
        Add-Failure 'LAYOUT' 'Historical experiments must be in archive/sketches.'
    }
}

$lockPath = Join-Path $Root 'dependencies.lock.json'
if (Test-Path -LiteralPath $lockPath) {
    $lock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
    if ($lock.schemaVersion -ne 1) { Add-Failure 'LOCK' 'Unsupported lock file schema.' }
    Test-Hashes $lock.settimino.path $lock.settimino.files 'SETTIMINO_HASH'
}
$archiveManifest = Join-Path $Root 'archive/sha256.json'
if (Test-Path -LiteralPath $archiveManifest) {
    $archive = Get-Content -LiteralPath $archiveManifest -Raw | ConvertFrom-Json
    Test-Hashes 'archive' $archive.Files 'ARCHIVE_HASH'
}

foreach ($relative in @('firmware/PLC_AWS_Gateway/secrets.h','.env','.build/fixture.tmp')) {
    & git -C $Root check-ignore --no-index -q -- $relative
    if ($LASTEXITCODE -ne 0) { Add-Failure 'IGNORE' "Missing protection: $relative" }
}
& git -C $Root check-ignore --no-index -q -- 'firmware/PLC_AWS_Gateway/secrets.h.example'
if ($LASTEXITCODE -eq 0) { Add-Failure 'IGNORE' 'The secrets.h.example template must be publishable.' }

# Only Git candidates are checked: an ignored local secrets.h remains private.
foreach ($relative in $files) {
    if ($relative -match '(^|/)(secrets\.h|\.env(?:\..*)?)$|\.(pem|key|p12|pfx|crt|iso|img|zip|rar|exe|apk)$' -and
        $relative -notmatch '(^|/)\.env\.example$') {
        Add-Failure 'FORBIDDEN_FILE' "File must not be published: $relative"
        continue
    }
    if ($relative -match '^(_analysis|\.build)/') {
        Add-Failure 'FORBIDDEN_FILE' "Local artifact included in publication candidates: $relative"
        continue
    }
    $path = Join-Path $Root $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $file = Get-Item -LiteralPath $path -Force
    if ($file.Length -gt 5MB) { Add-Failure 'LARGE_FILE' "Unexpected file over 5 MiB: $relative" }
    if ($file.Extension -in @('.ino','.cpp','.h','.ps1','.yml','.yaml','.json','.md','.example','.txt','.properties') -or $file.Name -eq 'LICENSE') {
        $content = [IO.File]::ReadAllText($path)
        if ($content -match '-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|[a-z0-9-]+-ats\.iot\.[a-z0-9-]+\.amazonaws\.com') {
            Add-Failure 'PRIVATE_KEY' "Sensitive marker in file: $relative"
        }
    }
}

if ($failures.Count) { throw ($failures -join "`n") }
Write-Output "Repository checks passed: $($files.Count) candidate files; Settimino sources and archive are intact."
# The expected exit code 1 from git check-ignore must not become the check result.
exit 0
