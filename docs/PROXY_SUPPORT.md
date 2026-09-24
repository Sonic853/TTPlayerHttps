# ttp_https.dll 代理扩展

更新日期：2026-09-25。DLL 文件版本 4.2.0.2，Mbed TLS 4.2.0 / TF-PSA-Crypto 1.2.0。

## 配置与支持范围

播放器沿用“网络连接 → 代理服务器”的现有字段。

| 配置 | 行为 |
|---|---|
| 不使用代理服务器 | 直接连接目标 HTTPS 服务 |
| 使用 Internet Explorer 的代理设置 | 读取 IE/PAC、HTTPS 代理及绕过规则；无法由 DLL 处理的系统策略保留 WinHTTP 回退 |
| 自定义服务器 `proxy.example` 或 `http://proxy.example` | HTTP CONNECT，可使用下面列出的认证 |
| `socks4://proxy.example` | SOCKS4 CONNECT，本机解析 IPv4；用户名作为 USERID，协议没有密码认证 |
| `socks4a://proxy.example` | SOCKS4a CONNECT，代理解析域名 |
| `socks5://proxy.example` / `socks5h://proxy.example` | SOCKS5 CONNECT，支持 IPv4、IPv6、域名；域名交给代理解析 |

端口栏为正数时覆盖地址中的端口；0 表示使用地址中的端口或默认端口（HTTP 80、SOCKS 1080）。
用户名与密码分别填写原输入框。服务器地址不接受嵌入式 `user:password@host`。

### HTTP 代理认证

- Basic：支持代理声明的 UTF-8；无 charset 时使用 ISO-8859-1，无法表示的字符报错。
- Digest：MD5、MD5-sess、SHA-256、SHA-256-sess；支持 auth、auth-int、旧式无 qop、opaque、stale、UTF-8 和 userhash。
- 多个 Proxy-Authenticate 响应头及单行多挑战均可解析；Digest 优先 SHA-256，再 MD5。
- NTLM / Negotiate：通过系统 SSPI 生成令牌，用户名可使用 `DOMAIN\user`；每条连接有独立认证上下文。
- 自动选择优先级：Negotiate、NTLM、Digest、Basic。认证失败不会自动降级为较弱方式。
- 407 的 Content-Length/chunked 正文先读完，再继续认证；连接关闭时重建连接，NTLM 上下文随连接重建。
- 最多八轮认证，共用请求截止时间、取消检查和 64 KiB 响应头限制。

### SOCKS

SOCKS5 支持无需认证及 RFC 1929 用户名／密码认证。认证时用户名和密码各为 1～255 个 UTF-8 字节。
仅实现 HTTPS 所需的 TCP CONNECT，不实现 SOCKS BIND、UDP ASSOCIATE 或 SOCKS GSSAPI。
不支持“TLS 连接到代理本身”的 HTTPS 代理协议；`https://` 代理地址明确报错。

## TLS、凭据与兼容性

代理只负责建立 TCP 隧道，随后目标 HTTPS 的 TLS 1.2/1.3、证书链、名称和有效期验证均由 Mbed TLS 完成。
Proxy-Authorization 仅写入 CONNECT，不会发给歌词服务器，也不会随重定向复制给源站。
凭据只用于用户选定的自定义代理；不会把自定义凭据发给 IE/PAC 返回的其他代理。
IE 自动登录策略仍由原 WinHTTP 控制，DLL 不自行使用当前 Windows 登录身份；此类系统认证回退后受系统 TLS 能力限制。

公开 C 接口升级为 ABI 2，在旧请求末尾追加用户名、密码指针；DLL 同时接受 ABI 1，按旧结构大小读取。
旧 ABI 1 的认证请求仍回退 WinHTTP。新 EXE 配旧 DLL 时按不兼容组件回退，应配套更新。
缺失／损坏 DLL 的原 WinHTTP 回退、仅从 AddIn 加载及响应由 DLL 释放的约定不变。

体积优化参数与固定依赖不变。Digest MD5 使用 XP CryptoAPI，未给 TLS 启用 MD5；NTLM/Negotiate 使用 XP 已有的 secur32。
当前 x86 Release 为 **420,352 字节（410.5 KiB）**，比初始封装增加 29.5 KiB。
SHA-256：`01bd20ca6a2e8efd55a4e057492391b31d6784fb0c1b1e55664781dbba092389`。
XP/Win7 静态导入审计通过：7 个系统 DLL、109 个导入；旧系统播放器仍为 19 个系统 DLL、656 个导入。

