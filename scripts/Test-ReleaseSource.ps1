[CmdletBinding()]
param([string]$BuildDir = 'build/release-validation')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    & cmake -S . -B $BuildDir -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $BuildDir --config Release --parallel 4 -- /p:VcpkgEnabled=false
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
    & ctest --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Release tests failed.' }
    & "$PSScriptRoot/verify-security-regressions.ps1"
    $dll = Join-Path $BuildDir 'plugins/NppAIAssistant/Release/NppAIAssistant.dll'
    & python tests/verify-composer-resource.py $dll
    if ($LASTEXITCODE -ne 0) { throw 'Compiled resource check failed.' }
    $vswhereCandidates = @(
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe' }),
        $(if (${env:ProgramFiles}) { Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio/Installer/vswhere.exe' }),
        'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
    $vswhere = $vswhereCandidates | Select-Object -First 1
    if (-not $vswhere) { throw 'vswhere.exe not found.' }
    $dumpbin = @(& $vswhere -latest -products '*' -find 'VC/Tools/MSVC/**/bin/Hostx64/x64/dumpbin.exe') |
        Where-Object { $_ -and (Test-Path -LiteralPath $_) } |
        Select-Object -First 1
    if (-not $dumpbin) { throw 'dumpbin not found.' }
    & "$PSScriptRoot/Test-NppPluginBinary.ps1" -DllPath $dll -DumpbinPath $dumpbin -Platform x64
} finally { Pop-Location }
