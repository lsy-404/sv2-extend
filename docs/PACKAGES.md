# 声明式扩展包

扩展包可以不含原生代码。包在 `plugin.json` 中声明它需要的加载器能力、要申请的权限和它贡献的内容；加载器负责解析依赖、向用户展示权限开关，并代替包执行每一次调用。同一个包目录可在所有实现了相应能力的平台上使用。

含 `nativeLibraries` 的包仍按 [跨平台契约](CROSS-PLATFORM.md) 加载。原生代码在宿主内运行并受信任，不受下述权限约束，因此不能同时声明 `permissions` 或 `contributes`。

## 清单

```json
{
  "schema": 1,
  "id": "example.greeter",
  "name": "Greeter",
  "version": "1.0.0",
  "requires": {
    "loader": ">=0.10",
    "capabilities": { "mcp.tools": "*", "panel": "*", "storage": "*" },
    "packages": { "example.hello": ">=1.0" }
  },
  "permissions": ["package.example.hello"],
  "contributes": {
    "mcpTools": [
      {
        "name": "svext_greeter_greet",
        "description": "Read the greeting provided by the Hello package.",
        "inputSchema": { "type": "object", "properties": {}, "additionalProperties": false },
        "call": { "method": "hello.greet" }
      }
    ],
    "panel": {
      "menu": [
        { "title": "Hello", "items": [{ "id": "greet", "title": "Read greeting", "call": { "method": "hello.greet" } }] },
        { "id": "clear", "title": "Clear note", "call": { "method": "storage.write", "arguments": { "key": "note", "value": "" } } }
      ]
    }
  }
}
```

完整示例见 `sdk/examples/greeter/plugin.json`。

| 字段 | 含义 |
| --- | --- |
| `requires.loader` | 加载器版本范围。 |
| `requires.capabilities` | 必须存在的加载器能力及版本范围。任一项不满足，包不加载。 |
| `requires.packages` | 依赖的其他扩展包及版本范围，依赖先于本包启用。 |
| `permissions` | 需要用户开启的权限：可授权能力的标识，或 `package.<id>`（调用所依赖的包提供的方法）。 |
| `contributes.mcpTools` | MCP 工具，名称以 `svext_` 开头；需要能力 `mcp.tools`。 |
| `contributes.panel.menu` | 在 Extend 中注册的面板菜单：动作，或带标题的一组动作，最多 24 行；需要能力 `panel`。 |

版本范围是以空格分隔、需同时成立的比较式：`>=1.2 <2`、`=1.0`、`1.0`、`*`。版本为一至三段非负整数。

`call` 指定目标方法和固定参数。固定参数覆盖调用方传入的同名参数。目标方法必须属于 `requires.capabilities` 中的能力、`permissions` 中的权限，或 `requires.packages` 中原生包注册的方法。未知的贡献类型会使整个包被拒绝。

## 加载器能力

| 能力 | 需授权 | 方法 |
| --- | --- | --- |
| `mcp.tools` | 否 | 发布 MCP 工具 |
| `panel` | 否 | 注册面板与菜单 |
| `storage` | 否 | `storage.read`、`storage.write`：读写包自己的数据 |
| `profile.clean` | 否 | Windows 启动器提供干净 profile 启动；没有可调用方法 |
| `editor.read` | 是 | `selection.read`、`pitch.read_range` |
| `editor.write` | 是 | `notes.set_pitch`、`notes.add`、`notes.remove`、`groups.create`、`groups.remove`、`pitch.replace_range` |
| `transport.read` | 是 | `transport.status` |
| `transport.control` | 是 | `transport.play`、`transport.stop` |
| `voicebanks.read` | 是 | `voicebanks.current`、`voicebanks.project`、`voicebanks.list` |
| `voicebanks.select` | 是 | `voicebanks.select` |
| `exports.read` | 是 | `exports.snapshot`、`exports.status` |
| `exports.write` | 是 | `exports.start`、`exports.cancel` |
| `http` | 是 | `http.start`、`http.status`、`http.cancel` |

Windows 的 `ExtendLauncher --host-clean <official-synthv-studio.exe> <profile-directory>` 使用新建或只包含插件目录的 profile，准备官方运行资源并启动受管宿主。它不会迁移当前用户数据，已初始化的 profile 会被拒绝。支持该入口的启动器与 Core 组合会声明 `profile.clean`。

当前加载器实际提供哪些能力由运行环境决定。`runtime.capabilities` 的 `loader` 字段列出加载器版本和每项能力的 `supported` 状态，`packages` 字段列出每个包的状态、原因和权限开关。

`storage.write` 接受 `{key, value}`，`value` 为任意 JSON；`storage.read` 接受 `{key}`，返回 `{found, value}`。数据位于 `plugin-data/<包 id>/`，与安装目录分离。

## 权限

每个声明式包在 Extend 中有一个由加载器注册的面板，包含“启用扩展包”开关、`permissions` 中每项权限的开关和包声明的菜单。权限默认关闭。当前加载器不提供的能力不显示开关。

调用被拒绝时返回的错误码：

| 错误码 | 原因 |
| --- | --- |
| `PERMISSION_DENIED` | 用户尚未开启该权限。 |
| `PERMISSION_UNDECLARED` | 目标方法属于未在 `permissions` 中声明的包。 |
| `CAPABILITY_UNSUPPORTED` | 当前加载器不提供目标方法所属的能力。 |
| `TARGET_NOT_ALLOWED` | 目标方法不属于能力目录，也不属于原生包。 |
| `PACKAGE_UNLOADED` | 包已停用、移除或未通过依赖解析。 |

## 动态加载

声明式包不需要重启宿主：

- 关闭“启用扩展包”会撤销它的 MCP 工具，重新开启后恢复。
- 调用 `runtime.packages.reload` 会重新扫描 `plugins/`：新增的声明式包被加载，清单变化的包被替换，目录已删除的包被卸载；随后按新的包集合重新解析依赖。

原生包驻留到宿主退出，新增的原生包在下次启动时加载。

## 平台状态

| 平台 | 状态 |
| --- | --- |
| Windows x64 | 已实现；独立 fixture 宿主验证了依赖解析、授权与拒绝、存储读写、缺少能力时不加载和运行时卸载。尚未在 SV2 宿主中验证，`http` 未提供。 |
| macOS | 尚未实现；无原生库的包会被跳过。 |

Lua 与 JavaScript 脚本贡献尚未实现。
