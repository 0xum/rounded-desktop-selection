param([string]$ConfigPath)
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
Assert-Administrator
$project = Get-ProjectInfo $ConfigPath
$windhawk = Get-WindhawkContext $project -ForInstall
if (-not (Test-Path -LiteralPath $windhawk.ModPath)) { throw 'Mod not installed. Use install.ps1.' }
$libraryPath = Join-Path $project.Build $project.Library
if (-not (Test-Path -LiteralPath $libraryPath)) { throw 'Build the mod before updating.' }
$wasDisabled = Get-ItemPropertyValue -LiteralPath $windhawk.ModPath -Name Disabled
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value 1 -Type DWord
Wait-ModUnload $project.LocalId
Copy-Item -LiteralPath $libraryPath -Destination (Join-Path $windhawk.ModsDirectory $project.Library) -Force
Copy-Item -LiteralPath $project.Source -Destination (Join-Path $windhawk.SourceDirectory ($project.LocalId + '.wh.cpp')) -Force
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name LibraryFileName -Value $project.Library
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Version -Value $project.Version
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name SettingsChangeTime -Value ([int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -Type DWord
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value $wasDisabled -Type DWord
Write-Output "Updated $($project.LocalId) to $($project.Version); settings and enabled state preserved."
