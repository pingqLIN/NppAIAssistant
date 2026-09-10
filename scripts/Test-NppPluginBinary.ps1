[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DllPath,
    [Parameter(Mandatory)][string]$DumpbinPath,
    [ValidateSet('x86', 'x64', 'arm64')][string]$Platform = 'x64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$dll = Get-Item -LiteralPath $DllPath
if ($dll.Name -ne 'NppAIAssistant.dll') { throw 'Plugin DLL basename changed.' }
$headers = (& $DumpbinPath /headers $dll.FullName) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect PE headers.' }
$exports = (& $DumpbinPath /exports $dll.FullName) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect DLL exports.' }
$dependencies = (& $DumpbinPath /dependents $dll.FullName) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect DLL dependencies.' }
$machines = @{ x86 = '14C'; x64 = '8664'; arm64 = 'AA64' }
if ($headers -notmatch "(?im)^\s*$($machines[$Platform]) machine") {
    throw "PE machine does not match $Platform."
}
foreach ($name in @('setInfo', 'getName', 'getFuncsArray', 'beNotified', 'messageProc', 'isUnicode')) {
    if ($exports -notmatch "(?m)\s$([regex]::Escape($name))\s*$") {
        throw "Missing exact Notepad++ ABI export: $name"
    }
}
foreach ($flag in @('Dynamic base', 'NX compatible')) {
    if ($headers -notmatch [regex]::Escape($flag)) { throw "Missing PE mitigation: $flag" }
}
if ($dependencies -match '(?i)(VCRUNTIME|MSVCP|ucrtbase|api-ms-win-crt)[^\s]*\.dll') {
    throw 'Plugin requires a separately deployed Visual C++ runtime.'
}
$imports = @([regex]::Matches($dependencies, '(?im)^\s+([a-z0-9_.-]+\.dll)\s*$') |
    ForEach-Object { $_.Groups[1].Value })
[pscustomobject]@{
    Platform = $Platform
    Version = $dll.VersionInfo.FileVersion
    Sha256 = (Get-FileHash -LiteralPath $dll.FullName -Algorithm SHA256).Hash
    ExactExports = 'PASS'
    AslrAndNx = 'PASS'
    ExternalVcRuntimeRequired = $false
    Imports = $imports
    HostLoadTest = 'NOT RUN: static PE inspection only'
}
