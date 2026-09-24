# 独立构建与 GitHub Actions

工作流：`.github/workflows/build.yml`，界面名称 **Build HTTPS plugin**。
源码仓库为 `Sonic853/TTPlayerHttps`，即本地的 `mbedtlsmin` 目录。

## 使用

1. 将本仓库的工作流和源码提交到 GitHub。
2. 打开 Actions → Build HTTPS plugin → Run workflow，选择分支。
3. 不勾选 Release a Version：只生成可下载的构建产物。
4. 勾选 Release a Version (GitHub)：构建成功后创建日期标签和 GitHub Release。

构建产物名为 `ttp_https-版本号`；Release 附件为 `ttp_https-版本号.zip`。
两者解压后均只有：

```text
AddIn/ttp_https.dll
SHA256SUMS.txt
```

ZIP 内的 SHA256SUMS.txt 校验 DLL。构建本地另生成 PACKAGE_SHA256SUMS.txt 校验 ZIP；该文件不放入发行 ZIP，也不作为额外 Release 附件上传。
不附加源码包、许可证包、README、PDB、LIB、测试文件或构建日志。源码、版权声明、完整许可证、原始 CA PEM 和文档均保留在本仓库；Release 正文链接到本次构建提交中的对应文件。

## 版本规则

与播放器和 AAC 插件采用相同规则：北京时间 `yyyy.MM.dd`。
正式发布前读取本仓库标签和已有 Release（包括草稿），当日版本已存在时选择下一号 `p1`、`p2`……。
发布运行使用同一并发组串行处理，版本占号保持到发布结束；不会覆盖已有 Release。

版本在编译前固定，并同时写入 DLL 的 FileVersion、ProductVersion 和 Windows 四段数字版本。
例如 `2026.09.25p3` 对应数字版本 `2026.9.25.3`，补丁号最多 65535。
打包和发布时核对 DLL 文件版本、ZIP 名称、文件白名单及 SHA-256。
Mbed TLS 4.2.0、TF-PSA-Crypto 1.2.0 和公开 C ABI 2 不随日期文件版本改变。

## 构建边界

- 仅 checkout 本仓库，构建目标为 `ttp_https`，不 checkout／调用 rebuild 或 ttp_aac。
- 使用 VS 2026 的 Windows x86 Release，沿用体积优先选项和 XP 运行库适配。
- 构建时下载固定版本、固定 SHA-256 的 Mbed TLS、VC-LTL、YY-Thunks 和 CA 数据。
- 构建后用本仓库的 PE 导入检查脚本与 YY-Thunks XP／Win7 导出清单核对，不运行 DLL 或测试程序。
- `build.ps1` 显式清空两个测试源码开关，即使复用曾经启用测试的 CMake 缓存，也不会构建测试目标。
- 播放器继续独立构建；需要使用本 DLL 时接收预编译文件并放入 AddIn。

构建 job 只有 `contents: read`；发布 job 才获得 `contents: write`。默认不勾选发布，不会创建远端标签或 Release。

## 本地复现

```powershell
./build.ps1 -Package
./build.ps1 -Package -PackageVersion 2026.09.25p1
```

输出位于 `build/Release`。只需要安装运行文件时，也可在新暂存目录执行：

```powershell
cmake --install build --config Release --component Runtime --prefix out/runtime
```

该组件现在只安装 DLL 和其 SHA256SUMS.txt；安装到历史暂存目录时，CMake 不会自动删除以前留下的文档，因此发行 ZIP 直接按两项白名单创建，不对目录整体压缩。

## 本地验证记录（2026-09-25）

- actionlint 1.7.12：工作流语法与表达式检查通过。
- 7 个 PowerShell 脚本语法检查通过；实际日期构建、资源版本和打包检查通过。
- 将本仓库文件单独导出到新目录，重新下载固定依赖并完整构建成功；未使用相邻仓库源码或测试。
- 新 DLL 的 XP／Win7 PE 导入检查通过：7 个系统 DLL、109 个导入。
- 同日标签及草稿 Release 占号、无历史版本、非法日期、补丁上限、额外 ZIP 文件及错误校验和拒绝通过本地检查。
- 本地检查代码仅保存在 `rebuild/tests/https-actions`，不属于本仓库，也不在 Actions 执行。
- 本次仅创建及本地验证工作流；尚未在 GitHub 托管 runner 上触发运行或发布。

运行环境参考：[GitHub Windows runner 列表](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)。
