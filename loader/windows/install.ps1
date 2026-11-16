param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [string]$HostExecutable = (Join-Path $env:ProgramFiles 'Synthesizer V Studio 2 Pro/synthv-studio.exe'),
    [string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'SV2Extend'),
    [string[]]$PluginPackage = @()
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
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
    if ($packages.id -contains $definition.id) { throw 'Duplicate plugin package ID.' }
    $packages += [pscustomobject]@{ id = $definition.id; path = $folder }
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
}
$installedResources = Join-Path $native 'EditorLua'
New-Item -ItemType Directory -Path $installedResources -Force | Out-Null
Copy-Item -Path (Join-Path $editorResources '*.lua') -Destination $installedResources -Force
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'Extend Loader.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = Join-Path $native 'ExtendLauncher.exe'
$shortcut.Arguments = '--host "' + $hostFile.FullName + '" "' + $profile + '"'
$shortcut.WorkingDirectory = $native
$shortcut.Description = 'SV2 Extend Loader'
$shortcut.IconLocation = $hostFile.FullName + ',0'
$shortcut.Save()
$receipt = [ordered]@{
    schema = 1
    platform = 'windows-x86_64'
    installDirectory = $install
    profileDirectory = $profile
    officialHost = $hostFile.FullName
    shortcut = $shortcutPath
    backup = $backup
    plugins = @($packages | ForEach-Object { $_.id })
    runtimeVerified = $false
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $install 'installation.json') -Encoding utf8
$receipt | ConvertTo-Json -Depth 5
