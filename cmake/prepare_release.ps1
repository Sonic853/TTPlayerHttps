[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$PreviousVersion,
    [Parameter(Mandatory = $true)][string]$Repository,
    [Parameter(Mandatory = $true)][string]$Commit,
    [string]$OutputDirectory = 'build/Release',
    [string]$ServerUrl = 'https://github.com'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')
$version = (Get-HttpsBuildVersion $Version).Name
if ($PreviousVersion -and -not (Read-HttpsVersionTag $PreviousVersion)) { throw 'Invalid previous release version.' }
if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $Commit -notmatch '^[0-9a-fA-F]{40}$') {
    throw 'Missing or invalid repository/commit.'
}
& (Join-Path $PSScriptRoot 'verify_package.ps1') -Archive (Join-Path $OutputDirectory "ttp_https-$version.zip") `
    -Version $version -ArchiveManifest (Join-Path $OutputDirectory 'PACKAGE_SHA256SUMS.txt')
$repoUrl = "$ServerUrl/$Repository"
$logUrl = if ($PreviousVersion) { "$repoUrl/compare/$PreviousVersion...$version" } else { "$repoUrl/commits/$version" }
$notes = @"
**完整更新日志**：$logUrl

下载并解压 ttp_https-$version.zip，将 AddIn/ttp_https.dll 放入播放器安装目录，重启播放器后生效。
同一 x86 DLL 支持 Windows XP SP3、Windows 7 及现代 Windows。
ZIP 仅包含 AddIn/ttp_https.dll 和 SHA256SUMS.txt。
HTTPS 插件与播放器分别构建；此包不包含播放器程序。
TLS 库版本保持 Mbed TLS 4.2.0 / TF-PSA-Crypto 1.2.0，文件版本与发行标签一致。

[本次构建的源码和构建说明]($repoUrl/tree/$Commit)
[第三方版权声明]($repoUrl/blob/$Commit/THIRD_PARTY_NOTICES.md)
[完整许可证文件]($repoUrl/tree/$Commit/licenses)
[代理配置与支持范围]($repoUrl/blob/$Commit/docs/PROXY_SUPPORT.md)
"@
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'release-notes.md'), $notes, (New-Object Text.UTF8Encoding($false)))
Write-Output "Prepared HTTPS release $version"
