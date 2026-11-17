# 跨平台插件契约

插件以同一 `sdk/include/loader_api.h` C ABI 接入 Loader。Lua/JS 模块与方法名称保持平台无关；原生宿主对象、侧栏、网络与启动过程由各平台 Loader 适配。

插件不得直接依赖宿主地址、Objective-C 类型或 Win32 句柄。需要平台功能时，先检查 API `struct_size`、能力位以及方法返回值。未实现的后端返回 `LOADER_ERR_NOT_SUPPORTED`，不伪造成功。`host_network_blocked` 返回策略状态，验证状态由 receipt 返回。

一个插件包包含多个原生库，在 `plugin.json` 中选择：

```json
{
  "schema": 1,
  "loaderAPI": 1,
  "id": "example.hello",
  "name": "Hello",
  "version": "1.0.0",
  "nativeLibraries": {
    "macos-arm64": "native/macos-arm64/plugin.dylib",
    "macos-x86_64": "native/macos-x86_64/plugin.dylib",
    "windows-x86_64": "native/windows-x86_64/plugin.dll"
  }
}
```

`windows-arm64` 也是预留平台键，目前没有 ARM64 Windows 构建验证。原生库不能跨操作系统共用；源码、接口和包格式可以共用。旧 `nativeLibrary` 仅用于兼容既有 macOS 包。存在 `nativeLibraries` 时不得回退至其他平台。

`sdk/examples/hello_plugin.c` 是同一份 C 源码，已在 macOS 构建 arm64/x86_64 Mach-O，并交叉构建 Windows x64 PE。编译产物不等于 Windows 运行验证。

内置编辑器工具使用两平台一致的 `sv_` 名称。独立 provider 的 MCP 工具名称必须使用 `svext_` 前缀，且引用该 provider 自己声明的方法。示例工具为 `svext_hello_greet`。使用 `sdk/examples/build_hello.py --platform macos-arm64 --platform macos-x86_64 --platform windows-x86_64 --out /absolute/path/hello.sv2ext` 生成单个多平台包。

## 当前平台状态

| 能力 | macOS | Windows x64 |
|---|---|---|
| 共享 C ABI、平台包选择 | 本机验证 | 独立 fixture 实机验证 |
| 启动器拉起宿主并退出 | SV2 验证 | 官方 2.3.0 隔离副本实机验证 |
| 宿主内 provider 注册/请求 | SV2 验证 | fixture 请求和正式宿主插件初始化验证 |
| 动态原生右侧面板 | 2.3.0 arm64 验证 | 官方 2.3.0 隔离宿主验证分类、停靠、缩放及状态刷新 |
| MCP、Lua、JS 原生宿主接入 | SV2 验证 | 2.3.0 隔离宿主已验证 MCP、官方 Lua 与 JS 调用 |
| 声库、pitch、导出后端 | SV2 验证 | 已验证音符组、pitch、声库选择及原生 WAV 混音 |
| 托管数据目录重定向 | SV2 验证 | AppContainer 与主程序 Shell 路径重定向验证 |
| 显式 WebKit 上下文回收 | 本机验证；不等于完整隔离 | NOT_SUPPORTED |
| 完整联网隔离 | 尚未验证完整覆盖 | 尚未实现 |

Windows 支持 `--host <official-synthv-studio.exe> <profile-directory>`。启动器复制完整官方安装，迁移用户数据至独立 profile，原目录和已有数据目标保持原样；AppContainer 仅授予 SMBIOS 读取 capability，在 main 前加载 Core。直接导入和动态解析的 Roaming/Local Shell 查询都重定向至 profile。运行时与插件目录只读，host-data、outputs、preferences、storage 可写。同一 profile 的运行锁随宿主保持，重复启动被拒绝。官方目录的文件列表、大小或修改时间变化后，保留旧副本并更新受管程序。

当前后端以离线模式运行，动态联网开发中。AppContainer 也限制对 profile 之外项目路径的访问。

Windows MCP 服务驻留在 SV2 内。右侧工具栏接入宿主原生 Extend 分类及独立插件分类，面板内容由跟随宿主停靠区域的原生 SV 组件渲染；插件使用相同的声明式 UI API，不直接使用宿主控件。官方 2.3.0 隔离宿主已验证分类打开和关闭，以及实际屏幕缩放 100%→150%→100% 时的面板与字体尺寸；重复提交相同插件状态时焦点和滚动位置保持。客户端使用 `Native/Sv2Mcp.exe --endpoint Profile/outputs/mcp-endpoint.json` 建立 stdio 连接；该程序只转发 MCP，无常驻管理逻辑。内部官方 Lua 后端自动启动，用户无需运行脚本或使用 Lua 面板。入口有官方 2.3.0 的版本与字节校验，后端未就绪时不公布工具。

