. (Join-Path $PSScriptRoot 'common.ps1')
$project = Get-ProjectInfo
$settings = Read-ModSettings (Join-Path $project.Root 'settings.default.json')
$source = Get-Content -Raw -LiteralPath $project.Source
$settingsBlock = [regex]::Match($source, '(?s)// ==WindhawkModSettings==(.*?)// ==/WindhawkModSettings==').Groups[1].Value
foreach ($property in $settings.PSObject.Properties) {
    $pattern = '(?m)^- ' + [regex]::Escape($property.Name) + ':\s*([^\r\n]+)'
    $match = [regex]::Match($settingsBlock, $pattern)
    $value = $match.Groups[1].Value.Trim().Trim('"')
    if (-not $match.Success -or $value -ne [string]$property.Value) {
        throw "Installer default differs from Windhawk metadata: $($property.Name)."
    }
}
$scripts = Get-ChildItem -LiteralPath $project.Root -Filter '*.ps1'
$scripts += Get-ChildItem -LiteralPath (Join-Path $project.Root 'scripts') -Filter '*.ps1'
foreach ($script in $scripts) {
    $tokens = $null
    $errors = $null
    [Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count) { throw "PowerShell syntax errors in $($script.Name): $errors" }
}
Write-Output "Project checks passed: $($project.Id) $($project.Version), $($scripts.Count) scripts, settings defaults match."
