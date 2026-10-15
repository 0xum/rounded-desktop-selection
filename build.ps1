param([switch]$Test, [string]$ConfigPath)
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
$project = Get-ProjectInfo $ConfigPath
$windhawk = Get-WindhawkContext $project
New-Item -ItemType Directory -Path $project.Build -Force | Out-Null
$common = @('-std=c++23', '-O2', '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00',
    '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
    '-D__USE_MINGW_ANSI_STDIO=0', '-target', 'x86_64-w64-mingw32')
if ($Test) {
    $output = Join-Path $project.Build 'render-tests.exe'
    $arguments = $common + @((Join-Path $project.Root 'render-tests.cpp'), '-o', $output)
} else {
    $output = Join-Path $project.Build $project.Library
    $arguments = $common + @('-shared', '-DWH_MOD', ('-DWH_MOD_ID=L"' + $project.LocalId + '"'),
        ('-DWH_MOD_VERSION=L"' + $project.Version + '"'), (Join-Path $windhawk.Engine '64\windhawk.lib'),
        '-include', 'windhawk_api.h', $project.Source, '-Wl,--export-all-symbols', '-o', $output)
}
$arguments += @('-lgdiplus', '-lcomctl32', '-lgdi32', '-luxtheme', '-lmsimg32')
Push-Location $windhawk.Compiler
try {
    & (Join-Path $windhawk.Compiler 'bin\clang++.exe') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed ($LASTEXITCODE)." }
} finally { Pop-Location }
foreach ($pair in @(@('libc++.dll', 'libc++.whl'), @('libunwind.dll', 'libunwind.whl'),
                     @('windhawk-mod-shim.dll', 'windhawk-mod-shim.dll'))) {
    Copy-Item -LiteralPath (Join-Path $windhawk.Compiler ('x86_64-w64-mingw32\bin\' + $pair[0])) -Destination (Join-Path $project.Build $pair[1]) -Force
}
if ($Test) {
    Push-Location $project.Build
    try {
        & $output
        if ($LASTEXITCODE -ne 0) { throw "Rendering tests failed ($LASTEXITCODE)." }
    } finally { Pop-Location }
} else { Write-Output "Built $output" }
