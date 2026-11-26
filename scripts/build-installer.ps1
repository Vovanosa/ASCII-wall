param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.3'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$scriptPath = Join-Path $root 'installer\AsciiWall.iss'
$outputDir = Join-Path $root 'build\installer'
$outputPath = Join-Path $outputDir "AsciiWall-Setup-$Version.exe"

$compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($compiler) {
    $compilerPath = $compiler.Source
} else {
    $compilerCandidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    $compilerPath = $compilerCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}

if (-not $compilerPath) {
    throw 'Inno Setup 6 is required to build the installer. Install it from https://jrsoftware.org/isinfo.php.'
}
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
    throw "Installer script is missing: $scriptPath"
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
& $compilerPath "/DAppVersion=$Version" $scriptPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "Inno Setup did not create the expected installer: $outputPath"
}

$hash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path -Leaf $outputPath)" |
    Set-Content -LiteralPath "$outputPath.sha256" -Encoding ASCII

Write-Output "Created $outputPath"
Write-Output "SHA-256: $hash"
