# 网络控制验证状态

## Windows 受管宿主

官方 Windows 2.3.0 受管宿主支持运行时阻断与恢复联网。策略状态通过 `get_host_network_policy` 区分请求、已应用、待处理和错误；`get_host_network_state` 与 `verify_host_network_quiescence` 另行报告排空和验证结果。已在真实宿主验证 WebView 环境及 controller 回调、原生 Close、受管 Job 中无其他成员、当前宿主 IP 活动连接为零，并在阻断策略生效后读回 `whole_host_verified=1`；恢复联网后可重新在线运行。

该验证适用于官方 2.3.0 已支持的导入入口、受管 Job 及完整匹配的宿主防火墙规则。存在未知子进程、待决 WebView 回调、未关闭连接或规则不匹配时不能将其视为已验证；不覆盖其他宿主或任意原生插件的全部联网行为。

## macOS 覆盖

当前 arm64 Swift NWConnectionGroup 已接入统一策略：

- 在 Swift 构造入口弱引用登记对象并记录创建时 socket scope。
- 阻断撤销宿主旧对象及阻断期间新建的宿主对象。
- 已撤销对象的 Swift start 不再进入 SDK；恢复联网不能复活旧组。
- 插件 scope 不计入宿主对象数；仍持有的宿主对象保守地算作未排空，直到释放。
- 构造和 start 转接只在 arm64 构建启用。x86_64 编译成功不代表具备同等覆盖。

真实 SV2 内加载诊断插件已观察到阻断前和恢复后的新组实际 UDP 收发；独立接收端未收到旧组发包；阻断期间创建的宿主组未就绪；插件 scope 对象未计入宿主数，恢复联网后可实际收发；测试对象释放后计数归零。插件 scope 在阻断期间的异步收发不在本次验证范围内。

SDK 对启动前已取消的组可能保持 setup 且不回调；直接使用官方 API 的独立对照呈现相同行为。对已撤销对象再次调用 SDK start 的测试曾观察到 wrapper 长期存活；现由 start 接入拒绝该路径。没有伪造取消事件。

## 未完成的覆盖

普通 Swift NWConnection 的 host/port 与 endpoint 构造、start/restart 已接入并通过真实宿主 15 项检查：旧活跃连接发送被取消、旧未启动连接及阻断期间新建连接不复活、原队列恰好一次错误回调、正常新连接收发及释放计数；Unix endpoint 仅验证不计入宿主数，未验证实际本地收发。

其他构造入口/跨模块调用、活跃对象回调排空及并发竞态仍需验证和实现。正向 message-context extraction、QUIC/multiplex、系统后台及私有路径仍未充分验证。

LOADER_NET_SWIFT_NETWORK 和 LOADER_NET_GROUP_MULTIPLEX 未验证标志保持设置；whole_host_verified 保持 false。

## 重跑

在当前工作区先运行 `sh projects/sv2-extend/loader/macos/build.sh`，再设置 LOADER_GROUP_CORE 为候选 loader-probe.dylib 的绝对路径并运行 `python3 projects/sv2-extend/loader/macos/run-swift-network-probe.py`。驱动创建新的实验 profile 和空白宿主副本，使用安装版启动器搭载候选核心。未完成观测会保留同一 PID 待检查；完成后终止自己的测试宿主。

完整本机证据保存在工作区 outputs；不将宿主副本、运行日志、账号或原始测试数据纳入公开仓库。

普通连接重跑使用同一 LOADER_GROUP_CORE，运行 `python3 projects/sv2-extend/loader/macos/run-swift-connection-probe.py`。

## 并发策略（已安装 0.9.2）

Swift start/restart 使用覆盖实际 SDK 调用的接入 lease，避免许可检查与阻断撤销之间的竞态。未经过已知构造入口的对象会在 start 时登记；阻断期间首次启动的宿主对象保持撤销，恢复联网不会使它复活。插件/控制 scope 和 Unix endpoint 保留原有豁免。

整个联网策略切换被串行化，覆盖 socket、Network/Swift、WebKit 和 NSURLSession 的更新及取消。其他线程不能取得中间状态；SDK 同线程回调重入修改或读取中间快照会返回 INVALID。任务表锁不跨越 NSURLSession cancel 调用。

12项真实 SDK 对象/并发线程测试、9项策略协调测试及真实宿主35项传输矩阵、34项URLSession回调回归通过。这些证据不覆盖全部未知传输，也不允许将 whole_host_verified 设为 true。

## WebKit RTC（已安装 0.9.3）

已确认 WebRTC/STUN 可以绕过 HTTP 代理。新建连接控制已接入并通过实际 SV2 验证，但旧连接仍会在阻断后重试。WEBKIT 和新增 WEBRTC 未验证位均保持设置；详见 [真实反例与验证范围](WEBKIT-RTC-STATUS.md)。原生偏好恢复不重新暴露离线创建文档的接口，需要新导航；不自动重载或永久禁用 WebRTC。

0.9.4 新增显式网页进程回收 API：独占 lease、策略代次绑定、进程退出与池稳定为空后回调；实际宿主验证通过。它不是普通离线切换，也不构成全宿主隔离证明，未验证位保持。详见上述验证范围。

## 正常联网启动期（已安装 0.9.6）

