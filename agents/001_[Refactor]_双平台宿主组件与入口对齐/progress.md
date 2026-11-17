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
