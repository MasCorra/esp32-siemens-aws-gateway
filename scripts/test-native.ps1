#requires -Version 7.0
param([string]$Compiler = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$include = Join-Path $root '.build/user/libraries/ArduinoJson/src'
if (-not (Test-Path -LiteralPath (Join-Path $include 'ArduinoJson.h'))) {
    throw 'Run scripts/prepare-environment.ps1 -NativeOnly first.'
}
if (-not $Compiler) {
    $command = Get-Command 'g++' -ErrorAction SilentlyContinue
    if ($command) { $Compiler = $command.Source }
    elseif ($IsWindows) {
        # Compilers are local prerequisites; the script does not install them.
        foreach ($candidate in @('C:/msys64/ucrt64/bin/g++.exe', 'C:/mingw64/bin/g++.exe', 'C:/Program Files/mingw64/bin/g++.exe')) {
            if (Test-Path -LiteralPath $candidate) { $Compiler = $candidate; break }
        }
    }
}
if (-not $Compiler) { throw 'A C++17 compiler is required: install g++ or pass -Compiler.' }
Write-Host "Native compiler: $Compiler"
& (Join-Path $root 'tests/run-native.ps1') -Compiler $Compiler -ArduinoJsonInclude $include -BuildDirectory (Join-Path $root '.build/native-tests')
