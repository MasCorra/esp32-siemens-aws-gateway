#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$checker = Join-Path $root 'scripts/check-repository.ps1'
$files = @(& git -C $root -c core.quotePath=false ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Cannot read repository files.' }
$testRoot = Join-Path $root ('.build/repository-selftests/' + [guid]::NewGuid().ToString('N'))
$failures = 0

function New-Case([string]$Name) {
    $path = Join-Path $testRoot $Name
    [IO.Directory]::CreateDirectory($path) | Out-Null
    foreach ($relative in $files) {
        $destination = Join-Path $path $relative
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
    }
    & git -C $path init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the Git fixture.' }
    return $path
}

function Expect-Rejection([string]$Name, [string]$Path, [string]$Code) {
    $caught = ''
    try { & $checker -Root $Path | Out-Null } catch { $caught = $_.Exception.Message }
    if ($caught.Contains($Code)) {
        Write-Output "PASS $Name"
    } else {
        $script:failures++
        Write-Output "FAIL $Name : expected rejection missing for code $Code"
    }
}

$case = New-Case 'modified-settimino'
Add-Content -LiteralPath (Join-Path $case 'third_party/Settimino/Settimino.cpp') -Value '// Intentional change in the fixture only.'
Expect-Rejection 'Settimino integrity' $case '[SETTIMINO_HASH]'

$case = New-Case 'tracked-key'
# Synthetic string: this is not a real key and must not appear in full in the test.
$fakeKey = ('-----BEGIN ' + 'PRIVATE KEY-----') + "`nDUMMY_CONTENT`n"
[IO.File]::WriteAllText((Join-Path $case 'fixture-private.key'),$fakeKey)
& git -C $case -c core.autocrlf=false add --force -- 'fixture-private.key'
if ($LASTEXITCODE -ne 0) { throw 'Cannot prepare the key fixture.' }
Expect-Rejection 'Private key included in publication candidates' $case '[FORBIDDEN_FILE]'

$case = New-Case 'active-historical-firmware'
[IO.Directory]::CreateDirectory((Join-Path $case 'firmware/Old')) | Out-Null
[IO.File]::WriteAllText((Join-Path $case 'firmware/Old/Old.ino'),'void setup() {} void loop() {}')
Expect-Rejection 'Firmware and archive separation' $case '[LAYOUT]'

$case = New-Case 'missing-secrets-protection'
$ignore = Join-Path $case '.gitignore'
$content = [IO.File]::ReadAllText($ignore).Replace("secrets.h`r`n",'').Replace("secrets.h`n",'')
[IO.File]::WriteAllText($ignore,$content)
Expect-Rejection 'Ignore rule for secrets.h' $case '[IGNORE]'

if ($failures) { throw "$failures negative checks did not detect the expected violation." }
Write-Output '4 negative checks passed; fixtures remain only in the ignored .build directory.'
exit 0
