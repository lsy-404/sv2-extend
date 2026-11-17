# 操作记录

- 2026-11-17：加载 agent-mode 和仓库 AGENTS.md；main 与 origin/main 位于 a7c11d19，工作区干净；git worktree list 仅主检出。
- 初始化 agents/project.md、tasks.md 和本轮计划、发现、进度。
- 创建独立 Windows worktree（codex/windows-ui-parity）和 macOS worktree（codex/macos-host-components），两个子 agent 分别负责；保留所有既有检出。
- Windows 子分支 af35bae 已 fast-forward 合入主检出：设置分组及次级说明；子 agent 继续入口、分类栏和隔离宿主绑定调查。
- 主检出执行 Python/MinGW Windows 构建，输出 test/.outputs/ui-parity-build，exit 0；运行验证与最终安装另行执行。
- IRIS Mac mini 环境任务已排队，macOS 子 agent 通过 IRIS 协调真实绑定、构建与安装；没有 SSH 时不猜测宿主 ABI。
- IRIS 排队无实际执行后取消；SSH连接确认 Darwin arm64，macOS 子 agent 已在独立会话定位 Xcode 27.2、官方 2.3.0 universal 和旧安装备份，建立独立源检出和 worktree。
- Windows 隔离宿主原生探针返回 hiddenRegistry/hiddenSaved/nativeButton/restoredRegistry/restoredSaved/rootOpen/rootOpenKnown/rootOpened 全 true，scriptsEntries=1。产物仅位于子 worktree 的 test/.outputs。
- 使用 computer-use 技能截图确认隔离宿主被首次登录 modal 覆盖；没有操作认证对话框，也没有修改既有 candidate 工程。
- 本地合并 Windows 子分支 94098c8；完整生产构建成功；bootstrap smoke 和 managed smoke 各 11 项通过，profile lease 与缓存时间戳检查通过。
- Windows 最终隔离探针：caption、toggle checked/value、hidden/restored persisted/registry、root open、short-height hidden 与 separator anchor 全通过；可复现工具已提交到根 test。
- Windows 安装器备份旧安装并更新正式安装目录，原开始菜单入口保持；启动该入口继续运行验证。既有其他 candidate 进程保留。
- 正式入口进程 86836 的 Core 模块路径匹配已安装目录；runtime receipt 证实 boxy.sv2 和 sv2.mcp 均 initResult=0，category binding ready、renderer=sv-components、foreignContentWindows=0。
- CUA 实际点击 Scripts、恢复按钮、总显隐开关、MCP Refresh，通过可见控件与首选项/分类状态核对隐藏和恢复。随后仅正常关闭本轮启动的正式空工程并重启，继续检查持久化。
- 正式入口重启后新宿主 97868 读回根入口 visible=false；实际点击 Scripts 底部按钮后根入口恢复、面板打开且开关 On，首选项保存 true。launcher 已退出，原其他 candidate 进程未动。安装 receipt 的 runtimeVerified 在真实验证后标记 true。
