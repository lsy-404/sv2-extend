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

完整示例见 `sdk/examples/greeter`（纯声明）、`sdk/examples/counter`（带代码，并被 greeter 依赖）和 `sdk/examples/echo`（自己开 HTTP 服务并发起请求）。

| 字段 | 含义 |
| --- | --- |
| `requires.loader` | 加载器版本范围。 |
| `requires.capabilities` | 必须存在的加载器能力及版本范围。任一项不满足，包不加载。 |
| `requires.packages` | 依赖的其他扩展包及版本范围，依赖先于本包启用。 |
| `permissions` | 需要用户开启的权限：可授权能力的标识，或 `package.<id>`（调用所依赖的包提供的方法）。 |
| `main` | 包代码入口，包内相对路径的 `.lua` 文件；需要能力 `runtime.lua`。 |
| `update.url` | 包自己声明的更新端点（必须 HTTPS）。加载器把 `update.fetch` 锁定到此地址，并在包状态和 `extend.package.update` 中公开它。 |
| `icon` | 包的分类图标，内联 SVG 字符串（≤2048 字节）。省略时使用通用插件图标。 |
| `activeIcon` | 分类打开时使用的内联 SVG 字符串（≤2048 字节），保留其中的配色。 |
| `matchIconColors` | 布尔值，默认 `true`。将单色黑白 SVG 匹配到宿主的浅灰和激活绿。设为 `false` 可保留图标自带的配色。 |
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
| `profile.clean` | 否 | Windows 启动器提供干净 profile 启动；没有可调用方法 |
| `runtime.lua` | 否 | 在隔离的 Lua 5.5 运行时中执行包代码 |
| `runtime.protected` | 否 | 读取包自带的密文、在内存中解密并加载为可执行逻辑 |
| `editor.read` | 是 | `selection.read`、`pitch.read_range` |
| `editor.write` | 是 | `notes.set_pitch`、`notes.add`、`notes.remove`、`groups.create`、`groups.remove`、`pitch.replace_range` |
| `transport.read` | 是 | `transport.status` |
| `transport.control` | 是 | `transport.play`、`transport.stop` |
| `voicebanks.read` | 是 | `voicebanks.current`、`voicebanks.project`、`voicebanks.list` |
| `voicebanks.select` | 是 | `voicebanks.select` |
| `exports.read` | 是 | `exports.snapshot`、`exports.status` |
| `exports.write` | 是 | `exports.start`、`exports.cancel` |
| `http` | 是 | `http.request`：发起 HTTPS 请求 |
| `http.server` | 是 | `server.listen`、`server.close`：在本机回环地址提供 HTTP 服务 |
| `update` | 是 | `update.fetch`：向清单 `update.url` 声明的端点发起请求（地址由加载器锁定）|

Windows 的 `ExtendLauncher --host-clean <official-synthv-studio.exe> <profile-directory>` 使用新建或只包含插件目录的 profile，准备官方运行资源并启动受管宿主。它不会迁移当前用户数据，已初始化的 profile 会被拒绝。支持该入口的启动器与 Core 组合会声明 `profile.clean`。

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
- `extend.b64encode(bytes)` / `extend.b64decode(string)` 在原始字节与标准 Base64 之间转换，便于处理端点返回的密钥、IV 等。
- 可用的库只有 base、coroutine、table、string、math、utf8。`os`、`io`、`debug`、`package`、`require`、`load`、`dofile`、`loadfile`、`collectgarbage`、`string.dump` 不存在，也不能访问宿主的 `SV` 对象；只加载文本源码，上限 256 KiB。
- 每个状态内存上限 16 MiB；每次进入代码的指令预算为两千万条，超出后调用以 `HANDLER_FAILED` 结束，状态可继续使用。单次 C 库调用（如病态的模式匹配）内部不计入预算。
- 表的键恰为 1..n 时转换为 JSON 数组，否则键必须全部是字符串；空表转换为对象。
- 入口加载失败，或清单引用的处理函数不存在，包不会启用。

依赖的包先于本包启动；依赖关系不允许成环，因此对依赖包的调用不会回到调用方。

## 加密逻辑

部分实现逻辑可以密文形式随包分发，由包在运行时自行核验访问权限（例如向自己的授权端点取回密钥）后，在内存中解密并加载。明文不落盘，由包代码决定何时解密、何时加载。

