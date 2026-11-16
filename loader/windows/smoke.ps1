param([Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path $BuildDirectory).Path
$report = Join-Path $build 'windows-runtime-results.json'
Remove-Item $report -ErrorAction SilentlyContinue
$launcher = Join-Path $build 'ExtendLauncher.exe'
$fixture = Join-Path $build 'ExtendFixture.exe'
$profile = Join-Path $build 'smoke-profile'
$receipt = & $launcher --fixture $fixture $profile $report
if ($LASTEXITCODE -ne 0) { throw "Bootstrap failed: $LASTEXITCODE" }
$launch = $receipt | ConvertFrom-Json
if (!$launch.residentCore -or !$launch.fixtureOnly) { throw 'Invalid launcher receipt' }
$deadline = [DateTime]::UtcNow.AddSeconds(10)
while (!(Test-Path $report) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
$result = Get-Content $report -Raw | ConvertFrom-Json
if (!$result.passed -or $result.hostPid -ne $launch.hostPid) { throw 'Resident fixture verification failed' }
$hostProcess = Get-Process -Id $launch.hostPid -ErrorAction SilentlyContinue
if ($hostProcess) { $hostProcess.WaitForExit(10000) | Out-Null }
if (Get-Process -Id $launch.hostPid -ErrorAction SilentlyContinue) { throw 'Test host did not exit' }
if (Get-Process -Id $launch.launcherPid -ErrorAction SilentlyContinue) { throw 'Launcher remained resident' }
$result | ConvertTo-Json -Depth 12
