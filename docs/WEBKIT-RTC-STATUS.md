# WebKit WebRTC 联网控制

HTTP/HTTPS 的 SOCKS 代理和内容规则不能证明 WebRTC 的 UDP 路径受控。实际隔离宿主探测中，阻断期间向独立本地 UDP 接收端观察到了新的 STUN 请求；因此 WebKit 和新增 `LOADER_NET_WEBRTC` 均保留未验证标志，完整隔离 receipt 不放行。

当前接入 WebKit 原生 `_peerConnectionEnabled` 偏好，并检查运行时方法签名。离线时禁止新建 RTCPeerConnection，包含页面预先缓存的构造函数；调用者在离线时试图重新启用该偏好仍保持禁用。联网时恢复调用者原先要求的偏好；原先关闭的值不会被强制开启。

这项私有 SPI 不提供已有连接撤销。实际接收端仍观察到旧连接在阻断后的 STUN 重试。离线创建文档时 WebKit 不暴露 WebRTC 接口；恢复偏好之后，需要新导航/新文档才能重新暴露该接口。Loader 不自动重载页面，也不假装已使旧页面实时连接安全排空。

早期单视图终止实验曾出现 `callAsyncJavaScript` 回调未返回；该失败记录保留，不能作为可靠撤销证明。后续改为观察本宿主的全部 WebKit process pool，并增加了下述显式回收接口。普通联网开关不会自动调用它。

## 验证

```sh
python3 loader/macos/run-webkit-rtc-preferences-test.py
LOADER_WEBRTC_STANDALONE=1 LOADER_WEBRTC_EXPECT_GUARD=1 python3 loader/macos/run-webkit-realtime-probe.py
LOADER_WEBRTC_EXPECT_GUARD=1 python3 loader/macos/run-webkit-realtime-probe.py
```

第一项检查真实 WebKit 偏好值、调用者覆盖、重复切换、恢复和独立配置。第二项使用无窗口测试宿主和完整候选 Core；第三项创建独立 SV2 托管宿主及数据目录，不修改官方安装。探测不申请摄像头/麦克风，只向本地 STUN 观察端发送候选收集请求。

四阶段为：在线基线、离线旧页面的缓存构造函数、离线新页面、恢复联网后的新文档。实际 SV2 观察的 STUN 数量为 6/0/0/4；旧连接在离线窗口仍有重试。这个结果证明新连接控制，明确不证明已有连接撤销或整宿主隔离。已有 35 项传输矩阵和 34 项 URLSession 回调回归也通过，但不能替代该缺口。

参考原生语义：[WKPreferences 私有声明](https://github.com/WebKit/WebKit/blob/main/Source/WebKit/UIProcess/API/Cocoa/WKPreferencesPrivate.h)、[RTCPeerConnection 创建检查](https://github.com/WebKit/WebKit/blob/main/Source/WebCore/Modules/mediastream/RTCPeerConnection.cpp)。运行验证针对本机版本，不能把上游源码直接当成本机所有路径的证明。


## 显式回收 API（已安装 0.9.4）

版本化 SDK 在尾部追加 `retire_host_web_renderers(expected_generation, complete, context)`，通过 `LOADER_CAP_WEB_RENDERER_RETIREMENT` 和结构长度检查可用性。仅接受主线程调用、当前代次的阻断策略和本进程持有的独占 profile lease；调用者必须持有 lease 直到完成。它终止本宿主 WebKit 网页上下文，丢失页面运行状态，保留持久网站数据。它没有暴露给普通 MCP/Lua/JS 工具。

成功前同时检查：原先观察到的进程身份已退出，所有池的进程数量为零，并连续稳定为空 100ms。进程身份包含 PID、用户和启动时间，避免 PID 复用误判；没有 PID 的启动中进程也必须排空。最多等待 5 秒。策略代次或 lease 改变会异步失败。立即拒绝的调用不回调；已接受的调用在主线程恰好回调一次，回调执行期间仍计入未排空工作。

实际 SV2 测试中回收了两个网页进程，旧 PID 全部退出；独立 UDP 端观察到四阶段 STUN 为 4/0/0/4，阻断后旧连接数据为零。提前提交且保持待决的 JavaScript 请求在回收后收到一次 WebKit 原生错误 5，位于主线程；不终止的对照保持待决。15 项接口拒绝/代次/lease 检查通过；另有 6 项启动中进程观察检查。35 项传输矩阵与 34 项 URLSession 回调回归通过。

```sh
LOADER_WEBRTC_CORE_RETIRE=1 python3 loader/macos/run-webkit-realtime-probe.py
python3 loader/macos/run-web-retirement-launching-test.py
python3 loader/macos/run-policy-transport-regression.py
```

这是显式网页进程回收的证据，尚未覆盖每种 WebKit 原生请求回调或其他宿主网络路径。成功结果仍携带 `wholeHostNetworkVerified:false`；WEBKIT/WEBRTC 未验证位保留。
