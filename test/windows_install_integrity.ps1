param([Parameter(Mandatory=$true)][string]$Fixture)
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $PSScriptRoot '../loader/windows/install.ps1') -Raw
$definition=[regex]::Match($source, '(?s)Add-Type -TypeDefinition @"\r?\n(.*?)\r?\n"@').Groups[1].Value
if (!$definition) { throw 'Installer definitions missing' }
Add-Type -TypeDefinition $definition
$root=Join-Path ([IO.Path]::GetTempPath()) ('sv2-integrity-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
 $exe=Join-Path $root 'fixture.exe';Copy-Item -LiteralPath $Fixture -Destination $exe
 $before=(Get-Acl -LiteralPath $exe).Sddl
 & icacls.exe $exe /setintegritylevel L | Out-Null
 if($LASTEXITCODE -ne 0) {throw 'Cannot set test label'}
 & $exe child
 if($LASTEXITCODE -ne 10) {throw 'Low test process mismatch'}
 [ExecutableIntegrity]::Medium($exe)
 & $exe child
 if($LASTEXITCODE -ne 20) {throw 'Medium test process mismatch'}
 if((Get-Acl -LiteralPath $exe).Sddl -ne $before) {throw 'DACL changed'}
 $physical=[ExecutableIntegrity]::Physical($root,$true);$target=[ExecutableIntegrity]::Physical($exe,$false)
 if(![IO.Path]::IsPathFullyQualified($physical) -or ![IO.Path]::IsPathFullyQualified($target) -or ![IO.File]::Exists($target)) {throw 'Canonical path invalid'}
 $link=Join-Path $root 'test.lnk';$shell=New-Object -ComObject WScript.Shell;$shortcut=$shell.CreateShortcut($link);$shortcut.TargetPath=$target;$shortcut.Arguments='--profile "'+$physical+'"';$shortcut.WorkingDirectory=$physical;$shortcut.Save()
 $read=$shell.CreateShortcut($link);if($read.TargetPath -ne $target -or $read.Arguments -ne ('--profile "'+$physical+'"')) {throw 'Shortcut readback mismatch'}
} finally { Remove-Item -LiteralPath $root -Recurse -Force }
