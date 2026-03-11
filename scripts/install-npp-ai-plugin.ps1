param(
    [string]$NotepadRoot = "C:\Program Files\Notepad++",
    [string]$PluginBuildOutput = "Q:\Projects\NppAIAssistant\build\x64\Release\plugins\NppAIAssistant\NppAIAssistant.dll"
)

$ErrorActionPreference = "Stop"

$pluginDir = Join-Path $NotepadRoot "plugins\NppAIAssistant"
$pluginDll = Join-Path $pluginDir "NppAIAssistant.dll"

if (-not (Test-Path $PluginBuildOutput)) {
    throw "Plugin DLL not found: $PluginBuildOutput"
}

if (-not (Test-Path (Join-Path $NotepadRoot "notepad++.exe"))) {
    throw "Notepad++ not found at: $NotepadRoot"
}

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
Copy-Item -Path $PluginBuildOutput -Destination $pluginDll -Force

Get-Item $pluginDll | Select-Object FullName, Length, LastWriteTime
