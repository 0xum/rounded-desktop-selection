param([string]$ConfigPath, [string]$SettingsPath)
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
Assert-Administrator
$project = Get-ProjectInfo $ConfigPath
$windhawk = Get-WindhawkContext $project -ForInstall
if (Test-Path -LiteralPath $windhawk.ModPath) { throw 'Mod already installed. Use update.ps1.' }
if (-not $SettingsPath) { $SettingsPath = Join-Path $PSScriptRoot 'settings.default.json' }
$settings = Read-ModSettings $SettingsPath
$libraryPath = Join-Path $project.Build $project.Library
if (-not (Test-Path -LiteralPath $libraryPath)) { throw 'Build the mod before installing.' }
$created = $false
try {
    New-Item -Path $windhawk.ModPath -Force | Out-Null
    $created = $true
    New-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value 1 -PropertyType DWord -Force | Out-Null
    Copy-Item -LiteralPath $libraryPath -Destination (Join-Path $windhawk.ModsDirectory $project.Library) -Force
    Copy-Item -LiteralPath $project.Source -Destination (Join-Path $windhawk.SourceDirectory ($project.LocalId + '.wh.cpp')) -Force
    $settingsKey = $windhawk.ModPath + '\Settings'
    New-Item -Path $settingsKey -Force | Out-Null
    foreach ($entry in @{ LibraryFileName = $project.Library; Include = 'explorer.exe'; Exclude = '';
                         Architecture = 'x86-64'; Version = $project.Version }.GetEnumerator()) {
        New-ItemProperty -LiteralPath $windhawk.ModPath -Name $entry.Key -Value $entry.Value -PropertyType String -Force | Out-Null
    }
    New-ItemProperty -LiteralPath $windhawk.ModPath -Name LoggingEnabled -Value 0 -PropertyType DWord -Force | Out-Null
    foreach ($property in $settings.PSObject.Properties) {
        $kind = if ($property.Name -eq 'Color') { 'String' } else { 'DWord' }
        New-ItemProperty -LiteralPath $settingsKey -Name $property.Name -Value $property.Value -PropertyType $kind -Force | Out-Null
    }
    New-ItemProperty -LiteralPath $windhawk.ModPath -Name SettingsChangeTime -Value ([int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -PropertyType DWord -Force | Out-Null
    Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value 0 -Type DWord
    Write-Output "Installed $($project.LocalId) $($project.Version)."
} catch {
    if ($created) { Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value 1 -ErrorAction SilentlyContinue }
    throw
}