## 实测结果

本机、VirtualBox XP 5.1.2600 和 Win7 6.1.7601 各通过 **47/47 项**，最终 DLL 为上面的 SHA-256。
每项同时检查退出码及日志中的协议／证书／认证结果，避免将加载失败误判为正常拒绝。

| 验证内容 | 本机 | XP | Win7 |
|---|---|---|---|
| HTTP CONNECT、Basic、四种 Digest、多认证挑战及 407 重连 | 通过 | 通过 | 通过 |
| NTLM、Negotiate 协商 NTLM；服务端验证挑战响应 | 通过 | 通过 | 通过 |
| SOCKS4、4a、5 及 SOCKS5 密码认证 | 通过 | 通过 | 通过 |
| 代理隧道后的 TLS 1.2／1.3 与证书校验 | 通过 | 通过 | 通过 |
| 错误密码、缺少凭据、不支持的认证／协议明确失败 | 通过 | 通过 | 通过 |
| 未知 CA、错误名称、过期证书拒绝 | 通过 | 通过 | 通过 |
| 开始前及代理握手期间取消 | 通过 | 通过 | 通过 |
| ABI 1 直接请求及旧认证回退 | 通过 | 通过 | 通过 |
| Basic／SHA-256／NTLM 三组各 32 路并发 | 32/32 × 3 | 32/32 × 3 | 32/32 × 3 |
| 五类认证代理访问真实歌词端点，强制 TLS 1.3 | 通过 | 通过 | 通过 |
| 播放器通过 Basic／Digest／SOCKS5 搜索及下载歌词 | 通过 | 通过 | 通过 |
| 无 DLL、损坏、错误 ABI、错误目录时 WinHTTP 回退 | 通过 | 通过 | 通过 |
| HTTP 解析边界、音频插件扫描排除 | 通过 | 通过 | 通过 |

真实服务器使用用户指定的 `https://lyrics.qianqian.plus/api/search/`。
三种播放器集成测试均返回 1 个搜索结果，并下载 1388 个 wchar 的歌词。
测试 HTTPS 源站未收到任何 Proxy-Authorization 请求头。
Negotiate 已实测工作组内的 NTLM 协商；没有域环境，因此 **Kerberos 未实测**。IE/PAC 读取保持原流程，本次未对企业 PAC 与系统自动登录做完整部署验证。

### XP 测试发现并修复的问题

初版 ABI 1 函数表使用了依赖另一张静态表的局部动态初始化。XP 动态加载时，表中的版本字符串和函数指针仍为空，调用导致 `C0000005` 空地址异常。
两张函数表改为 `static constexpr`，保证编译期完整初始化，不依赖该运行时初始化路径。实际 XP 复测 ABI 1 TLS 1.3 请求成功，随后完整 47 项通过。
探针同时增加函数表字段检查及异常定位，避免系统错误报告窗口阻塞测试。

### 本地测试约束与复现

测试代码和日志仅在 `rebuild/tests/lyrics`，不上传、不进入发行包，GitHub Actions 仍关闭测试。
本机测试通过 `run_https_proxy_suite.py host` 执行，虚拟机脚本由同一文件的 `guest-script` 模式生成，使用隔离目录 `C:\proxy-test`。
日志目录为 `proxy-results-final`；每个平台有 47 项结果及详细协议日志。

测试代理仅监听本机回环地址，限定访问测试 TLS 服务及指定歌词域名，使用虚构凭据，不修改系统账户、系统代理或信任库。
NTLM 测试代理会校验挑战响应；测试完成后关闭临时代理及 TLS 服务。

## 规范依据

- [HTTP Digest：RFC 7616](https://www.rfc-editor.org/rfc/rfc7616.html)
- [SOCKS5：RFC 1928](https://www.rfc-editor.org/rfc/rfc1928.html)、[密码认证：RFC 1929](https://www.rfc-editor.org/rfc/rfc1929.html)
- [Windows SSPI InitializeSecurityContext](https://learn.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--ntlm)
- [NTLM 挑战结构](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/801a4681-8809-4be9-ab0d-61dcfe762786)
