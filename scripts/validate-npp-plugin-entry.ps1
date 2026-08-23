[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$EntryPath,

    [ValidateSet('32', '64', 'arm64')]
    [string]$Architecture = '64',

    [switch]$RequireDirectHttpsZip,

    [string]$SchemaUrl = 'https://raw.githubusercontent.com/notepad-plus-plus/nppPluginList/master/pl.schema',

    [string]$Python = 'python'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedEntry = (Resolve-Path -LiteralPath $EntryPath).Path
$entry = Get-Content -LiteralPath $resolvedEntry -Raw | ConvertFrom-Json

$required = @(
    'folder-name',
    'display-name',
    'version',
    'id',
    'repository',
    'description',
    'author',
    'homepage'
)

foreach ($name in $required) {
    if ($entry.PSObject.Properties.Name -notcontains $name) {
        throw "Entry is missing required field '$name'."
    }
}

if ($entry.'folder-name' -ne 'NppAIAssistant') {
    throw "folder-name must be NppAIAssistant for this project. Got '$($entry.'folder-name')'."
}

if ($entry.version -notmatch '^\d+(\.\d+){0,3}$') {
    throw "Invalid plugin version '$($entry.version)'."
}

if ($entry.id -notmatch '^[0-9A-Fa-f]{64}$') {
    throw 'id must be a 64-hex-character SHA-256 value.'
}

$repositoryUri = $null
if (-not [System.Uri]::TryCreate($entry.repository, [System.UriKind]::Absolute, [ref]$repositoryUri)) {
    throw "repository is not an absolute URI: '$($entry.repository)'."
}

if ($RequireDirectHttpsZip) {
    if ($repositoryUri.Scheme -ne 'https') {
        throw "repository must use HTTPS for a release submission. Got '$($repositoryUri.Scheme)'."
    }
    if (-not $repositoryUri.AbsolutePath.EndsWith('.zip', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "repository must point directly to a .zip release asset. Got '$($entry.repository)'."
    }
}

$pythonCommand = Get-Command $Python -ErrorAction SilentlyContinue
if (-not $pythonCommand) {
    throw "Python command '$Python' was not found."
}

& $Python -c 'import jsonschema' 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Python package 'jsonschema' is required. Install it with: $Python -m pip install jsonschema"
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("npp-plugin-schema-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

$schemaPath = Join-Path $tempRoot 'pl.schema'
$documentPath = Join-Path $tempRoot 'candidate.json'

try {
    Invoke-WebRequest -Uri $SchemaUrl -OutFile $schemaPath -MaximumRedirection 5

    $schemaSha256 = (Get-FileHash -LiteralPath $schemaPath -Algorithm SHA256).Hash.ToUpperInvariant()

    $document = [ordered]@{
        name = 'npp-pluginList'
        version = '0.0.0'
        arch = $Architecture
        'npp-plugins' = @($entry)
    }
    $document | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $documentPath -Encoding utf8

    $pythonCode = @'
import json
import sys
from jsonschema import Draft202012Validator, FormatChecker

schema_path, document_path = sys.argv[1], sys.argv[2]
with open(schema_path, encoding='utf-8') as f:
    schema = json.load(f)
with open(document_path, encoding='utf-8-sig') as f:
    document = json.load(f)

validator = Draft202012Validator(schema, format_checker=FormatChecker())
errors = sorted(validator.iter_errors(document), key=lambda e: list(e.absolute_path))
if errors:
    for error in errors:
        path = '/'.join(str(part) for part in error.absolute_path)
        print(f"SCHEMA_FAIL {path}: {error.message}", file=sys.stderr)
    raise SystemExit(2)

print('SCHEMA_PASS')
'@

    & $Python -c $pythonCode $schemaPath $documentPath
    if ($LASTEXITCODE -ne 0) {
        throw "Official nppPluginList schema validation failed with exit code $LASTEXITCODE."
    }

    [pscustomobject]@{
        EntryPath = $resolvedEntry
        Architecture = $Architecture
        Version = $entry.version
        Repository = $entry.repository
        Id = $entry.id
        RequireDirectHttpsZip = [bool]$RequireDirectHttpsZip
        SchemaUrl = $SchemaUrl
        SchemaSha256 = $schemaSha256
        Result = 'PASS'
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
