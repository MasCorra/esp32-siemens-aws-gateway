#requires -Version 7.0
param([switch]$ValidateConfiguration)
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
$cli = Join-Path $buildRoot $(if ($IsWindows) { 'cli/arduino-cli.exe' } else { 'cli/arduino-cli' })
$config = Join-Path $buildRoot 'arduino-cli.yaml'
if (-not (Test-Path -LiteralPath $cli) -or -not (Test-Path -LiteralPath $config)) {
    throw 'Run scripts/prepare-environment.ps1 first.'
}
$version = & $cli version
if ($LASTEXITCODE -ne 0 -or ($version -join ' ') -notmatch ('Version:\s*' + [regex]::Escape($lock.arduinoCli.version) + '\b')) {
    throw 'CLI version does not match the lock file.'
}
$settimino = Join-Path $root $lock.settimino.path
foreach ($file in $lock.settimino.files.PSObject.Properties) {
    $path = Join-Path $settimino $file.Name
    if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash -ne $file.Value) {
        throw "Settimino does not match the lock file: $($file.Name)."
    }
}
$mode = if ($ValidateConfiguration) { 'validation' } else { 'default' }
$workspace = Join-Path $buildRoot "firmware/$mode"
$sketch = Join-Path $workspace 'PLC_AWS_Gateway'
$resolved = [IO.Path]::GetFullPath($workspace)
$prefix = [IO.Path]::GetFullPath($buildRoot) + [IO.Path]::DirectorySeparatorChar
if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe build path.' }
if (Test-Path -LiteralPath $workspace) {
    if ((Get-Item -LiteralPath $workspace -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Build directory is a link: operation aborted.' }
    Remove-Item -LiteralPath $workspace -Recurse -Force
}
[IO.Directory]::CreateDirectory($sketch) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $buildRoot 'logs')) | Out-Null
# The copy allows builds without reading any local credentials.
$files = @('PLC_AWS_Gateway.ino', 'GatewayConfig.h', 'GatewayCore.h', 'GatewayCore.cpp', 'GatewayProtocol.h', 'GatewayProtocol.cpp', 'secrets.h.example')
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $root "firmware/PLC_AWS_Gateway/$file") -Destination (Join-Path $sketch $file)
}
$arguments = @('--config-file', $config, 'compile', '--fqbn', $lock.platform.fqbn, '--warnings', 'all', '--library', $settimino, '--libraries', (Join-Path $buildRoot 'user/libraries'), '--build-path', (Join-Path $workspace 'output'))
if ($ValidateConfiguration) {
    # Dummy values for building the configured branch; these are not PLC parameters.
    $flags = '-MMD -c -DGATEWAY_PLC_DB_NUMBER=2 -DGATEWAY_PLC_DATE_ENCODING=1 -DGATEWAY_PLC_DATE_FORMAT=1 -DGATEWAY_PLC_UTC_OFFSET_MINUTES=0'
    $arguments += @('--build-property', "compiler.cpp.extra_flags=$flags")
}
$arguments += $sketch
$savedEnvironment = @{}
try {
    foreach ($name in @('DATA', 'DOWNLOADS', 'USER')) {
        $variable = "ARDUINO_DIRECTORIES_$name"
        $savedEnvironment[$variable] = [Environment]::GetEnvironmentVariable($variable, 'Process')
        [Environment]::SetEnvironmentVariable($variable, (Join-Path $buildRoot $name.ToLowerInvariant()), 'Process')
    }
    $savedEnvironment['ARDUINO_BUILD_CACHE_PATH'] = [Environment]::GetEnvironmentVariable('ARDUINO_BUILD_CACHE_PATH', 'Process')
    [Environment]::SetEnvironmentVariable('ARDUINO_BUILD_CACHE_PATH', (Join-Path $buildRoot 'cache'), 'Process')
    & $cli @arguments 2>&1 | Tee-Object -FilePath (Join-Path $buildRoot "logs/firmware-$mode.log")
    if ($LASTEXITCODE -ne 0) { throw "ESP32 build failed: $LASTEXITCODE" }
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
Write-Host "ESP32 build ($mode) completed using placeholders only; no upload performed."
