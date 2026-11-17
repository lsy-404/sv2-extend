$ErrorActionPreference='Stop'
$scriptPath=Join-Path $PSScriptRoot '../loader/windows/update-network-helper.ps1'
$text=[IO.File]::ReadAllText((Resolve-Path $scriptPath))
$start=$text.IndexOf('$locks=@()')
if ($start -lt 0) { throw 'Transaction body missing' }
$transaction=[scriptblock]::Create($text.Substring($start))
$directory=Join-Path $PSScriptRoot ('.outputs/network-update-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($directory) | Out-Null
$ExpectedOldProfile=Join-Path $directory 'old-profile';$Profile=Join-Path $directory 'new-profile'
[IO.Directory]::CreateDirectory($ExpectedOldProfile) | Out-Null
[IO.Directory]::CreateDirectory($Profile) | Out-Null
$oldTarget='C:\fixture\old.exe';$newTarget='C:\fixture\new.exe'
$binary=Join-Path $directory 'network-helper.exe';$config=Join-Path $directory 'target.txt';$source=Join-Path $directory 'source.exe'
[IO.File]::WriteAllText($binary,'old-helper');[IO.File]::WriteAllText($source,'new-helper')
$oldBytes=[Text.Encoding]::Unicode.GetPreamble()+[Text.Encoding]::Unicode.GetBytes($oldTarget)
[IO.File]::WriteAllBytes($config,$oldBytes)
$ExpectedSha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$ExpectedOldSha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash;$expectedBytes=$oldBytes
function Same([string]$a,[string]$b) { [string]::Equals($a,$b,[StringComparison]::OrdinalIgnoreCase) }
function ExpectedAcl([bool]$unused) { return $null }
function AclText($unused) { return 'fixture' }
function Get-Acl { param($LiteralPath) return $null }
function Set-Acl { param($LiteralPath,$AclObject) }
function Get-CimInstance { param($ClassName,$Filter) return @() }
$rule=[pscustomobject]@{ApplicationName=$oldTarget;Enabled=$true};$oldEnabled=$true
$tasks=@();for($i=0;$i -lt 2;$i++){ $task=[pscustomobject]@{Enabled=$true};$task | Add-Member ScriptMethod GetInstances { param($flags) return @{Count=0} };$tasks += $task }
& $transaction
if ($rule.ApplicationName -ne $newTarget -or -not $tasks[0].Enabled -or -not $tasks[1].Enabled -or [IO.File]::ReadAllText($binary) -ne 'new-helper') { throw 'Successful update did not apply' }
[IO.File]::WriteAllText($binary,'old-helper');[IO.File]::WriteAllBytes($config,$oldBytes);$rule.ApplicationName=$oldTarget
$rule.PSObject.Properties.Remove('ApplicationName');$script:actualTarget=$oldTarget;$script:failOnce=$true
$rule | Add-Member ScriptProperty ApplicationName { $script:actualTarget } { param($value) $script:actualTarget=$value;if($script:failOnce -and $value -eq $newTarget){$script:failOnce=$false;throw 'injected rule failure'} }
$failed=$false;try { & $transaction } catch { $failed=$true;Write-Output $_.ToString() }
if (-not $failed -or $rule.ApplicationName -ne $oldTarget -or -not $tasks[0].Enabled -or -not $tasks[1].Enabled -or [IO.File]::ReadAllText($binary) -ne 'old-helper' -or [Convert]::ToBase64String([IO.File]::ReadAllBytes($config)) -cne [Convert]::ToBase64String($oldBytes)) { throw "Failed update was not rolled back: failed=$failed rule=$($rule.ApplicationName -eq $oldTarget) tasks=$($tasks[0].Enabled),$($tasks[1].Enabled) binary=$([IO.File]::ReadAllText($binary) -eq 'old-helper') config=$([Convert]::ToBase64String([IO.File]::ReadAllBytes($config)) -ceq [Convert]::ToBase64String($oldBytes))" }
$held=[IO.File]::Open((Join-Path $ExpectedOldProfile '.host.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try { $failed=$false;try { & $transaction } catch { $failed=$true };if (-not $failed -or $rule.ApplicationName -ne $oldTarget -or [IO.File]::ReadAllText($binary) -ne 'old-helper') { throw 'Live host lock was not respected' } } finally { $held.Dispose() }
Write-Output 'Network update success, rollback and host lock passed'
