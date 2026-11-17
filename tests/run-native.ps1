param(
    [string]$Compiler = '',
    [string]$ArduinoJsonInclude = '',
    [string]$BuildDirectory = ''
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Compiler) {
    $command = Get-Command 'g++' -ErrorAction SilentlyContinue
    if ($command) { $Compiler = $command.Source }
    elseif (Test-Path -LiteralPath 'C:/msys64/ucrt64/bin/g++.exe') { $Compiler = 'C:/msys64/ucrt64/bin/g++.exe' }
    else { throw 'Specify -Compiler with the path to a C++17 compiler.' }
}
$Compiler = (Get-Item -LiteralPath $Compiler -Force).FullName
if (-not $ArduinoJsonInclude) {
    $documents = [Environment]::GetFolderPath('MyDocuments')
    $ArduinoJsonInclude = Join-Path $documents 'Arduino/libraries/ArduinoJson/src'
}
if (-not (Test-Path -LiteralPath (Join-Path $ArduinoJsonInclude 'ArduinoJson.h'))) {
    throw 'Specify -ArduinoJsonInclude with the ArduinoJson 7 src directory.'
}
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $root '_analysis/native-tests' }
# .NET and native tools may resolve the same relative path against
# different current directories after PowerShell Set-Location.
$BuildDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDirectory)
[IO.Directory]::CreateDirectory($BuildDirectory) | Out-Null
$executable = Join-Path $BuildDirectory 'gateway-tests.exe'
$savedPath = $env:PATH
try {
    $env:PATH = [IO.Path]::GetDirectoryName($Compiler) + [IO.Path]::PathSeparator + $savedPath
    $sources = @(
        (Join-Path $root 'tests/gateway_tests.cpp'),
        (Join-Path $root 'firmware/PLC_AWS_Gateway/GatewayCore.cpp'),
        (Join-Path $root 'firmware/PLC_AWS_Gateway/GatewayProtocol.cpp')
    )
    & $Compiler -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I $ArduinoJsonInclude @sources -o $executable
    if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed with exit $LASTEXITCODE" }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw "Native tests failed with exit $LASTEXITCODE" }
} finally {
    $env:PATH = $savedPath
}
