#requires -Version 7.0
param([switch]$NativeOnly)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $root '.build'
if ((Test-Path -LiteralPath $buildRoot) -and ((Get-Item -LiteralPath $buildRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'The .build directory must not be a link.'
}
$lock = Get-Content -Raw -LiteralPath (Join-Path $root 'dependencies.lock.json') | ConvertFrom-Json
if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'X64' -or (-not $IsWindows -and -not $IsLinux)) {
    throw 'Only Windows x64 and Linux x64 with PowerShell 7 are supported.'
}
foreach ($directory in @('downloads', 'user/libraries', 'data', 'cache', 'logs')) {
    [IO.Directory]::CreateDirectory((Join-Path $buildRoot $directory)) | Out-Null
}
function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash -ne $Expected) {
        throw "SHA-256 checksum mismatch or missing file: $Path"
    }
}
function Get-VerifiedArchive([string]$Url, [string]$Expected, [string]$Name) {
    $destination = Join-Path $buildRoot "downloads/$Name"
    if (-not (Test-Path -LiteralPath $destination)) {
        $partial = "$destination.partial"
        Write-Host "Verified download: $Name"
        Invoke-WebRequest -Uri $Url -OutFile $partial
        Assert-Hash $partial $Expected
        Move-Item -LiteralPath $partial -Destination $destination -Force
    }
    Assert-Hash $destination $Expected
    return $destination
}
function Reset-BuildDirectory([string]$Path) {
    # Recursive deletion is allowed only within this checkout's .build directory.
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($buildRoot) + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe extraction path.' }
    if (Test-Path -LiteralPath $full) {
        if ((Get-Item -LiteralPath $full -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Extraction directory is a link: operation aborted.' }
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($full) | Out-Null
}
function Expand-VerifiedZip([string]$Archive, [string]$Destination) {
    $archiveHandle = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($entry in $archiveHandle.Entries) {
            $normalized = $entry.FullName.Replace('\', '/')
            if ($normalized.StartsWith('/') -or $normalized -match '(^|/)\.\.(/|$)|:' -or (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) {
                throw 'ZIP archive contains a disallowed path or link.'
            }
        }
    } finally { $archiveHandle.Dispose() }
    Reset-BuildDirectory $Destination
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Destination)
}
$selectedLibraries = @($lock.libraries | Where-Object { -not $NativeOnly -or $_.name -eq 'ArduinoJson' })
foreach ($library in $selectedLibraries) {
    $archive = Get-VerifiedArchive $library.url $library.sha256 "$($library.name)-$($library.version).zip"
    $unpack = Join-Path $buildRoot "unpack/$($library.name)"
    Expand-VerifiedZip $archive $unpack
    $metadata = @(Get-ChildItem -LiteralPath $unpack -Filter 'library.properties' -File -Recurse)
    if ($metadata.Count -ne 1) { throw "Unexpected structure in the $($library.name) archive." }
    $declared = if ($library.declaredVersion) { $library.declaredVersion } else { $library.version }
    $properties = Get-Content -LiteralPath $metadata[0].FullName
    if ($properties -notcontains "name=$($library.name)" -or $properties -notcontains "version=$declared") {
        throw "Unexpected metadata for $($library.name)."
    }
    $destination = Join-Path $buildRoot "user/libraries/$($library.name)"
    Reset-BuildDirectory $destination
    Get-ChildItem -LiteralPath $metadata[0].DirectoryName -Force | Copy-Item -Destination $destination -Recurse -Force
}
if ($NativeOnly) { Write-Host 'Native test dependency is ready.'; return }
foreach ($file in $lock.settimino.files.PSObject.Properties) {
    Assert-Hash (Join-Path (Join-Path $root $lock.settimino.path) $file.Name) $file.Value
}
$hostKey = if ($IsWindows) { 'windows-x64' } else { 'linux-x64' }
$release = $lock.arduinoCli.releases.$hostKey
$cliArchive = Get-VerifiedArchive $release.url $release.sha256 $release.archive
$cliDirectory = Join-Path $buildRoot 'cli'
if ($IsWindows) {
    Expand-VerifiedZip $cliArchive $cliDirectory
} else {
    $entries = & tar -tzf $cliArchive
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the CLI TAR archive.' }
    foreach ($entry in $entries) {
        if ($entry.StartsWith('/') -or $entry -match '(^|/)\.\.(/|$)|:') { throw 'Disallowed TAR path.' }
    }
    $details = & tar -tvzf $cliArchive
    if ($LASTEXITCODE -ne 0 -or @($details | Where-Object { $_ -match '^[lh]' }).Count) { throw 'Unexpected TAR archive or archive containing links.' }
    Reset-BuildDirectory $cliDirectory
    & tar -xzf $cliArchive -C $cliDirectory
    if ($LASTEXITCODE -ne 0) { throw 'CLI extraction failed.' }
}
$cli = Join-Path $cliDirectory $(if ($IsWindows) { 'arduino-cli.exe' } else { 'arduino-cli' })
if (-not $IsWindows) {
    & chmod u+x $cli
    if ($LASTEXITCODE -ne 0) { throw 'Cannot make the verified CLI executable.' }
}
$versionOutput = & $cli version
if ($LASTEXITCODE -ne 0 -or ($versionOutput -join ' ') -notmatch ('Version:\s*' + [regex]::Escape($lock.arduinoCli.version) + '\b')) {
    throw 'CLI version does not match the lock file.'
}
# Quoted JSON values are also valid in the CLI YAML file.
$data = (Join-Path $buildRoot 'data') | ConvertTo-Json -Compress
$downloads = (Join-Path $buildRoot 'downloads') | ConvertTo-Json -Compress
$userDirectory = (Join-Path $buildRoot 'user') | ConvertTo-Json -Compress
$cacheDirectory = (Join-Path $buildRoot 'cache') | ConvertTo-Json -Compress
$config = Join-Path $buildRoot 'arduino-cli.yaml'
@("directories:", "  data: $data", "  downloads: $downloads", "  user: $userDirectory", 'build_cache:', "  path: $cacheDirectory", '  extra_paths: []', 'board_manager:', '  enable_unsafe_install: false', 'library:', '  enable_unsafe_install: false') | Set-Content -LiteralPath $config -Encoding utf8
$savedEnvironment = @{}
try {
    foreach ($name in @('DATA', 'DOWNLOADS', 'USER')) {
        $variable = "ARDUINO_DIRECTORIES_$name"
        $savedEnvironment[$variable] = [Environment]::GetEnvironmentVariable($variable, 'Process')
        [Environment]::SetEnvironmentVariable($variable, (Join-Path $buildRoot $name.ToLowerInvariant()), 'Process')
    }
    foreach ($entry in @{ ARDUINO_BUILD_CACHE_PATH = (Join-Path $buildRoot 'cache'); ARDUINO_BOARD_MANAGER_ENABLE_UNSAFE_INSTALL = 'false' }.GetEnumerator()) {
        $savedEnvironment[$entry.Key] = [Environment]::GetEnvironmentVariable($entry.Key, 'Process')
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'Process')
    }
    & $cli --config-file $config core update-index --additional-urls $lock.platform.indexUrl
    if ($LASTEXITCODE -ne 0) { throw "Index update failed: $LASTEXITCODE" }
    # The core installed by the CLI must match the archive pinned in the lock file.
    $index = Get-Content -Raw -LiteralPath (Join-Path $buildRoot 'data/package_esp32_index.json') | ConvertFrom-Json
    $platform = @($index.packages.platforms | Where-Object { $_.architecture -eq 'esp32' -and $_.version -eq $lock.platform.version })
    if ($platform.Count -ne 1 -or $platform[0].url -ne $lock.platform.url -or $platform[0].checksum -ne "SHA-256:$($lock.platform.sha256)") {
        throw 'The published core does not match the lock file.'
    }
    & $cli --config-file $config core install "$($lock.platform.id)@$($lock.platform.version)" --additional-urls $lock.platform.indexUrl
    if ($LASTEXITCODE -ne 0) { throw "Core installation failed: $LASTEXITCODE" }
} finally {
    foreach ($variable in $savedEnvironment.Keys) {
        # A variable that was initially absent must be removed, not left empty.
        if ($null -eq $savedEnvironment[$variable]) {
            Remove-Item -LiteralPath "Env:$variable" -ErrorAction SilentlyContinue
        } else {
            [Environment]::SetEnvironmentVariable($variable, $savedEnvironment[$variable], 'Process')
        }
    }
}
Write-Host 'Isolated ESP32 environment is ready; no upload to the board performed.'
