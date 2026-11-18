# 声明式扩展包

扩展包可以不含原生代码。包在 `plugin.json` 中声明它需要的加载器能力、要申请的权限和它贡献的内容，需要逻辑时附带在加载器隔离运行时中执行的 Lua 代码；加载器负责解析依赖、向用户展示权限开关，并代替包执行每一次调用。同一个包目录可在所有实现了相应能力的平台上使用。

含 `nativeLibraries` 的包仍按 [跨平台契约](CROSS-PLATFORM.md) 加载。原生代码在宿主内运行并受信任，不受下述权限约束，因此不能同时声明 `permissions`、`main` 或 `contributes`。

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

完整示例见 `sdk/examples/greeter`（纯声明）和 `sdk/examples/counter`（带代码，并被 greeter 依赖）。

| 字段 | 含义 |
| --- | --- |
| `requires.loader` | 加载器版本范围。 |
| `requires.capabilities` | 必须存在的加载器能力及版本范围。任一项不满足，包不加载。 |
| `requires.packages` | 依赖的其他扩展包及版本范围，依赖先于本包启用。 |
| `permissions` | 需要用户开启的权限：可授权能力的标识，或 `package.<id>`（调用所依赖的包提供的方法）。 |
| `main` | 包代码入口，包内相对路径的 `.lua` 文件；需要能力 `runtime.lua`。 |
| `contributes.methods` | 向依赖本包的其他包公开的处理函数名，方法名为 `<包 id>.<函数名>`；需要 `main`。 |
| `contributes.mcpTools` | MCP 工具，名称以 `svext_` 开头；需要能力 `mcp.tools`。 |
| `contributes.panel.menu` | 在 Extend 中注册的面板菜单：动作，或带标题的一组动作，最多 24 行；需要能力 `panel`。 |

版本范围是以空格分隔、需同时成立的比较式：`>=1.2 <2`、`=1.0`、`1.0`、`*`。版本为一至三段非负整数。

`call` 指定 `method`（目标方法）或 `handler`（本包代码中的处理函数）之一，以及固定参数。固定参数覆盖调用方传入的同名参数。目标方法必须属于 `requires.capabilities` 中的能力、`permissions` 中的权限，或 `requires.packages` 中的包提供的方法。未知的贡献类型会使整个包被拒绝。

## 加载器能力

| 能力 | 需授权 | 方法 |
| --- | --- | --- |
| `mcp.tools` | 否 | 发布 MCP 工具 |
| `panel` | 否 | 注册面板与菜单 |
| `storage` | 否 | `storage.read`、`storage.write`：读写包自己的数据 |
| `runtime.lua` | 否 | 在隔离的 Lua 5.5 运行时中执行包代码 |
| `editor.read` | 是 | `selection.read`、`pitch.read_range` |
| `editor.write` | 是 | `notes.set_pitch`、`notes.add`、`notes.remove`、`groups.create`、`groups.remove`、`pitch.replace_range` |
| `transport.read` | 是 | `transport.status` |
| `transport.control` | 是 | `transport.play`、`transport.stop` |
| `voicebanks.read` | 是 | `voicebanks.current`、`voicebanks.project`、`voicebanks.list` |
| `voicebanks.select` | 是 | `voicebanks.select` |
| `exports.read` | 是 | `exports.snapshot`、`exports.status` |
| `exports.write` | 是 | `exports.start`、`exports.cancel` |
| `http` | 是 | `http.start`、`http.status`、`http.cancel` |

当前加载器实际提供哪些能力由运行环境决定。`runtime.capabilities` 的 `loader` 字段列出加载器版本和每项能力的 `supported` 状态，`packages` 字段列出每个包的状态、原因和权限开关。

`storage.write` 接受 `{key, value}`，`value` 为任意 JSON；`storage.read` 接受 `{key}`，返回 `{found, value}`。数据位于 `plugin-data/<包 id>/`，与安装目录分离。

## 包代码

