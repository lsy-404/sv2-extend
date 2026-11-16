# 宿主早期插件入口

macOS 0.9.5 Core 在自身构造函数内调用插件的早期入口，然后才允许可执行程序的原生 initializer list 和 main 运行。这不涵盖 Objective-C +load 或所有依赖库构造函数；插件必须对其实际目标生命周期验证顺序。0.9.4 及更早版本不提供此入口；Windows 开发后端也不提供。

插件在 plugin.json 声明：

```json
{"bootstrap":{"abi":1,"guardOwners":["example.recovery"]}}
```

固定导出 `loader_plugin_bootstrap_v1(const loader_bootstrap_api_v1 *)`。SDK 结构携带版本、长度、宿主 PID、边界标志、借用路径、启动网络策略快照、按 owner 查询 guard 数量的纯策略函数和日志函数。不得在入口中调用宿主账号 getter/reader/auth、编辑器、UI、MCP 或修改 session；只安装有版本检查的观察与恢复 hold。路径和 API 对象有效至宿主退出。声明 owner 必须唯一，不能用冒号分隔的 guard 复合键。

顺序：profile 共享内核 lease → 数据 namespace → 必要的启动联网阻断 → 待安装/删除插件事务 → 声明检查 → 早期入口 → 宿主初始化 → 普通插件入口。早期模块由 Core 保留至宿主退出，失败的部分挂钩也不卸载。普通入口复用完全相同的模块句柄，不能在两阶段之间替换该包。

持久策略 schema 2 增加 bootstrapGuards，键必须是 guards 中现存且值为 true 的键。成功 bootstrap 声明过的 owner 再调用 set_startup_network_guard，会持久记录对应的必需早期插件。旧 schema 1 和普通离线 guard 保持兼容；已有普通 guard 需要再次 set(true) 才提升为必需 bootstrap guard。账号插件应先取得独占 profile lease，再创建恢复 guard，直到真实事务结束才删除。Core 不读取插件私有 journal。

下次启动时，必需 owner 的包缺失、禁用、声明无效、导出不存在或入口失败，都会以 78 拒绝宿主原生初始化。无法读取的策略也拒绝，而不是把未知必需插件集解释为空。重新提供匹配插件可恢复，不自动删除恢复 guard。普通 guard 不凭空要求某个产品插件。

验证命令：

```sh
python3 loader/macos/run-startup-policy-test.py
python3 loader/macos/run-bootstrap-order-test.py
python3 loader/macos/run-startup-constructor-test.py
python3 loader/macos/run-plugin-manager-test.py
```

独立可执行程序测试覆盖早期入口先于 executable constructor、普通入口复用同一模块且只初始化一次、借用元数据寿命，以及必需插件缺失/禁用/失败/重复 owner/缺少导出/策略损坏时拒绝进入 constructor/main。这不替代真实 SV2 目标生命周期验收，也不构成完整网络隔离或账号切换证明。

已安装 0.9.5 入口检查通过。独立 fixture 的 16 项顺序/拒绝检查、34 项策略与 lease 检查、50 项包管理检查通过；真实 SV2 已确认观察器早于目标管理器创建、恢复 hold 生效、无 guard 对照存在真实请求，以及完整插件包普通入口复用早期模块。所有正式账号切换和整宿主隔离标志仍为未验证。
