param([Parameter(Mandatory = $true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$testRoot = Join-Path (Split-Path $build -Parent) ('managed-smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$outside = Join-Path $testRoot 'original.txt'
Set-Content -LiteralPath $outside -Value 'original fixture'
$profile = Join-Path $testRoot 'Profile'
$report = Join-Path $profile 'outputs/namespace-result.json'
$arguments = '--managed-fixture "' + (Join-Path $build 'namespace-host/NamespaceFixture.exe') + '" "' + $profile + '" "' + $report + '" "' + $outside + '"'
$launchFile = Join-Path $testRoot 'launch.json'
$launcher = Start-Process -FilePath (Join-Path $build 'ExtendLauncher.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $launchFile -RedirectStandardError (Join-Path $testRoot 'launch-error.txt')
if (!$launcher.WaitForExit(20000) -or $launcher.ExitCode -ne 0) { throw 'Managed bootstrap failed' }
$receipt = Get-Content -LiteralPath $launchFile -Raw | ConvertFrom-Json
if ($receipt.appContainer -or $receipt.fixtureOnly -or !$receipt.residentCore) { throw 'Invalid managed receipt' }
$duplicate = Start-Process -FilePath (Join-Path $build 'ExtendLauncher.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testRoot 'duplicate.json') -RedirectStandardError (Join-Path $testRoot 'duplicate-error.txt')
if (!$duplicate.WaitForExit(10000) -or $duplicate.ExitCode -ne 75) { throw 'The running profile was not locked' }
$deadline = [DateTime]::UtcNow.AddSeconds(15)
while (!(Test-Path -LiteralPath $report) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if (!$result.passed -or $result.hostPid -ne $receipt.hostPid) { throw 'Managed namespace checks failed' }
$process = Get-Process -Id $receipt.hostPid -ErrorAction SilentlyContinue
if ($process) { $process.WaitForExit(5000) | Out-Null }
if (Get-Process -Id $receipt.hostPid -ErrorAction SilentlyContinue) { throw 'Managed fixture did not exit' }
if (Get-Process -Id $receipt.launcherPid -ErrorAction SilentlyContinue) { throw 'Launcher remained resident' }
if ((Get-Content -LiteralPath $outside -Raw).Trim() -ne 'original fixture') { throw 'Original fixture changed' }
$originalHost = Get-Item -LiteralPath (Join-Path $build 'namespace-host/NamespaceFixture.exe')
$cachedHost = Get-Item -LiteralPath (Join-Path $profile 'runtime/Synthesizer V Studio 2 Pro/NamespaceFixture.exe')
if ($originalHost.LastWriteTimeUtc -ne $cachedHost.LastWriteTimeUtc) { throw 'The cached host timestamp was not preserved' }
$result | Add-Member -NotePropertyName profileLockVerified -NotePropertyValue $true
$result | Add-Member -NotePropertyName cachedTimestampPreserved -NotePropertyValue $true
$result | ConvertTo-Json -Depth 6
