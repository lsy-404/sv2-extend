param(
    [Parameter(Mandatory=$true)][string]$Helper,
    [Parameter(Mandatory=$true)][ValidatePattern("^[0-9a-fA-F]{64}$")][string]$ExpectedSha256,
    [Parameter(Mandatory=$true)][string]$Profile,
    [Parameter(Mandatory=$true)][string]$UserSid
)
$ErrorActionPreference = 'Stop'
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator required' }
$sid = [Security.Principal.SecurityIdentifier]::new($UserSid)
if ($sid.IsWellKnown([Security.Principal.WellKnownSidType]::WorldSid) -or $sid.IsWellKnown([Security.Principal.WellKnownSidType]::BuiltinUsersSid)) { throw 'A user SID is required' }
$profilePath = [IO.Path]::GetFullPath($Profile).TrimEnd('\')
$target = [IO.Path]::Combine($profilePath, 'runtime', 'Synthesizer V Studio 2 Pro', 'synthv-studio.exe')
if ($target.Contains("`n") -or $target.Contains("`r")) { throw 'Invalid profile path' }
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class SV2NetworkExecutablePath {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern uint GetFinalPathNameByHandleW(SafeFileHandle file, StringBuilder path, uint size, uint flags);
}
"@
$handle = [SV2NetworkExecutablePath]::CreateFileW($target, 0x80, 7, [IntPtr]::Zero, 3, 0x80, [IntPtr]::Zero)
if ($handle.IsInvalid) { throw 'Managed host executable is unavailable' }
try {
    $buffer = [Text.StringBuilder]::new(32768)
    $length = [SV2NetworkExecutablePath]::GetFinalPathNameByHandleW($handle, $buffer, [uint32]$buffer.Capacity, 0)
    if ($length -eq 0 -or $length -ge $buffer.Capacity) { throw 'Cannot resolve managed host executable' }
    $physical = $buffer.ToString()
    if ($physical.StartsWith('\\?\UNC\', [StringComparison]::OrdinalIgnoreCase)) { $physical = '\\' + $physical.Substring(8) }
    elseif ($physical.StartsWith('\\?\', [StringComparison]::OrdinalIgnoreCase)) { $physical = $physical.Substring(4) }
    else { throw 'Unexpected canonical executable path' }
    if (-not [string]::Equals($target, $physical, [StringComparison]::OrdinalIgnoreCase)) { throw 'Profile must be the physical canonical path' }
    $target = $physical
} finally { $handle.Dispose() }

$programFiles = [IO.Path]::GetFullPath([Environment]::GetFolderPath('ProgramFiles')).TrimEnd('\')
$source = [IO.Path]::GetFullPath($Helper)
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $ExpectedSha256) { throw 'Helper hash mismatch' }
$directory = [IO.Path]::Combine($programFiles, 'SV2ExtendNetwork')
$binary = [IO.Path]::Combine($directory, 'network-helper.exe')
$ruleName = 'SV2 Extend Managed Host Outbound'
$taskNames = @('SV2 Extend Network Block', 'SV2 Extend Network Allow')
$policy = New-Object -ComObject HNetCfg.FwPolicy2
$scheduler = New-Object -ComObject Schedule.Service
$scheduler.Connect()
$folder = $scheduler.GetFolder('\')
if ([IO.Directory]::Exists($directory) -or [IO.File]::Exists($directory)) { throw 'Network helper directory already exists' }
foreach ($rule in $policy.Rules) { if ($rule.Name -eq $ruleName) { throw 'Network rule already exists' } }
foreach ($name in $taskNames) { try { $null = $folder.GetTask($name); throw 'Network task already exists' } catch [System.Runtime.InteropServices.COMException] { if ($_.Exception.HResult -ne -2147024894) { throw } } }
$createdTasks = [Collections.Generic.List[string]]::new()
$createdRule = $false
$createdDirectory = $false
try {
    $security = [Security.AccessControl.DirectorySecurity]::new()
    $security.SetAccessRuleProtection($true, $false)
    $security.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
    $inherit = [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
    foreach ($id in @('S-1-5-18', 'S-1-5-32-544')) {
        $security.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($id), 'FullControl', $inherit, 'None', 'Allow'))
    }
    $security.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new('S-1-5-32-545'), 'ReadAndExecute', $inherit, 'None', 'Allow'))
    if ($PSVersionTable.PSEdition -eq 'Core') {
        $null = [IO.FileSystemAclExtensions]::CreateDirectory($security, $directory)
    } else {
        $null = [IO.Directory]::CreateDirectory($directory, $security)
    }
    $createdDirectory = $true
    [IO.File]::Copy($source, $binary, $false)
    $fileSecurity = [Security.AccessControl.FileSecurity]::new()
    $fileSecurity.SetAccessRuleProtection($true, $false)
    $fileSecurity.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
    foreach ($id in @('S-1-5-18', 'S-1-5-32-544')) {
        $fileSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($id), 'FullControl', 'Allow'))
    }
    $fileSecurity.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new('S-1-5-32-545'), 'ReadAndExecute', 'Allow'))
    Set-Acl -LiteralPath $binary -AclObject $fileSecurity
    if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $ExpectedSha256) { throw 'Installed helper hash mismatch' }
    $config = [IO.Path]::Combine($directory, 'target.txt')
    [IO.File]::WriteAllBytes($config, [Text.Encoding]::Unicode.GetPreamble() + [Text.Encoding]::Unicode.GetBytes($target))
    Set-Acl -LiteralPath $config -AclObject $fileSecurity
    $rule = New-Object -ComObject HNetCfg.FWRule
    $rule.Name = $ruleName
    $rule.Grouping = 'SV2 Extend Managed Host'
    $rule.ApplicationName = $target
    $rule.Protocol = 256
    $rule.Direction = 2
    $rule.Action = 0
    $rule.Profiles = 0x7fffffff
    $rule.Enabled = $true
    $policy.Rules.Add($rule)
    $createdRule = $true
    $taskAcl = "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;$($sid.Value))"
    for ($i = 0; $i -lt 2; $i++) {
        $definition = $scheduler.NewTask(0)
        $definition.Principal.UserId = 'S-1-5-18'
        $definition.Principal.LogonType = 5
        $definition.Settings.Enabled = $true
        $definition.Settings.AllowDemandStart = $true
        $definition.Settings.DisallowStartIfOnBatteries = $false
        $definition.Settings.StopIfGoingOnBatteries = $false
        $definition.Settings.ExecutionTimeLimit = 'PT30S'
        $action = $definition.Actions.Create(0)
        $action.Path = $binary
        $action.Arguments = @('--block', '--allow')[$i]
        $action.WorkingDirectory = $directory
        $null = $folder.RegisterTaskDefinition($taskNames[$i], $definition, 2, 'SYSTEM', $null, 5, $taskAcl)
        $createdTasks.Add($taskNames[$i])
    }
} catch {
    foreach ($name in $createdTasks) { try { $folder.DeleteTask($name, 0) } catch {} }
    if ($createdRule) { try { $policy.Rules.Remove($ruleName) } catch {} }
    if ($createdDirectory) { [IO.Directory]::Delete($directory, $true) }
    throw
}