每个带 `main` 的包拥有一个独立的 Lua 5.5 状态，由加载器内置，与宿主的脚本引擎无关，两个包之间不共享任何对象。入口文件以 `extend` 为参数执行一次，返回处理函数表：

```lua
local extend = ...
local handlers = {}

function handlers.increment(arguments, reply)
  extend.request('storage.read', {key = 'count'}, function(stored, failure)
    if failure then reply(nil, failure) return end
    local count = (stored.found and stored.value or 0) + (arguments.by or 1)
    extend.request('storage.write', {key = 'count', value = count}, function(_, writeFailure)
      if writeFailure then reply(nil, writeFailure) else reply({count = count}) end
    end)
  end)
end

return handlers
```

- 处理函数收到 `(arguments, reply)`；`reply(result)` 或 `reply(nil, error)` 结束一次调用，可以在异步回调中调用。
- `extend.request(method, arguments, callback)` 是代码访问外部的唯一途径，回调收到 `(result, error)`；它与声明式调用经过同一套能力和权限检查。
- `extend.log(...)` 与 `print` 写入加载器日志；`extend.package` 提供 `id` 和 `version`。
- 可用的库只有 base、coroutine、table、string、math、utf8。`os`、`io`、`debug`、`package`、`require`、`load`、`dofile`、`loadfile`、`collectgarbage`、`string.dump` 不存在，也不能访问宿主的 `SV` 对象；只加载文本源码，上限 256 KiB。
- 每个状态内存上限 16 MiB；每次进入代码的指令预算为两千万条，超出后调用以 `HANDLER_FAILED` 结束，状态可继续使用。单次 C 库调用（如病态的模式匹配）内部不计入预算。
- 表的键恰为 1..n 时转换为 JSON 数组，否则键必须全部是字符串；空表转换为对象。
- 入口加载失败，或清单引用的处理函数不存在，包不会启用。

依赖的包先于本包启动；依赖关系不允许成环，因此对依赖包的调用不会回到调用方。

## 权限

每个声明式包在 Extend 中有一个由加载器注册的面板，包含“启用扩展包”开关、`permissions` 中每项权限的开关和包声明的菜单。权限默认关闭。当前加载器不提供的能力不显示开关。

调用被拒绝时返回的错误码：

| 错误码 | 原因 |
| --- | --- |
| `PERMISSION_DENIED` | 用户尚未开启该权限。 |
| `PERMISSION_UNDECLARED` | 目标方法所属的能力或包未在清单中声明。 |
| `CAPABILITY_UNSUPPORTED` | 当前加载器不提供目标方法所属的能力。 |
| `TARGET_NOT_ALLOWED` | 目标方法不属于能力目录，也不属于 `requires.packages` 中的包。 |
| `PACKAGE_UNLOADED` | 包已停用、移除或未通过依赖解析。 |
| `HANDLER_FAILED` | 处理函数抛出错误、超出指令预算或内存上限。 |

## 动态加载

声明式包不需要重启宿主：

- 关闭“启用扩展包”会撤销它的 MCP 工具和公开方法并销毁它的 Lua 状态，重新开启后重新加载。
- 调用 `runtime.packages.reload` 会重新扫描 `plugins/`：新增的声明式包被加载，清单变化的包被替换，目录已删除的包被卸载；随后按新的包集合重新解析依赖。

原生包驻留到宿主退出，新增的原生包在下次启动时加载。

## 平台状态

| 平台 | 状态 |
| --- | --- |
| Windows x64 | 已实现；独立 fixture 宿主验证了依赖解析、授权与拒绝、存储读写、缺少能力时不加载、运行时卸载与恢复、包代码处理调用、依赖包代码被调用、失控代码被终止和运行时隔离。尚未在 SV2 宿主中验证，`http` 未提供。 |
| macOS | 尚未实现；无原生库的包会被跳过。 |

包代码目前只有 Lua 运行时。向宿主用户脚本（官方 Lua/JS）提供模块尚未实现。
