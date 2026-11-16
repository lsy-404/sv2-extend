JS Extend 请求接口

在受支持的 SV2 中通过 Extend Loader 启动后，官方 JavaScript 脚本可加载：
var Extend = eval(Duktape.enc('opensv2.module.v1', ''));

本地服务和在线服务共用同一个异步接口：
Extend.http.get('http://127.0.0.1:8080/status', callback);
Extend.http.get('https://example.com/', callback);

callback(error, response) 在官方脚本的定时回调中取得结果。成功结果含
status、body、bodyEncoding（utf8 或 base64）、byteLength、redirectsFollowed。
HTTP 404 等状态仍有 response；连接或 TLS 失败通过 error 返回。
官方脚本须在请求回调结束后调用 SV.finish()，不要提前结束脚本。

请求通过 Loader 的 loader.http 能力在宿主内执行，没有外部 JS 服务或
文件/pipe 通信。支持 HTTP/HTTPS；TLS 校验开启，不跟随重定向，
超时 8 秒，响应上限 512 KiB。不支持 file:、URL 用户名密码或片段。
请求不自动携带宿主账号 cookie/session。

实际验证：SV2 2.3.0 arm64 / Duktape 2.5.0，见
loader/macos/evidence/javascript-native-http-runtime.json。
本地 HTTP、在线 HTTPS、404、二进制响应、TLS 拒绝、禁止重定向及
宿主 PID 归属均通过。该历史证据不是新隔离数据目录版本的验收。

开发中的通用请求（尚未安装到稳定入口）：
Extend.http.request({
    url: 'http://127.0.0.1:8080/api', method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({text: '声库'})
}, function (error, response) { /* 使用结果后再 SV.finish() */ });

共享传输层支持 GET、HEAD、POST、PUT、PATCH、DELETE、OPTIONS。
body 是 UTF-8 字符串，最大 64 KiB；GET/HEAD 不接受 body。
请求头最多 32 个、总计 16 KiB；拒绝控制字符和 Host/Content-Length/
Transfer-Encoding 等由传输层管理的头。响应格式与 get 相同。
二进制请求体尚未实现，不能宣称任意 fetch 兼容。

启动环境 OPEN_SV2_SCRIPT_HTTP_LOCAL=0 可禁止按 URL 标识的回环目标，
OPEN_SV2_SCRIPT_HTTP_ONLINE=0 可禁止其余目标。未设置时两类均允许。
LOCAL 指 localhost/*.localhost、127.0.0.0/8、::1；不是整个局域网。
这是脚本目的地分类开关，不是 DNS 解析后 IP 防火墙或宿主联网控制。
新版实验 Loader 已在设置界面接入两个开关，按数据目录保存，下次启动生效。
启动引导从受管副本读取保存的策略，覆盖相反的环境变量；旧副本没有策略
字段时仍沿用启动环境。也可用 --script-http-local allow|deny 和
--script-http-online allow|deny 配合 --configure-only 修改当前目录的设置。

共享原生层 22 项真实 libcurl 检查通过（独立测试进程）：
loader/macos/evidence/resident-http-request-transport.json。
重新验证：python3 loader/macos/run-resident-http-test.py。

真实宿主新增接口验收：SV2 2.3.0 arm64 / Duktape 2.5.0，PID 56919。
POST、请求头、中文 JSON、兼容 GET、在线禁用、异常头拒绝均通过；
主线程回调及与宿主相同 PID 已核对，没有文件/pipe 通信。
证据 loader/macos/evidence/javascript-http-request-runtime.json。
本轮在线权限关闭；在线 HTTPS 成功仍以历史 GET 证据为准。

设置验证：旧 JSON 兼容、保存/重读、两种策略的实际宿主请求、直接启动、
相反环境变量不能覆盖保存策略，共 34 项通过；设置 UI 保存/取消另 4 项通过。
证据 script-http-settings-runtime.json、script-http-settings-ui.json。
这些检查使用独立测试 Loader 和数据目录，尚未升级稳定入口。

取消请求（开发版本，尚未升级稳定安装）：
var handle = Extend.http.request({url: 'http://127.0.0.1:8080/slow'}, callback);
handle.cancel();

get() 和 request() 均返回 cancel() 句柄。回调交付前调用 cancel()，最终
callback 收到 HTTP_CANCELLED；重复取消与完成后取消安全。取消是尽力停止
传输，不能撤回已经发送的数据。仍须保留脚本运行到回调后再 SV.finish()。

共享 Loader API 提供 http.start（返回 requestId）、http.status（pending 或
response，最终结果读取一次后释放）、http.cancel。最多 64 个内存请求记录，
60 秒后清理并发出取消信号。旧 http.get/http.request 能力保持兼容；旧调用
没有 requestId 时不支持取消。新 JS 封装使用 start/status/cancel，不受通用
调用六秒等待限制影响；传输自身超时仍为八秒。

11 项真实原生取消检查以及真实 SV2 官方 JS 取消验收通过，证据
resident-http-cancellation.json、javascript-http-cancellation-runtime.json。
首次立即取消竞态的失败记录另行保留，修复后才通过官方 JS 验收。