Windows 插件设置和右侧入口显隐保存在 profile 的 `preferences/windows-plugin-ui.json`；macOS 使用 `preferences/extend-ui.json`。两平台保持同一 SDK 契约，各自渲染面板内容。

已在真实隔离宿主验证读取工程、创建普通音符组、添加与删除音符、音高及 pitchDelta 编辑、过期 guard 拒绝与测试内容清理。主组添加/删除音符会在调用原版前拒绝。隔离宿主已通过声库身份读取和选择，以及 MCP、官方 Lua 和 JS 的原生 WAV 导出。工程声库清单和其他能力按运行时注册情况公布；新改动的 macOS 宿主执行尚未验证。

## Windows 开发构建

需要 Python 3、x64 MinGW-w64（GNU C++17、assembler、objcopy、objdump）。

```sh
python3 loader/windows/build.py --out /absolute/path/to/windows-build
```

在 Windows 的 MinGW 环境使用 `--prefix ""`，要求无前缀的 `gcc`、`g++`、`objcopy` 和 `objdump` 均在 PATH。构建会产生 `ExtendLauncher.exe`、`ExtendCore.dll`、测试宿主和同源码 Hello 插件。随后在真实 Windows 执行：

```powershell
./loader/windows/smoke.ps1 -BuildDirectory C:\path\to\windows-build
./test/windows_managed_smoke.ps1 -BuildDirectory C:\path\to\windows-build
```

Windows x64 原生 MinGW 构建及上述 smoke 已执行通过。11 项检查覆盖 Core 在宿主 main 前就绪、API PID、插件调用、拒绝私人能力域与未验证的网络 receipt；脚本另确认宿主和 launcher 退出。它是独立测试宿主验证，不代替 SV2 验收。输出只用于测试，不是运行时文件通信协议。构建报告的 `runtimeVerified: false` 表示构建本身不证明运行，运行结果另由 `windows-runtime-results.json` 记录。

Windows 启动采用 suspended 主线程 + pre-start APC，代码在 `DllMain` 之外初始化；没有外部常驻管理器。APC 按 [Microsoft 的启动前 APC 语义](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-queueuserapc) 排队，避免在 [DllMain 的 loader lock](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain) 中初始化插件。独立 fixture 已确认启动时序；官方 2.3.0 已安装入口验证了同 PID Core 与插件驻留。

JSON 依赖固定为 nlohmann/json 3.11.3，Shell 与编辑器入口使用 MinHook；许可证随构建的 THIRD-PARTY-NOTICES.txt 提供。构建不会下载或分发官方 SV2。

SDK 的 64 位 API 表现为 264 字节，新增 WebKit 上下文回收函数在尾部偏移 240；另有命名空间存储读写在偏移 248/256，旧字段位置保持。macOS arm64/x86_64 与 Windows x64 ABI 静态检查通过；Windows 官方隔离宿主已验证 Core、插件和原生分类面板。未实现的 API 继续返回 NOT_SUPPORTED。

## 接口检查

以下检查比较两平台内置 MCP 名称、方法与描述，并验证 Windows 注册和调用契约。需要 Python 3、MinGW-w64 和 Lua 运行时：

```powershell
python test/check_mcp_contract.py
New-Item -ItemType Directory -Force test/.outputs
g++ -std=c++17 -pthread test/windows_mcp_protocol.cpp -o test/.outputs/mcp-contract.exe
./test/.outputs/mcp-contract.exe
g++ -std=c++17 test/windows_editor_lua_service.cpp -lbcrypt -luser32 -o test/.outputs/editor-contract.exe
./test/.outputs/editor-contract.exe
lua test/script_voice_project_contract.lua
```

这些是接口和模拟工程检查。实际宿主版本、能力注册、声库授权、原生渲染与输出文件仍需在目标宿主验证。

## Windows 安装

构建验证后在 PowerShell 中执行：

```powershell
./loader/windows/install.ps1 -BuildDirectory C:\path\to\windows-build
```

默认安装到当前用户的本地应用目录，创建 `Extend Loader` 开始菜单入口。可用 `-PluginPackage` 提供 Windows 原生插件包目录；更新前关闭受管宿主，安装器备份现有程序及 profile。启动器退出后只保留官方宿主进程。
