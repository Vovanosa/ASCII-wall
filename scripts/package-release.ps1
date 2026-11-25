param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
$build = Join-Path $root 'build'
$stage = Join-Path $build 'package-stage'
$archiveName = "AsciiWall-$Version-windows-x64.zip"
$archive = Join-Path $build $archiveName

$requiredFiles = @(
    'asciiwall.exe',
    'settings.ini',
    'README.txt',
    'THIRD_PARTY_NOTICES.txt'
)
$license = Join-Path $root 'LICENSE'

foreach ($file in $requiredFiles) {
    $path = Join-Path $dist $file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required release file is missing: $path"
    }
}
if (-not (Test-Path -LiteralPath $license -PathType Leaf)) {
    throw "Required project license is missing: $license"
}

$scene = Join-Path $dist 'scenes\campfire.lua'
if (-not (Test-Path -LiteralPath $scene -PathType Leaf)) {
    throw "Required release scene is missing: $scene"
}

if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $stage 'scenes') -Force | Out-Null

foreach ($file in $requiredFiles) {
    Copy-Item -LiteralPath (Join-Path $dist $file) -Destination $stage
}
Copy-Item -LiteralPath $license -Destination $stage
Copy-Item -LiteralPath $scene -Destination (Join-Path $stage 'scenes')

$shell = New-Object -ComObject WScript.Shell
$shortcutPath = Join-Path $stage 'AsciiWall Settings.lnk'
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = Join-Path $stage 'asciiwall.exe'
$shortcut.Arguments = '--settings'
$shortcut.Description = 'Open the AsciiWall visual settings'
$shortcut.RelativePath = $shortcutPath
$shortcut.Save()

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
$zip = [System.IO.Compression.ZipFile]::Open(
    $archive,
    [System.IO.Compression.ZipArchiveMode]::Create
)
try {
    foreach ($file in $requiredFiles) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            (Join-Path $stage $file),
            $file,
            [System.IO.Compression.CompressionLevel]::Optimal
        ) | Out-Null
    }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip,
        (Join-Path $stage 'LICENSE'),
        'LICENSE',
        [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip,
        (Join-Path $stage 'scenes\campfire.lua'),
        'scenes/campfire.lua',
        [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip,
        $shortcutPath,
        'AsciiWall Settings.lnk',
        [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
}
finally {
    $zip.Dispose()
}

$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $archiveName" | Set-Content -LiteralPath "$archive.sha256" -Encoding ASCII

Remove-Item -LiteralPath $stage -Recurse -Force
Write-Output "Created $archive"
Write-Output "SHA-256: $hash"
