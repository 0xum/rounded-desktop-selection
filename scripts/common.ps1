Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProjectInfo {
    param([string]$ConfigPath)
    $root = Split-Path $PSScriptRoot -Parent
    if (-not $ConfigPath) {
        $ConfigPath = Join-Path $root 'project.config.local.json'
        if (-not (Test-Path -LiteralPath $ConfigPath)) { $ConfigPath = Join-Path $root 'project.config.json' }
    }
    $config = Get-Content -Raw -LiteralPath $ConfigPath | ConvertFrom-Json
    $source = Join-Path $root 'rounded-desktop-selection.wh.cpp'
    $text = Get-Content -Raw -LiteralPath $source
    $id = [regex]::Match($text, '(?m)^// @id\s+([a-z0-9-]+)\s*$').Groups[1].Value
    $version = [regex]::Match($text, '(?m)^// @version\s+([0-9.]+)\s*$').Groups[1].Value
    if (-not $id -or -not $version) { throw 'Missing mod ID or version in source metadata.' }
    [version]$version | Out-Null
    $windhawkRoot = $config.windhawkRoot
    if (-not $windhawkRoot) {
        $programFiles = [Environment]::GetEnvironmentVariable('ProgramW6432')
        if (-not $programFiles) { $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles') }
        $windhawkRoot = Join-Path $programFiles 'Windhawk'
    }
    $build = $config.buildDirectory
    if (-not $build) { throw 'buildDirectory must not be empty.' }
    if (-not [IO.Path]::IsPathRooted($build)) { $build = Join-Path $root $build }
    [pscustomobject]@{
        Root = $root; Source = $source; Id = $id; LocalId = 'local@' + $id; Version = $version
        Library = 'local@' + $id + '_' + $version + '.dll'
        Build = [IO.Path]::GetFullPath($build)
        WindhawkRoot = [IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($windhawkRoot))
    }
}

function Read-StorageIni {
    param([string]$Path)
    $values = @{}
    $inStorage = $false
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $inStorage = $Matches[1] -eq 'Storage'; continue }
        if ($inStorage -and $line -match '^\s*([^;=]+?)\s*=\s*(.*?)\s*$') { $values[$Matches[1]] = $Matches[2] }
    }
    return $values
}

function Resolve-StoragePath {
    param([string]$Base, [string]$Value)
    if (-not $Value) { throw 'A required storage path is missing from windhawk.ini.' }
    $expanded = [Environment]::ExpandEnvironmentVariables($Value)
    if (-not [IO.Path]::IsPathRooted($expanded)) { $expanded = Join-Path $Base $expanded }
    return [IO.Path]::GetFullPath($expanded)
}

function Get-WindhawkContext {
    param($Project, [switch]$ForInstall)
    $storage = Read-StorageIni (Join-Path $Project.WindhawkRoot 'windhawk.ini')
    $compiler = Resolve-StoragePath $Project.WindhawkRoot $storage.CompilerPath
    $engine = Resolve-StoragePath $Project.WindhawkRoot $storage.EnginePath
    if (-not (Test-Path -LiteralPath (Join-Path $compiler 'bin\clang++.exe'))) {
        throw 'Windhawk compiler not found. Configure windhawkRoot in project.config.local.json.'
    }
    $context = [ordered]@{ Compiler = $compiler; Engine = $engine }
    if ($ForInstall) {
        if ($storage.Portable -ne '0') { throw 'Installation scripts support installed Windhawk only. Use its editor for portable installations.' }
        $appData = Resolve-StoragePath $Project.WindhawkRoot $storage.AppDataPath
        $engineStorage = Read-StorageIni (Join-Path $engine 'engine.ini')
        if ($engineStorage.RegistryKey -notmatch '^HKLM\\(.+)') { throw 'Expected an HKLM Windhawk engine registry backend.' }
        $context.ModPath = 'HKLM:\' + $Matches[1] + '\Mods\' + $Project.LocalId
        $engineData = Resolve-StoragePath $engine $engineStorage.AppDataPath
        $context.ModsDirectory = Join-Path $engineData 'Mods\64'
        $context.SourceDirectory = Join-Path $appData 'ModsSource'
    }
    return [pscustomobject]$context
}

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run this script from an elevated PowerShell session, or manage the mod in Windhawk.'
    }
}

function Read-ModSettings {
    param([string]$Path)
    $settings = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
    $ranges = @{ FillOpacity = @(0, 100); BorderOpacity = @(0, 100); CornerRadius = @(0, 64); BorderWidth = @(0, 8) }
    $switches = @('ExplorerHighlights', 'ExplorerMarquee')
    $names = @($settings.PSObject.Properties.Name)
    if ($names.Count -ne 7 -or @($names | Where-Object { $_ -ne 'Color' -and -not $ranges.ContainsKey($_) -and $_ -notin $switches }).Count) {
        throw 'Settings must contain Color, FillOpacity, BorderOpacity, CornerRadius, BorderWidth, ExplorerHighlights and ExplorerMarquee only.'
    }
    if ($settings.Color -notmatch '^#[0-9a-fA-F]{6}$') { throw 'Color must have the form #RRGGBB.' }
    foreach ($name in $switches) {
        if ($settings.$name -isnot [bool]) { throw "$name must be true or false." }
    }
    foreach ($name in $ranges.Keys) {
        $number = 0
        if (-not [int]::TryParse([string]$settings.$name, [ref]$number) -or $number -lt $ranges[$name][0] -or $number -gt $ranges[$name][1]) {
            throw "Invalid setting: $name. Expected an integer in $($ranges[$name][0])..$($ranges[$name][1])."
        }
    }
    return $settings
}

function Wait-ModUnload {
    param([string]$LocalId)
    $deadline = (Get-Date).AddSeconds(10)
    while (@(Get-Process explorer -Module -ErrorAction Stop |
        Where-Object { $_.ModuleName.StartsWith($LocalId + '_') -and $_.ModuleName.EndsWith('.dll') }).Count -gt 0) {
        if ((Get-Date) -gt $deadline) { throw 'The mod has not unloaded. Close the drag selection and retry.' }
        Start-Sleep -Milliseconds 200
    }
}
