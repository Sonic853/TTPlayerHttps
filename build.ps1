[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build'),
    [string]$Generator = 'Visual Studio 18 2026',
    [string[]]$CMakeArguments = @(),
    [switch]$Package,
    [string]$PackageVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'cmake/version.ps1')
$PackageVersion = (Get-HttpsBuildVersion $PackageVersion).Name
function Invoke-CMake([string[]]$Arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & cmake @Arguments 2>&1 | ForEach-Object { $_.ToString() }
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    if ($code -ne 0) { throw "CMake failed ($code): $Arguments" }
}
# Configure and compile this repository alone. Test sources are always disabled
# in this build/release entry point, including when reusing a developer cache.
Invoke-CMake (@('-S',$PSScriptRoot,'-B',$BuildDirectory,'-G',$Generator,'-A','Win32') + $CMakeArguments +
    @("-DTTP_HTTPS_BUILD_VERSION=$PackageVersion",'-DTTP_HTTPS_TEST_SOURCE=', '-DTTP_HTTPS_CODEC_TEST_SOURCE='))
Invoke-CMake @('--build',$BuildDirectory,'--config','Release','--target','ttp_https','--parallel','4')
Assert-HttpsFileVersion (Join-Path $BuildDirectory 'Release/AddIn/ttp_https.dll') $PackageVersion
if ($Package) {
    & (Join-Path $PSScriptRoot 'cmake/package.ps1') -BuildDirectory $BuildDirectory -Version $PackageVersion
}
