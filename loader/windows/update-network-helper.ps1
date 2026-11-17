param(
    [Parameter(Mandatory=$true)][string]$Helper,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedOldSha256,
    [Parameter(Mandatory=$true)][string]$ExpectedOldProfile,
    [Parameter(Mandatory=$true)][string]$Profile,
    [Parameter(Mandatory=$true)][string]$UserSid
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not ([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator required' }
$sid = [Security.Principal.SecurityIdentifier]::new($UserSid)
if ($sid.IsWellKnown([Security.Principal.WellKnownSidType]::WorldSid) -or $sid.IsWellKnown([Security.Principal.WellKnownSidType]::BuiltinUsersSid)) { throw 'A user SID is required' }
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class SV2UpdatePath {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern uint GetFinalPathNameByHandleW(SafeFileHandle file, StringBuilder path, uint size, uint flags);
}
"@
function PhysicalHost([string]$profile) {
    $root = [IO.Path]::GetFullPath($profile).TrimEnd('\')
    $path = [IO.Path]::Combine($root, 'runtime', 'Synthesizer V Studio 2 Pro', 'synthv-studio.exe')
    if ($path.Contains("`n") -or $path.Contains("`r")) { throw 'Invalid profile' }
    $handle = [SV2UpdatePath]::CreateFileW($path, 0x80, 7, [IntPtr]::Zero, 3, 0x80, [IntPtr]::Zero)
    if ($handle.IsInvalid) { throw 'Managed host executable is unavailable' }
    try {
        $buffer = [Text.StringBuilder]::new(32768)
        $length = [SV2UpdatePath]::GetFinalPathNameByHandleW($handle, $buffer, [uint32]$buffer.Capacity, 0)
        if ($length -eq 0 -or $length -ge $buffer.Capacity) { throw 'Cannot resolve managed host executable' }
        $physical = $buffer.ToString()
        if ($physical.StartsWith('\\?\UNC\', [StringComparison]::OrdinalIgnoreCase)) { $physical = '\\' + $physical.Substring(8) }
        elseif ($physical.StartsWith('\\?\', [StringComparison]::OrdinalIgnoreCase)) { $physical = $physical.Substring(4) }
        else { throw 'Unexpected canonical executable path' }
        if (-not [string]::Equals($path, $physical, [StringComparison]::OrdinalIgnoreCase)) { throw 'Profile must be the physical canonical path' }
        return $physical
    } finally { $handle.Dispose() }
}
function Same([string]$a,[string]$b) { return [string]::Equals($a,$b,[StringComparison]::OrdinalIgnoreCase) }
function Unrestricted([string]$value) { return [string]::IsNullOrEmpty($value) -or $value -eq '*' }
function AclText($acl) { return $acl.GetSecurityDescriptorSddlForm([Security.AccessControl.AccessControlSections]::Owner -bor [Security.AccessControl.AccessControlSections]::Access).Replace('D:PAI(','D:P(') }
function ExpectedAcl([bool]$directory) {
    if ($directory) { $acl = [Security.AccessControl.DirectorySecurity]::new() } else { $acl = [Security.AccessControl.FileSecurity]::new() }
    $acl.SetAccessRuleProtection($true,$false);$acl.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
    foreach ($entry in @(@('S-1-5-18','FullControl'),@('S-1-5-32-544','FullControl'),@('S-1-5-32-545','ReadAndExecute'))) {
        $identity = [Security.Principal.SecurityIdentifier]::new($entry[0])
        if ($directory) { $rule = [Security.AccessControl.FileSystemAccessRule]::new($identity,$entry[1],[Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit','None','Allow') }
        else { $rule = [Security.AccessControl.FileSystemAccessRule]::new($identity,$entry[1],'Allow') }
        $acl.AddAccessRule($rule)
    }
    return $acl
}
$oldTarget = PhysicalHost $ExpectedOldProfile
$newTarget = PhysicalHost $Profile
if (Same $oldTarget $newTarget) { throw 'Profiles must differ' }
$directory = [IO.Path]::Combine([Environment]::GetFolderPath('ProgramFiles'),'SV2ExtendNetwork')
$binary = [IO.Path]::Combine($directory,'network-helper.exe')
$config = [IO.Path]::Combine($directory,'target.txt')
$source = [IO.Path]::GetFullPath($Helper)
foreach ($path in @($directory,$binary,$config)) {
    $item = Get-Item -LiteralPath $path -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Unexpected reparse point' }
    $expected = ExpectedAcl ($path -eq $directory)
    if ((AclText (Get-Acl -LiteralPath $path)) -ne (AclText $expected)) { throw 'Unexpected helper ACL' }
}
if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $ExpectedOldSha256 -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $ExpectedSha256) { throw 'Helper identity mismatch' }
$bytes = [IO.File]::ReadAllBytes($config)
$expectedBytes = [Text.Encoding]::Unicode.GetPreamble() + [Text.Encoding]::Unicode.GetBytes($oldTarget)
if ([Convert]::ToBase64String($bytes) -cne [Convert]::ToBase64String($expectedBytes)) { throw 'Unexpected existing target' }
$policy = New-Object -ComObject HNetCfg.FwPolicy2
$matches = @($policy.Rules | Where-Object { $_.Name -eq 'SV2 Extend Managed Host Outbound' })
if ($matches.Count -ne 1) { throw 'Unexpected rule count' }
$rule = $matches[0]
if (-not (Same $rule.ApplicationName $oldTarget) -or $rule.Grouping -ne 'SV2 Extend Managed Host' -or $rule.Direction -ne 2 -or $rule.Action -ne 0 -or $rule.Protocol -ne 256 -or $rule.Profiles -ne 0x7fffffff -or -not (Unrestricted $rule.ServiceName) -or -not (Unrestricted $rule.LocalAddresses) -or -not (Unrestricted $rule.RemoteAddresses) -or $rule.Interfaces -or ($rule.InterfaceTypes -ne 'All' -and -not (Unrestricted $rule.InterfaceTypes)) -or $rule.EdgeTraversal) { throw 'Unexpected firewall rule' }
foreach ($field in @('LocalAppPackageId','LocalUserOwner','LocalUserAuthorizedList','RemoteUserAuthorizedList','RemoteMachineAuthorizedList')) { if (-not (Unrestricted $rule.$field)) { throw 'Unexpected firewall restriction' } }
if ($rule.SecureFlags -ne 0) { throw 'Unexpected firewall security flags' }
$oldEnabled = $rule.Enabled
$scheduler = New-Object -ComObject Schedule.Service;$scheduler.Connect();$folder=$scheduler.GetFolder('\')
$tasks = @();$names=@('SV2 Extend Network Block','SV2 Extend Network Allow');$arguments=@('--block','--allow')
$expectedTaskAcl=[Security.AccessControl.RawSecurityDescriptor]::new("D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;$($sid.Value))")
$normalizedTaskAcl=[Security.AccessControl.RawSecurityDescriptor]::new("D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;0x1200a9;;;$($sid.Value))(A;;FR;;;SY)")
for ($i=0;$i -lt 2;$i++) {
    $task=$folder.GetTask($names[$i]);$definition=$task.Definition;$action=$definition.Actions.Item(1)
    $taskAcl=[Security.AccessControl.RawSecurityDescriptor]::new($task.GetSecurityDescriptor(4))
    $aclText=$taskAcl.GetSddlForm('Access')
    if ($task.GetInstances(0).Count -ne 0 -or -not $task.Enabled -or $definition.Triggers.Count -ne 0 -or $definition.Actions.Count -ne 1 -or $action.Type -ne 0 -or -not (Same $action.Path $binary) -or $action.Arguments -cne $arguments[$i] -or -not (Same $action.WorkingDirectory $directory) -or $definition.Principal.UserId -notin @('S-1-5-18','SYSTEM') -or $definition.Principal.LogonType -ne 5 -or ($aclText -ne $expectedTaskAcl.GetSddlForm('Access') -and $aclText -ne $normalizedTaskAcl.GetSddlForm('Access'))) { throw 'Unexpected network task' }
    $tasks += $task
}
$locks=@();$disabled=@();$replacedBinary=$false;$replacedConfig=$false;$ruleChanged=$false
$suffix=[Guid]::NewGuid().ToString('N');$stagedBinary=Join-Path $directory ('.new-helper-'+$suffix);$stagedConfig=Join-Path $directory ('.new-target-'+$suffix);$backupBinary=Join-Path $directory ('.old-helper-'+$suffix);$backupConfig=Join-Path $directory ('.old-target-'+$suffix)
try {
    foreach ($path in @($ExpectedOldProfile,$Profile)) { $locks += [IO.File]::Open([IO.Path]::Combine([IO.Path]::GetFullPath($path),'.host.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read) }
    foreach ($process in @(Get-CimInstance Win32_Process -Filter "Name='synthv-studio.exe'")) {
        if (-not $process.ExecutablePath -or (Same $process.ExecutablePath $oldTarget) -or (Same $process.ExecutablePath $newTarget)) { throw 'Managed host is running or cannot be inspected' }
    }
    foreach ($task in $tasks) { $disabled += $task;$task.Enabled=$false }
    foreach ($task in $tasks) { if ($task.GetInstances(0).Count -ne 0) { throw 'Network task is running' } }
    [IO.File]::Copy($source,$stagedBinary,$false);Set-Acl -LiteralPath $stagedBinary -AclObject (ExpectedAcl $false)
    if ((Get-FileHash -LiteralPath $stagedBinary -Algorithm SHA256).Hash -ne $ExpectedSha256) { throw 'Staged helper hash mismatch' }
    [IO.File]::WriteAllBytes($stagedConfig,[Text.Encoding]::Unicode.GetPreamble()+[Text.Encoding]::Unicode.GetBytes($newTarget));Set-Acl -LiteralPath $stagedConfig -AclObject (ExpectedAcl $false)
    [IO.File]::Replace($stagedBinary,$binary,$backupBinary);$replacedBinary=$true
    [IO.File]::Replace($stagedConfig,$config,$backupConfig);$replacedConfig=$true
    $ruleChanged=$true;$rule.ApplicationName=$newTarget
    if (-not (Same $rule.ApplicationName $newTarget) -or $rule.Enabled -ne $oldEnabled -or (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $ExpectedSha256 -or [Convert]::ToBase64String([IO.File]::ReadAllBytes($config)) -cne [Convert]::ToBase64String([Text.Encoding]::Unicode.GetPreamble()+[Text.Encoding]::Unicode.GetBytes($newTarget))) { throw 'Update verification failed' }
    foreach ($path in @($binary,$config)) { if ((AclText (Get-Acl -LiteralPath $path)) -ne (AclText (ExpectedAcl $false))) { throw 'Updated helper ACL mismatch' } }
    foreach ($task in $disabled) { $task.Enabled=$true;if (-not $task.Enabled) { throw 'Task restoration failed' } };$disabled=@()
} catch {
    $failure=$_;$rollback=@()
    foreach ($task in $disabled) { try { $task.Enabled=$false } catch { $rollback += $_.ToString() } }
    if ($ruleChanged) { try { $rule.ApplicationName=$oldTarget;if (-not (Same $rule.ApplicationName $oldTarget)) { throw 'Rule rollback failed' } } catch { $rollback += $_.ToString() } }
    if ($replacedConfig) { try { [IO.File]::Replace($backupConfig,$config,$stagedConfig) } catch { $rollback += $_.ToString() } }
    if ($replacedBinary) { try { [IO.File]::Replace($backupBinary,$binary,$stagedBinary) } catch { $rollback += $_.ToString() } }
    try {
        if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $ExpectedOldSha256 -or [Convert]::ToBase64String([IO.File]::ReadAllBytes($config)) -cne [Convert]::ToBase64String($expectedBytes) -or -not (Same $rule.ApplicationName $oldTarget) -or $rule.Enabled -ne $oldEnabled) { throw 'Rollback verification failed' }
    } catch { $rollback += $_.ToString() }
    if (-not $rollback.Count) { foreach ($task in $disabled) { try { $task.Enabled=$true;if (-not $task.Enabled) { throw 'Task restoration failed' } } catch { $rollback += $_.ToString() } } }
    if ($rollback.Count) { throw "Update failed: $failure; rollback failed: $($rollback -join '; ')" }
    throw $failure
} finally {
    foreach ($lock in $locks) { $lock.Dispose() }
    foreach ($path in @($stagedBinary,$stagedConfig)) { if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) } }
}
foreach ($path in @($backupBinary,$backupConfig)) { try { [IO.File]::Delete($path) } catch { Write-Warning 'Could not remove protected backup' } }
