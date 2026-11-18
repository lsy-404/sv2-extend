# 显式回收 WebKit 上下文

`retire_host_web_contexts` 是独立的版本化 API，回收已观察的网页渲染和 WebKit 网络进程。旧 `retire_host_web_renderers` 继续只承诺渲染进程回收。普通联网开关不会自动执行这两种破坏性操作；它们没有作为 MCP、Lua 或 JS 工具公开。

新函数追加在 `loader_api_v1` 偏移 240，在 0.9.9 追加时 64 位结构长度为 248；后续 SDK 继续在尾部追加字段。旧字段位置不变；调用方同时检查长度、`LOADER_CAP_WEB_CONTEXT_RETIREMENT` 和函数指针。Windows 不公布这两项回收能力，调用返回 `LOADER_ERR_NOT_SUPPORTED`；该平台的普通断网保留原生 controller 和本地页面。

```c
if (api->struct_size >= offsetof(loader_api_v1, retire_host_web_contexts)
        + sizeof(api->retire_host_web_contexts)
    && (api->capabilities & LOADER_CAP_WEB_CONTEXT_RETIREMENT)
    && api->retire_host_web_contexts) {
    /* Main thread, blocked policy, expected generation and exclusive lease. */
}
```

调用者必须在主线程、策略已请求阻断、代次匹配且持有 profile 独占租约时调用，并保持租约到回调返回。立即拒绝的调用不回调；已接受调用在主线程恰好完成一次，回调执行期间仍计入未排空工作。共享租约、错误代次、空回调和并行回收会被拒绝；等待期间策略或租约变化会失败。

Windows 复用已安装的 WebView Runtime，通过宿主内传输门禁阻断网页网络。离线期间仍立即完成原生 environment 和 controller 创建，不关闭宿主持有的 controller；恢复联网继续使用同一页面。账号插件等待 `verify_host_network_quiescence`，无需执行网页回收。

以下为 macOS 的 WebKit 实现和验证范围。Core 弱引用观察 default、named、nonPersistent 以及可用的私有配置 factory，并观察配置赋值和实际 WebView 的 store。它不为追踪延长 ephemeral store 的 cookie 寿命。超过 256 个已观察 store 后拒绝回收确认。无法匹配的原生网络进程、缺少 SPI 或观察失败不会被解释为排空成功。

渲染进程除了 PID 列表还检查 pool 数量；网络进程除了 `_networkingProcessInfo` 还检查各 store 的 `_networkProcessExists`。实际观察过代理已存在、但 PID 列表仍为空的启动窗口。只在代理存在时才读取 `_networkProcessIdentifier`，避免空闲 getter 自己启动网络进程。进程身份包含 UID、PID 和启动时间；回收使用本 UI 进程持有的原生 pool/store 对象，不通过任意 PID 发终止信号。

成功前要求原进程退出、已观察代理及原生列表为空并连续稳定 100ms；最多等待 5 秒。新接口结果包含 `renderersExited` 和 `networkProcessesExited`，但始终保留 `wholeHostNetworkVerified:false`。私有构造路径、所有客户端回调、其他宿主传输和 Web 身份状态不在此回执的完整证明范围内。未清除 WEBKIT/WEBRTC 等未验证位。

回收会丢失页面运行状态，并可能中断在途操作；不会主动删除持久网站数据。测试确认一个持久测试 cookie 能在网络进程重新启动后读取，但不能将它解释为全部 cookie/HTTP 身份缓存已切换或已快照。账号插件仍需自己的身份、事务与回滚验证。

可复现检查：

```sh
python3 loader/macos/run-web-store-registry-test.py
python3 loader/macos/run-web-context-launching-test.py
EXTEND_WEB_CONTEXT_STANDALONE=1 python3 loader/macos/run-web-context-probe.py
python3 loader/macos/run-web-context-probe.py
```

真实 WebKit 对象登记/弱寿命等 11 项检查通过；10 项受控代理检查覆盖无 PID 的启动窗口和未观察进程拒绝，该组不代表实际进程退出。无窗口宿主和真实隔离 SV2 各通过 27 项检查，包含 renderer-only 网络进程仍存活的对照、两类旧 PID 真正退出、待决 JavaScript 一次原生错误、回调执行计数、租约/代次拒绝、持久测试 cookie 保留及新网络进程恢复。旧 renderer/WebRTC 与传输/URLSession/MCP 回归也保留。

SPI 语义参考 [WebKit 的数据存储实现](https://github.com/WebKit/WebKit/blob/main/Source/WebKit/UIProcess/API/Cocoa/WKWebsiteDataStore.mm)。上游代码不能替代本机运行验证；Core 对方法签名做实际检查。
