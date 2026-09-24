[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$Version
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')
$version = (Get-HttpsBuildVersion $Version).Name
$output = Join-Path ([IO.Path]::GetFullPath($BuildDirectory)) 'Release'
$dll = Join-Path $output 'AddIn/ttp_https.dll'
Assert-HttpsFileVersion $dll $version

# Construct the archive from two explicit entries, not a build/staging glob.
# Existing docs, tests, libraries or previous ZIPs cannot leak into this package.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = Join-Path $output "ttp_https-$version.zip"
$temporary = Join-Path $output ('package-' + [guid]::NewGuid().ToString('N') + '.tmp')
$hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = "$hash  AddIn/ttp_https.dll`n"
if ((Get-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding UTF8 -Raw).Trim() -cne $manifest.Trim()) {
    throw 'Input HTTPS DLL checksum does not match its build manifest.'
}
$stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew)
try {
    $zip = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $dll, 'AddIn/ttp_https.dll',
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        $entry = $zip.CreateEntry('SHA256SUMS.txt', [IO.Compression.CompressionLevel]::Optimal)
        $writer = New-Object IO.StreamWriter($entry.Open(), (New-Object Text.UTF8Encoding($false)))
        try { $writer.Write($manifest) } finally { $writer.Dispose() }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
Move-Item -LiteralPath $temporary -Destination $archive -Force
# The manifest beside AddIn remains a DLL checksum; this second manifest is for
# the release ZIP asset and is not placed inside it.
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'PACKAGE_SHA256SUMS.txt'),
    "$archiveHash  ttp_https-$version.zip`n", (New-Object Text.UTF8Encoding($false)))
& (Join-Path $PSScriptRoot 'verify_package.ps1') -Archive $archive -Version $version
Write-Output "HTTPS package: $archive"