声明 `runtime.protected` 后，`extend` 多出三个方法：

- `extend.resource(name)`：读取包目录内的一个自带文件（如密文 `secret.bin`）为原始字节，路径限定在包目录内，上限 1 MiB。
- `extend.decrypt{algorithm, key, iv, data, tag, aad?}`：AES-GCM 解密，`key`/`iv`/`data`/`tag` 均为原始字节；`algorithm` 为 `aes-256-gcm`（默认）、`aes-192-gcm` 或 `aes-128-gcm`，`iv` 为 12 字节、`tag` 为 16 字节，认证失败即报错。
- `extend.load(source, name?)`：在同一个沙箱、同样的内存与指令预算下编译并运行一段源码（通常是刚解密出的明文），返回其返回值。只接受文本源码，不接受字节码。

典型流程：处理函数先核验访问权限取回密钥，再读取密文、解密、加载，然后把加载结果作为本包其余逻辑的实现：

```lua
local extend = ...
local handlers, protected = {}, nil
function handlers.open(request, reply)
  local ok, result = pcall(function()
    if not protected then
      -- 向自己的端点核验访问权限、取回密钥，这里用传入参数示意
      local blob = extend.resource("secret.bin")
      local source = extend.decrypt{
        key = extend.b64decode(request.key),
        iv = extend.b64decode(request.iv),
        tag = extend.b64decode(request.tag),
        data = blob,
      }
      protected = extend.load(source, "secret")
    end
    return protected(request.input)
  end)
  if ok then reply(result) else reply(nil, { code = "LOCKED", message = tostring(result) }) end
end
return handlers
```

密钥、IV、认证标签与密文如何分发与保管由包自己决定；加载器只提供“核验后在内存解密并加载”这一机制，不保存密钥，也不限制逻辑如何取得密钥。被解密出的处理函数由本包的已声明处理函数内部调用，不能登记为 `contributes.methods` / 菜单 / MCP 工具（这些在加载时就要求处理函数已存在）。

加载器提供的 AES-GCM 只是便利项。包也可以自带加密实现：用纯 Lua 自行解密，或在自己的服务端解密后把明文取回，再交给 `extend.load`。`extend.resource` 与 `extend.load` 不要求使用 `extend.decrypt`。

## 更新

包可以在清单里声明 `update.url`（HTTPS），作为自己的更新端点。加载器不代替包决定何时更新或如何校验，只做两件事：把该地址在包状态（`runtime.capabilities` 的 `update` 字段）和 `extend.package.update` 中公开；在用户开启 `update` 权限后，`update.fetch` 向这个被锁定的地址发起请求（复用出站请求的 HTTPS 与大小校验，包不能改写目标主机）。

按需更新由包自己驱动：在需要时 `extend.request("update.fetch", {headers=...}, callback)` 取回更新描述（可带鉴权头），用自己的方式校验签名、解密负载，然后把新逻辑直接 `extend.load` 到内存（无需落盘），或写入自己的 `storage` 缓存。更新新增的能力仍受权限约束——即便包改写自己的清单要求更多能力，这些能力默认关闭，仍需用户逐项开启。

## 网络

### 出站请求

`http.request` 接受 `{url, method, headers, body, timeoutMs}`，返回 `{status, headers, body}`；响应不是 UTF-8 文本时以 `bodyBase64` 返回。

- 只允许 HTTPS；`http://` 仅限回环主机（`127.0.0.1`、`localhost`、`::1`），例如本机的模型服务。
- 用户开启 `http` 权限后可访问任意主机。
- 校验服务器证书，跟随重定向但不从 HTTPS 降到 HTTP，不保存 Cookie，不自动附带凭据。
- 方法为 GET、HEAD、POST、PUT、PATCH、DELETE；请求体上限 1 MiB，响应上限 4 MiB，超时默认 30 秒、最长 120 秒；同时进行的请求最多 8 个。
- 响应在收完后一次返回，不支持流式读取。
- 请求由宿主进程发出，宿主被设为离线时同样受阻断。

### 包自己的 HTTP 服务

包代码调用 `server.listen` 开启监听，调用 `server.close` 或包被停用、卸载时关闭；每个包同时只有一个监听。

