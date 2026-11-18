param([Parameter(Mandatory = $true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$testRoot = Join-Path (Split-Path $build -Parent) ('clean-profile-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$outside = Join-Path $testRoot 'original.txt'
Set-Content -LiteralPath $outside -Value 'original fixture'
$profile = Join-Path $testRoot 'Profile'
New-Item -ItemType Directory -Path (Join-Path $profile 'plugins') -Force | Out-Null
$report = Join-Path $profile 'outputs/namespace-result.json'
$arguments = '--managed-clean-fixture "' + (Join-Path $build 'namespace-host/NamespaceFixture.exe') + '" "' + $profile + '" "' + $report + '" "' + $outside + '"'
$launchFile = Join-Path $testRoot 'launch.json'
$launcher = Start-Process -FilePath (Join-Path $build 'ExtendLauncher.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $launchFile -RedirectStandardError (Join-Path $testRoot 'launch-error.txt')
if (!$launcher.WaitForExit(20000) -or $launcher.ExitCode -ne 0) { throw 'Clean managed bootstrap failed' }
$receipt = Get-Content -LiteralPath $launchFile -Raw | ConvertFrom-Json
if (!$receipt.residentCore -or $receipt.dataCopied) { throw 'Invalid clean profile receipt' }
$deadline = [DateTime]::UtcNow.AddSeconds(15)
while (!(Test-Path -LiteralPath $report) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if (!$result.passed -or $result.hostPid -ne $receipt.hostPid) { throw 'Clean managed namespace checks failed' }
$process = Get-Process -Id $receipt.hostPid -ErrorAction SilentlyContinue
if ($process) { $process.WaitForExit(10000) | Out-Null }
if (Get-Process -Id $receipt.hostPid -ErrorAction SilentlyContinue) { throw 'Clean managed fixture did not exit' }
$duplicate = Start-Process -FilePath (Join-Path $build 'ExtendLauncher.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testRoot 'duplicate.json') -RedirectStandardError (Join-Path $testRoot 'duplicate-error.txt')
if (!$duplicate.WaitForExit(10000) -or $duplicate.ExitCode -ne 75) { throw 'Initialized profile was not rejected' }
$unowned = Join-Path $testRoot 'Unowned'
New-Item -ItemType Directory -Path $unowned | Out-Null
Set-Content -LiteralPath (Join-Path $unowned 'existing.txt') -Value 'preserved fixture'
$unownedArguments = '--managed-clean-fixture "' + (Join-Path $build 'namespace-host/NamespaceFixture.exe') + '" "' + $unowned + '"'
$rejected = Start-Process -FilePath (Join-Path $build 'ExtendLauncher.exe') -ArgumentList $unownedArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testRoot 'unowned.json') -RedirectStandardError (Join-Path $testRoot 'unowned-error.txt')
if (!$rejected.WaitForExit(10000) -or $rejected.ExitCode -ne 75) { throw 'Nonempty unowned profile was not rejected' }
if ((Get-Content -LiteralPath (Join-Path $unowned 'existing.txt') -Raw).Trim() -ne 'preserved fixture') { throw 'Unowned profile changed' }
if (Test-Path -LiteralPath (Join-Path $unowned '.host.lock')) { throw 'Unowned profile was modified' }
[pscustomobject]@{
    passed = $true
    hostPid = $receipt.hostPid
    profileDataCopied = $receipt.dataCopied
    initializedProfileRejected = $true
    unownedProfileRejected = $true
    namespaceChecks = $result.checks
} | ConvertTo-Json -Depth 6