网络观察现在始终在可执行程序原生初始化列表之前安装。先前只有恢复 guard 要求阻断时才提前安装；普通联网启动会遗漏构造函数已经 resume 的 NSURLSession 请求与 completion。独立可执行程序配合真实本地 HTTP 观察端重现了遗漏，修复后 7 项检查通过：main 前登记请求、保持原生响应、执行中 completion 计数非零、回调返回前不报告排空。35 项真实宿主传输矩阵、34 项 URLSession 回调以及 15 项 Swift connection 回归通过。重跑 `python3 loader/macos/run-premain-urlsession-test.py`。它不覆盖 Objective-C +load 和全部依赖库构造阶段；所有剩余未验证位保留。

## Swift 回调保留窗口（0.9.6 实测）

`python3 loader/macos/run-swift-callback-probe.py` 在真实隔离 SV2 中通过 27 项检查：UDP send completion、receiveMessage completion，以及暂停原始回调队列后排队的 send completion。测试在回调挂起期间经统一 `network_gate_set` 阻断，再取消连接并释放应用侧唯一强引用；这些窗口内 SDK 仍保留 Swift wrapper，弱引用非空，outstanding 保持非零，公开 `tracked_tasks_drained` 保持 false。回调在原队列恰好一次执行，返回后计数归零；独立 UDP 对端观察实际请求/响应。

这验证了这些特定 SDK 回调的引用寿命。排队前已提交的发送可能已有成功原生完成结果；测试不将它改成伪造失败，也不拿完成成功当作阻断后新发送的许可。尚未覆盖所有 state/path/viability 回调、其他 Swift 构造入口、组/QUIC/multiplex 回调与私有路径。未修改生产 API 或清除任何未验证位，whole_host_verified 仍为 false。

## 接入 socket 与内核存量发现（已安装 0.9.7）

已复现 accept 子连接在首次读写前未被登记：阻断未关闭旧连接，恢复后仍可发送。现在普通 accept 登记子连接，继承监听器 scope，计入活动连接与阻塞调用，并用阻断 epoch 防止返回较迟的旧接入连接复活。创建 socket 的系统调用与 scope 登记在同一短临界区内完成，避免扫描将刚创建的控制 socket 错判为宿主对象。

阻断会枚举本进程的 socket，补发现经 accept$NOCANCEL 等路径产生的旧 FD。未知连接默认按宿主处理；只有与已登记豁免 TCP 监听器的实际本地地址、端口及 IPv6 scope 匹配时才继承豁免。内核元数据确认该监听器确实处于 LISTEN；不按远端端口或任意绑定客户端授予豁免。IPv4/IPv6 共 20 项真实 TCP 检查通过，包括普通/非取消 accept、独立对端关闭观察、恢复后旧宿主拒绝发送、控制连接保持可用。

早期候选曾尝试复制 FD 来固定内核对象，遇到 Network.framework 的 guarded FD 后真实触发 EXC_GUARD(DUP)；该候选未安装。已修正为不复制发现的 FD、不修改或解除 guard。遇到 guarded FD 时由拥有它的高层网络对象负责取消，并显式保留 RAW_SOCKET 未验证位；发现失败、超出登记范围等情况也保留该位。扫描不证明未拦截的并发 close/reuse 完全原子，也不覆盖其他进程的网络。

修正后 31 项真实 Swift 回调检查包含实际存在的 guarded IP socket 与 RAW 未验证标志；35 项传输、34 项 URLSession 回调、7 项描述符复用、9 项策略协调回归通过。8 项真实 MCP 检查确认正常入口驻留的控制接口在宿主阻断期间连续响应，恢复后仍可用。全宿主覆盖待验证。

重跑：`run-socket-accept-test.py`、`run-socket-identity-test.py`、`run-swift-callback-probe.py`、`run-network-control-mcp-test.py`，均位于 loader/macos。后两项使用真实隔离宿主；MCP 测试等待编辑器已就绪后再切换策略。

## 未撤销的 guarded socket 活动计数（已安装 0.9.8）

0.9.7 跳过无法由底层安全关闭的 guarded FD 时仍将 connected 清零；全局未验证位虽然保留，活动计数本身不准确。0.9.8 保守地将这类资源计为活动，直到拥有它的网络层实际关闭 FD，内核身份刷新确认其消失。元数据不可读或 shutdown 失败也不冒充已排空。不修改或解除系统拥有的 guard。

`python3 loader/macos/run-socket-guarded-test.py` 创建测试自身拥有的回环 TCP socket，使用真实 DUP guard，检查 guard 保留、RAW 未验证标志、持有期间活动计数非零、真正关闭后归零，共 4 项通过。API 原型和 guard 位取自 [Apple XNU 声明](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/guarded.h)，真实运行结果限定于本机。该诊断不修改系统或其他组件拥有的 guard。31 项 Swift 回调、35+34 项真实宿主传输/URLSession、20 项 IPv4/IPv6 accept、7 项复用及阻断期间的 8 项 MCP 检查通过；整宿主隔离仍未验证。

## WebKit 网络上下文（已安装 0.9.9）

新增独立 `retire_host_web_contexts`，在原有 renderer-only API 之外显式回收观察到的 WebKit 网络进程，保持原接口语义。真实SV2 27项检查通过，包含旧接口仍留下网络进程的对照、启动代理无PID窗口的独立控制检查、真实两类PID退出、待决JS一次原生错误、持久测试cookie保留及网络进程恢复。store弱观察11项、无PID/未知进程拒绝10项检查通过。当时的 SDK 及 Windows x64 交叉编译通过；Windows 当前实现见 [接口与验证范围](WEB-CONTEXTS.md)。此结果不覆盖全部WK客户端回调、持久Web身份切换或其他联网路径；未清除未验证位。见 [接口与验证范围](WEB-CONTEXTS.md)。
