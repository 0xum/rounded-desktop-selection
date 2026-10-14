param([string]$ConfigPath)
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
Assert-Administrator
$project = Get-ProjectInfo $ConfigPath
$windhawk = Get-WindhawkContext $project -ForInstall
Set-ItemProperty -LiteralPath $windhawk.ModPath -Name Disabled -Value 1 -Type DWord
