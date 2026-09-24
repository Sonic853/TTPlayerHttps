[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Archive,
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$ArchiveManifest
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')
$version = (Get-HttpsBuildVersion $Version).Name
$archive = [IO.Path]::GetFullPath($Archive)
if ([IO.Path]::GetFileName($archive) -cne "ttp_https-$version.zip") { throw 'Unexpected HTTPS package name.' }
if ($ArchiveManifest) {
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    $expected = (Get-Content -LiteralPath $ArchiveManifest -Raw -Encoding UTF8).Trim()
    if ($expected -cne "$hash  ttp_https-$version.zip") { throw 'HTTPS archive SHA-256 mismatch.' }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName } | Sort-Object)
    if ($names.Count -ne 2 -or $names[0] -cne 'AddIn/ttp_https.dll' -or $names[1] -cne 'SHA256SUMS.txt') {
        throw 'HTTPS ZIP must contain exactly AddIn/ttp_https.dll and SHA256SUMS.txt.'
    }
    $reader = New-Object IO.StreamReader($zip.GetEntry('SHA256SUMS.txt').Open())
    try { $manifest = $reader.ReadToEnd().Trim() } finally { $reader.Dispose() }
    $sha = [Security.Cryptography.SHA256]::Create()
    $dll = $zip.GetEntry('AddIn/ttp_https.dll').Open()
    try { $hash = ([BitConverter]::ToString($sha.ComputeHash($dll))).Replace('-','').ToLowerInvariant() }
    finally { $dll.Dispose(); $sha.Dispose() }
    if ($manifest -cne "$hash  AddIn/ttp_https.dll") { throw 'HTTPS DLL checksum mismatch inside ZIP.' }
} finally { $zip.Dispose() }
Write-Output "Verified HTTPS package $version (two files, SHA-256)."