```lua
extend.request('server.listen', {handler = 'serve'}, function(server, failure)
  -- server.url 形如 http://127.0.0.1:49152，server.token 为本次监听的令牌
end)

function handlers.serve(request, reply)
  reply({status = 200, headers = {['Content-Type'] = 'application/json'}, body = '{}'})
end
```

- 只绑定 `127.0.0.1` 的临时端口，端口和令牌每次监听重新生成。
- 处理函数收到 `{method, path, query, headers, body}`（头名小写），以 `{status, headers, body}` 应答；路由、协议和状态完全由包代码决定。
- 默认要求请求带 `Authorization: Bearer <token>`，否则返回 401。传入 `auth = "none"` 可取消令牌，此时带 `Origin` 头的请求（来自浏览器页面）一律返回 403。
- 请求头上限 16 KiB、请求体 1 MiB、响应体 4 MiB；处理函数 30 秒内未应答返回 504；每个连接只处理一个请求，不支持分块请求体和流式响应。
- 监听属于加载器的本地控制通道，宿主离线时保持可用。

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
| `INSECURE_URL` | 出站请求使用了非回环主机的 `http://`。 |
| `HANDLER_FAILED` | 处理函数抛出错误、超出指令预算或内存上限。 |

## 动态加载

声明式包不需要重启宿主：

- 关闭“启用扩展包”会撤销它的 MCP 工具和公开方法并销毁它的 Lua 状态，重新开启后重新加载。
- 调用 `runtime.packages.reload` 会重新扫描 `plugins/`：新增的声明式包被加载，清单变化的包被替换，目录已删除的包被卸载；随后按新的包集合重新解析依赖。

原生包驻留到宿主退出，新增的原生包在下次启动时加载。

## 外观

包清单提供分类图标的初始值。原生插件可以在自己的面板描述符中指定这些字段，并通过 `update_plugin_ui` 在运行中修改。Windows 声明式包可在面板注册后调用 `extend.request("ui.icon", fields, callback)` 更新自己的图标。运行中传入空字符串可清除对应的 `icon` 或 `activeIcon`。内置 Lua、JavaScript 和 MCP 分类使用自己的面板描述符，Extend 根图标由加载器管理。

## 平台状态

| 平台 | 状态 |
| --- | --- |
| Windows x64 | 已实现；独立 fixture 宿主验证了依赖解析、授权与拒绝、存储读写、缺少能力时不加载、运行时卸载与恢复、包代码处理调用、依赖包代码被调用、失控代码被终止、运行时隔离，包读取自带密文、在内存解密并加载执行且篡改密文被拒绝，更新端点声明的公开与 `update.fetch` 的权限锁定，以及包开启本机 HTTP 服务并经出站请求访问它、令牌校验和非回环 `http://` 被拒绝。HTTPS 与证书校验用独立程序对公网站点验证过。尚未在 SV2 宿主中验证，包括离线策略对包请求的影响。 |
| macOS | 声明式运行时尚未移植：当前 macOS 加载器只处理带原生库的包，无原生库的包会被跳过。平台无关部分（`package_model.h`、`package_runtime.h`、`package_crypto.h`、`builtin_icons.h`）已就绪，其中 AES-GCM 原语已含 CommonCrypto 后端；仍需把清单解析、Lua 运行时、能力与网络接入 macOS 加载器，密封逻辑与更新随之生效。该移植需在 macOS 工具链上构建验证。 |

## 跨平台对齐

两平台共享的契约集中在 `loader/packages/`（清单模型、运行时、AES-GCM 原语、图标素材），各平台加载器消费同一份。已对齐的行为：清单 schema 与能力目录、插件面板描述符（id/name/version/status/sidebar/settings，新增 `icon`）、分类图标渲染（声明式、与标识无关）。仍存在的差异：声明式运行时（Lua 运行时、能力、网络、密封逻辑、更新）目前只在 Windows，macOS 待移植（见上）；MCP 分类标识在两平台分别为 `sv2.mcp`（Windows）与 `opensv2.mcp`（macOS），因图标改为声明式后已不影响外观，统一标识需连带偏好迁移，另行处理。本机只有 Windows 工具链，macOS 侧改动按契约对齐、随 macOS 构建验证。

包代码目前只有 Lua 运行时。向宿主用户脚本（官方 Lua/JS）提供模块尚未实现。
