# Mbed TLS 4.2.0 / TF-PSA-Crypto 1.2.0 实测记录

日期：2026-09-25（北京时间）。源码、下载哈希、体积配置见 [README](../README.md)。

## 最终 DLL

- 文件：`mbed_tls_min.dll`，x86，338,944 字节（331 KiB）。
- 文件版本：4.2.0.1；接口返回 `Mbed TLS 4.2.0 / TF-PSA-Crypto 1.2.0 / ABI 1`。
- SHA-256：`019b0437642c554466efd79c18107c30438a63fec7a9b9be52eeea5326ba89a9`。
- Windows 子系统版本 5.01；导入审计通过 XP 5.1.2600 与 Win7 6.1.7600 导出清单。
- 只有 4 个系统 DLL、73 个导入；无 VCRUNTIME、UCRT 或 BCrypt 运行时依赖。
- 唯一公开 C 入口为 `mtm_get_api`；TLS 和密码库静态链接在同一个 DLL 中。

## 验证矩阵

测试通过 VirtualBox Guest Additions 在真实运行的 XP 和 Win7 来宾内执行；
不是修改版本号后的宿主机模拟测试。测试文件置于来宾 `C:\mtm-test`，未替换既有播放器安装。

| 项目 | 本机 Windows | XP 5.1.2600 | Win7 6.1.7601 |
|---|---|---|---|
| DLL 加载及 ABI 版本检查 | 通过 | 通过 | 通过 |
| Cloudflare TLS 1.3 / HTTP 200 | 通过 | 通过 | 通过 |
| 用户指定歌词 API TLS 1.3 / HTTP 200 | 通过 | 通过 | 通过 |
| TLS 1.2 独立服务器 | — | 通过 | 通过 |
| TLS 1.3 独立服务器 | 通过 | 通过 | 通过 |
| 非信任 CA 拒绝 | 通过（播放器路径） | 通过 | 通过 |
| 域名不匹配拒绝 | — | 通过 | 通过 |
| 过期证书拒绝 | — | 通过 | 通过 |
| 同进程 32 线程首次初始化及并发 TLS 1.3 | 32/32 | 32/32 | 32/32 |
| 播放器搜索并下载真实歌词 | 通过 | 通过 | 通过 |
| HTTP 解析边界、截断、大小限制及取消检查 | 通过 | 通过 | 通过 |

成功握手均返回 `protocol=0x0304`（TLS 1.3）或 `0x0303`（TLS 1.2），
`verify=0x00000000`。TLS 1.3 覆盖 AES-256-GCM-SHA384、ChaCha20-Poly1305-SHA256；
TLS 1.2 独立服务器强制 ECDHE-RSA-AES128-GCM-SHA256。

## 用户指定歌词服务器

服务器：[https://lyrics.qianqian.plus/api/search/](https://lyrics.qianqian.plus/api/search/)。

仅访问空路径会得到空 `<result>`，因此实测同时发送了原歌词协议搜索参数：

```text
/api/search/?sh?Artist=68547067264F&Title=74662959&Flags=0&
```

这里是“周杰伦／晴天”的 UTF-16LE 十六进制值。返回 HTTP 200，130 字节 XML，
包含一条 `id=198` 的匹配结果。DLL 探针记录 TLS 1.3、证书验证无错误。

随后用 `rebuild/src/lyrics/lyric_http.cpp` 的真实 `SearchHttpLyrics`、
`DownloadHttpLyric` 调用完成搜索和下载。XP、Win7、本机都返回：

```text
PASS: live lyric search 1 result(s), download 1388 wchar(s)
```

这里验证的是播放器同一份传输、XML 解析、下载校验码和 LRC 下载代码。
本次没有额外做歌词搜索窗口的手工点击演示；原 DLL 私有歌词插件的内部 HTTP 实现不在替换范围内。

## 测试中修复的问题

1. **4.x 线程 API 不兼容**：TF-PSA-Crypto 1.2.0 的 ALT 层需要 9 个互斥锁／条件变量回调。
   旧四回调代码不能编译；已完整适配，并通过 XP 上的条件变量兼容实现实测。
2. **默认 PSA 密钥槽不足**：32 路同时握手最初在 XP 失败 30 路、Win7 失败 19 路，
   出现握手协商失败及 PSA 内存不足。启用 `MBEDTLS_PSA_KEY_STORE_DYNAMIC` 后均为 32/32，
   DLL 仅增加 512 字节。
3. **无 TLS 关闭通知的 EOF**：上游 `mbedtls_ssl_read` 将连接 EOF 返回为 0。
   封装区分未通知关闭与 `close_notify`，HTTP 层仅在已收到完整 Content-Length／chunked 内容时接受已完成响应。
4. **根证书文本体积**：188,900 字节 PEM 转换为 DER 内嵌，不丢弃任何根证书；
   从 397,824 字节降至 338,432 字节，再加动态密钥存储后为最终 338,944 字节。

## 本地复现

本地测试源码都位于 `rebuild/tests/lyrics`，测试子模块不上传，发行包不包含测试程序。
Actions 未新增运行测试步骤。

- `mbed_tls_min_probe.c`：DLL ABI、固定 TLS 版本、私有 CA、32 线程测试。
- `mtm_tls_fixture.py`：仅绑定宿主机回环地址的 TLS 1.2、TLS 1.3、过期证书服务；
  VirtualBox NAT 通过 `10.0.2.2` 访问。私有 CA 只显式传给探针，不安装到证书库。
- `portable_https_contract_tests.cpp`：生产 HTTP 解析器使用碎片化输入验证响应边界。
- `service_catalog_tests.cpp --live-lyrics <URL>`：真实搜索／下载链路。
- `service_catalog_tests.cpp --reject-tls <URL>`：播放器 transport 验证私有 CA 被拒绝。

`MTM_TEST_SOURCE` 默认为空。仅在本地需要时传入探针路径，并手动构建 `mbed_tls_min_probe`。

原始日志保存在本地 `mbedtlsmin/build` 和 `rebuild/tests/lyrics/mtm-results`，不加入发行包。

## 范围与限制

- 本次真实网络验证为直连；未声称已验证需要 NTLM／Negotiate／Digest 的代理。
- 这些原生代理认证场景继续使用 WinHTTP，因此 XP 的该路径仍受系统 TLS 限制。
- 传输等待阶段支持取消及 65 秒总期限检查；同步系统 DNS／PAC 调用还受系统自身超时约束。
- 证书库固定于 2026-08-13，不执行在线吊销查询；维护时需要更新证书包及校验哈希。
- 本机与来宾日期必须正确，否则证书有效期校验会按实际错误日期拒绝连接。
