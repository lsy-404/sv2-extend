# SV2 Extend

Synthesizer V Studio 2 的宿主内扩展框架，提供插件加载、实时 MCP、Lua 和 JavaScript 扩展。

## 功能

- Loader 管理宿主副本及可配置数据目录，运行时驻留宿主进程。
- 插件通过版本化 C API 接入，可注册右侧面板和功能设置。
- Extend 面板提供插件安装、更新、启停和删除。
- MCP 支持音符音高、pitchDelta 曲线、声库读取与切换、WAV 导出。
- Lua/JavaScript 共享扩展 API，并提供本地及在线 HTTP 请求。

## 平台

| 平台 | 状态 |
| --- | --- |
| macOS / SV2 2.3.0 arm64 | 本机功能验证 |
| macOS x86_64 | 通用二进制构建 |
| Windows x64 / SV2 2.3.0 | 隔离宿主、原生面板、MCP、内部 Lua、音符与 pitch 编辑验证 |

网络控制已接入 socket、NSURLSession、Network.framework 和 WebKit；各路径的覆盖状态见 [网络状态](docs/NETWORK-STATUS.md)。

## 构建

需要 macOS、Xcode Command Line Tools 和本机安装的 SV2。

```sh
sh loader/macos/build-launcher-app.sh /absolute/path/to/ExtendLoader.app
```

## 目录

- `loader/`：平台启动器与运行时。
- `sdk/`：插件 C ABI、示例和 ABI 检查。
- `plugins/`：MCP、Lua Extend、JavaScript Extend。
- `docs/`：接口、平台和验证范围。

[版本下载](https://github.com/lsy-404/sv2-extend/releases) · [实现状态](docs/IMPLEMENTATION-STATUS.md) · [插件管理](docs/PLUGIN-MANAGER.md)

## 许可证

GPL-3.0-only，见 [LICENSE](LICENSE)。第三方 JSON 组件采用 MIT，MinHook 采用 BSD，声明见 [NOTICE](NOTICE)。
