# SV2 Extend

Synthesizer V Studio 2 的宿主内扩展框架，提供插件加载、实时 MCP、Lua 和 JavaScript 扩展。

## 功能

- Loader 管理宿主副本及可配置数据目录，运行时驻留宿主进程。
- 插件通过版本化 C API 接入，可注册右侧面板和功能设置。
- Extend 面板提供插件入口和设置；macOS 还提供插件安装、更新、启停和删除。
- MCP 提供编辑器、声库和 WAV 导出工具；工具仅在对应原生能力可用时公布。
- Lua 和 JavaScript 使用相同的编辑器、声库和导出方法，并提供 HTTP 请求。

## 平台

| 平台 | 状态 |
| --- | --- |
| macOS / SV2 2.3.0 arm64 | 本机功能验证 |
| macOS x86_64 | 通用二进制构建 |
| Windows x64 / SV2 2.3.0 | 隔离宿主已验证原生面板、MCP、官方 Lua/JS、声库选择与 WAV 导出 |

网络控制已接入 socket、NSURLSession、Network.framework 和 WebKit；各路径的覆盖状态见 [网络状态](docs/NETWORK-STATUS.md)。

## 构建

macOS 构建需要 Xcode Command Line Tools 和本机安装的 SV2。

```sh
sh loader/macos/build-launcher-app.sh /absolute/path/to/ExtendLoader.app
```

Windows x64 需要 Python 3 和 MinGW-w64：

```powershell
python loader/windows/build.py --prefix "" --out ./build/windows
```

构建不等于安装或宿主运行验证。声库清单与导出接口见 [声库 API](docs/VOICE-API.txt) 和 [WAV 导出 API](docs/EXPORT-API.txt)。

## 目录

- `loader/`：平台启动器与运行时。
- `sdk/`：插件 C ABI、示例和 ABI 检查。
- `plugins/`：MCP、Lua Extend、JavaScript Extend。
- `docs/`：接口、平台和验证范围。

[版本下载](https://github.com/lsy-404/sv2-extend/releases) · [实现状态](docs/IMPLEMENTATION-STATUS.md) · [插件管理](docs/PLUGIN-MANAGER.md) · [声明式扩展包](docs/PACKAGES.md)

## 许可证

GPL-3.0-only，见 [LICENSE](LICENSE)。第三方 JSON 组件和 Lua 采用 MIT，MinHook 采用 BSD，声明见 [NOTICE](NOTICE)。
