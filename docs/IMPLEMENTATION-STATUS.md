# 实现状态

下表中 macOS 历史验证使用 SV2 2.3.0 arm64；新共享声库清单尚未在 macOS 宿主执行。

| 功能 | 验证范围 |
| --- | --- |
| Loader 与数据目录 | 已安装入口、同 PID 启动、数据迁移及冲突处理 |
| 插件 API | Core/MCP 模块归属、SDK264 ABI、命名空间存储 |
| 实时编辑 | 音符音高和 pitchDelta 写入、读回、过期 guard、恢复原值 |
| 声库 | 读取、切换和身份读回 |
| Lua/JS | 官方脚本入口、声库操作、WAV 导出 |
| 导出 | 格式、完成状态、取消、已有输出处理 |
| 设置 | 主开关、函数权限、MCP 工具和监听生命周期 |
| 网络控制 | 分传输路径验证，详见 NETWORK-STATUS.md |
| Windows | 官方 2.3.0 隔离宿主已验证原生分类和面板、MCP、官方 Lua/JS、音符与 pitch、声库选择及 WAV 混音导出；动态联网覆盖尚未完成，取消操作仍在验证 |

接口和测试命令见 VOICE-VALIDATION.md、PITCH-VALIDATION.md、EXPORT-API.txt、CROSS-PLATFORM.md。
