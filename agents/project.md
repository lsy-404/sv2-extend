# SV2 Extend 项目索引
> 最后更新：2026-11-17

## 项目目标
为 Synthesizer V Studio 2 提供宿主内插件、MCP 和脚本扩展，保持原版安装、工程和账号数据不变。

## 技术栈
C/C++、Objective-C/Objective-C++、Swift、Lua、JavaScript；Windows MinGW 和 macOS Xcode 工具链。

## 模块结构
- loader/windows：Windows 启动、隔离宿主、宿主组件面板和编辑器接入。
- loader/macos：macOS 启动、宿主分类、插件管理和编辑器接入。
- sdk：版本化 C ABI 和插件示例。
- plugins：MCP、Lua 和 JavaScript 扩展。
- test：本轮新增验证和隔离输出（test/.outputs）。

## 工作约定
- 此检出未注册 IRIS 工作区，使用文件审计路径。
- Windows worktree 放在当前用户的 .codex/worktrees/sv2-extend 下；macOS 按用户指定的 .codex/worktrees/sv2-extend 下放置。
- 详细计划、依据和执行结果仅存于 agents。保留原版安装和数据，更新前备份。
