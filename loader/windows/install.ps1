param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [string]$HostExecutable = (Join-Path $env:ProgramFiles 'Synthesizer V Studio 2 Pro/synthv-studio.exe'),
    [string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'SV2Extend'),
    [string[]]$PluginPackage = @()
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
Add-Type -TypeDefinition @"
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
public static class ExecutableIntegrity {
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ConvertStringSecurityDescriptorToSecurityDescriptor(string s, uint r, out IntPtr sd, IntPtr size);
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool SetFileSecurity(string path, uint info, IntPtr sd);
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool GetFileSecurity(string path, uint info, IntPtr sd, uint length, out uint needed);
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ConvertSecurityDescriptorToStringSecurityDescriptor(IntPtr sd, uint revision, uint info, out IntPtr text, IntPtr length);
 [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr value);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern uint GetFinalPathNameByHandleW(SafeFileHandle file, StringBuilder path, uint size, uint flags);
 public static string Physical(string path, bool directory) {
  using(var file=CreateFileW(path,0x80,7,IntPtr.Zero,3,directory?0x02000000u:0u,IntPtr.Zero)) {
   if(file.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
   var value=new StringBuilder(32768); uint n=GetFinalPathNameByHandleW(file,value,(uint)value.Capacity,0);
   if(n==0 || n>=value.Capacity) throw new Win32Exception(Marshal.GetLastWin32Error());
   var result=value.ToString();
   if(result.StartsWith(@"\\?\UNC\",StringComparison.OrdinalIgnoreCase)) return @"\\"+result.Substring(8);
   if(result.StartsWith(@"\\?\",StringComparison.OrdinalIgnoreCase)) return result.Substring(4);
   throw new InvalidOperationException("Unexpected canonical path.");
  }
 }
 public static void Medium(string path) {
  IntPtr sd=IntPtr.Zero, read=IntPtr.Zero, text=IntPtr.Zero;
  try {
   if(!ConvertStringSecurityDescriptorToSecurityDescriptor("S:(ML;;NW;;;ME)",1,out sd,IntPtr.Zero) || !SetFileSecurity(path,16,sd)) throw new Win32Exception(Marshal.GetLastWin32Error());
   uint n; GetFileSecurity(path,16,IntPtr.Zero,0,out n); if(n==0) throw new Win32Exception(Marshal.GetLastWin32Error());
   read=Marshal.AllocHGlobal((int)n); if(!GetFileSecurity(path,16,read,n,out n) || !ConvertSecurityDescriptorToStringSecurityDescriptor(read,1,16,out text,IntPtr.Zero)) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(Marshal.PtrToStringUni(text)!="S:(ML;;NW;;;ME)") throw new InvalidOperationException("Executable integrity verification failed.");
  } finally { if(sd!=IntPtr.Zero) LocalFree(sd); if(read!=IntPtr.Zero) Marshal.FreeHGlobal(read); if(text!=IntPtr.Zero) LocalFree(text); }
 }
}
"@
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$hostFile = Get-Item -LiteralPath $HostExecutable
if ($hostFile.PSIsContainer -or $hostFile.Name -ne 'synthv-studio.exe') { throw 'Select the official SV2 executable.' }
$install = [IO.Path]::GetFullPath($InstallDirectory).TrimEnd('\', '/')
if ($install -eq [IO.Path]::GetPathRoot($install).TrimEnd('\', '/')) { throw 'An installation directory is required.' }
if ($install.StartsWith($hostFile.DirectoryName + '\', [StringComparison]::OrdinalIgnoreCase) -or $install -eq $hostFile.DirectoryName -or $hostFile.DirectoryName.StartsWith($install + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Keep the Loader separate from the official installation.' }
$profile = Join-Path $install 'Profile'
$native = Join-Path $install 'Native'
$files = @('ExtendLauncher.exe', 'ExtendCore.dll', 'Sv2Mcp.exe', 'THIRD-PARTY-NOTICES.txt')
foreach ($name in $files) {
    $source = Get-Item -LiteralPath (Join-Path $build $name)
    if ($source.PSIsContainer -or $source.Length -eq 0) { throw "Missing native artifact: $name" }
}
$editorResources = Join-Path $build 'EditorLua'
foreach ($name in @('sv2_extend.lua', 'sv2_extend_json.lua', 'editor_provider.lua', 'windows_runtime.lua')) {
    if (!(Test-Path -LiteralPath (Join-Path $editorResources $name) -PathType Leaf)) { throw "Missing editor resource: $name" }
}
foreach ($process in Get-CimInstance Win32_Process) {
    if ($process.ExecutablePath -and $process.ExecutablePath.StartsWith($install + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Close the managed host before upgrading Loader.' }
}
$packages = @()
$PluginPackage = @((Join-Path $build 'plugins/sv2.mcp')) + $PluginPackage
foreach ($package in $PluginPackage) {
    $folder = (Resolve-Path -LiteralPath $package).Path
    if (!(Test-Path -LiteralPath $folder -PathType Container)) { throw 'A plugin package must be a directory.' }
    if ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Plugin packages cannot be links.' }
    if (Get-ChildItem -LiteralPath $folder -Force -Recurse | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Plugin packages cannot contain links.' }
    $definition = Get-Content -LiteralPath (Join-Path $folder 'plugin.json') -Raw | ConvertFrom-Json
    if ($definition.schema -ne 1 -or $definition.id -cnotmatch '^[a-zA-Z][a-zA-Z0-9._-]{0,95}$') { throw 'Invalid plugin package manifest.' }
    if (!$definition.nativeLibraries.'windows-x86_64') { throw 'The package needs a Windows x64 native library.' }
    $relative = [string]$definition.nativeLibraries.'windows-x86_64'
    $library = [IO.Path]::GetFullPath((Join-Path $folder $relative))
    if ([IO.Path]::IsPathRooted($relative) -or !$library.StartsWith($folder.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath $library -PathType Leaf)) { throw 'Invalid Windows plugin library path.' }
    $helpers = @()
    if ($definition.nativeHelpers) {
        $declared = $definition.nativeHelpers.'windows-x86_64'
        if ($null -ne $declared) {
            if ($declared -isnot [array] -or $declared.Count -gt 8) { throw 'Invalid Windows helper list.' }
            foreach ($entry in $declared) {
                if ($entry -isnot [string] -or !$entry -or $entry.Length -gt 512 -or $entry.Contains(':') -or [IO.Path]::IsPathRooted($entry) -or [IO.Path]::GetExtension($entry) -cne '.exe') { throw 'Invalid Windows helper path.' }
                $parts = $entry -split '[\\/]'
                if ($parts -contains '..' -or $parts -contains '.' -or $parts -contains '') { throw 'Invalid Windows helper path.' }
                $helper = [IO.Path]::GetFullPath((Join-Path $folder $entry))
                if (!$helper.StartsWith($folder.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath $helper -PathType Leaf)) { throw 'Invalid Windows helper path.' }
                $helpers += $entry
            }
        }
    }
    if ($packages.id -contains $definition.id) { throw 'Duplicate plugin package ID.' }
    $packages += [pscustomobject]@{ id = $definition.id; path = $folder; helpers = $helpers }
}
$backup = $null
if (Test-Path -LiteralPath $install) {
    if (Get-ChildItem -LiteralPath $install -Force -Recurse | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Refusing to back up an installation containing links.' }
    $backup = Join-Path $env:LOCALAPPDATA ('SV2ExtendBackups/' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $backup | Out-Null
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    & icacls.exe $backup /inheritance:r /grant:r "*${identity}:(OI)(CI)F" '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot protect the upgrade backup.' }
    Copy-Item -LiteralPath $install -Destination (Join-Path $backup 'Installation') -Recurse
    $backupInstallation = [IO.Path]::GetFullPath((Join-Path $backup 'Installation'))
    $extendedBackup = if ($backupInstallation.StartsWith('\\?\')) { $backupInstallation } elseif ($backupInstallation.StartsWith('\\')) { '\\?\UNC\' + $backupInstallation.Substring(2) } else { '\\?\' + $backupInstallation }
    & icacls.exe $extendedBackup /reset /T /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot protect the copied backup.' }
}
New-Item -ItemType Directory -Path $native, (Join-Path $profile 'plugins') -Force | Out-Null
$identity = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
& icacls.exe $install /inheritance:r /grant:r "*${identity}:(OI)(CI)F" '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Cannot protect the Loader installation.' }
foreach ($name in $files) {
    $source = Join-Path $build $name
    $destination = Join-Path $native $name
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) { throw "Installed file verification failed: $name" }
    if ([IO.Path]::GetExtension($name) -eq ".exe") { [ExecutableIntegrity]::Medium($destination) }
}
foreach ($package in $packages) {
    $target = Join-Path (Join-Path $profile 'plugins') $package.id
    if (Test-Path -LiteralPath $target) {
        if (!$backup) { throw 'An existing plugin needs a backup before replacement.' }
        $retired = Join-Path $backup 'ReplacedPlugins'
        New-Item -ItemType Directory -Path $retired -Force | Out-Null
        $resolvedTarget = (Resolve-Path -LiteralPath $target).Path
        if (!$resolvedTarget.StartsWith($profile + '\plugins\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Plugin target escaped the installation.' }
        Move-Item -LiteralPath $resolvedTarget -Destination (Join-Path $retired $package.id)
    }
    Copy-Item -LiteralPath $package.path -Destination $target -Recurse
    foreach ($helper in $package.helpers) { [ExecutableIntegrity]::Medium((Join-Path $target $helper)) }
}
$managedHost = Join-Path $profile 'runtime/Synthesizer V Studio 2 Pro/synthv-studio.exe'
if (Test-Path -LiteralPath $managedHost -PathType Leaf) { [ExecutableIntegrity]::Medium($managedHost) }
$installedResources = Join-Path $native 'EditorLua'
New-Item -ItemType Directory -Path $installedResources -Force | Out-Null
Copy-Item -Path (Join-Path $editorResources '*.lua') -Destination $installedResources -Force
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'Extend Loader.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = [ExecutableIntegrity]::Physical((Join-Path $native 'ExtendLauncher.exe'), $false)
$shortcut.Arguments = '--host "' + $hostFile.FullName + '" "' + [ExecutableIntegrity]::Physical($profile, $true) + '"'
$shortcut.WorkingDirectory = [ExecutableIntegrity]::Physical($native, $true)
$shortcut.Description = 'SV2 Extend Loader'
$shortcut.IconLocation = $hostFile.FullName + ',0'
$shortcut.Save()
$receipt = [ordered]@{
    schema = 1
    platform = 'windows-x86_64'
    installDirectory = [ExecutableIntegrity]::Physical($install, $true)
    profileDirectory = [ExecutableIntegrity]::Physical($profile, $true)
    officialHost = $hostFile.FullName
    shortcut = $shortcutPath
    backup = $backup
    plugins = @($packages | ForEach-Object { $_.id })
    runtimeVerified = $false
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $install 'installation.json') -Encoding utf8
$receipt | ConvertTo-Json -Depth 5
