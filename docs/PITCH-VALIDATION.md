# 实时 pitch 验证

0.9.10 安装版 Core/MCP 的副本在真实 SV2 2.3.0 arm64 中通过 9 项检查：音符 MIDI pitch 写入及读回、旧 guard 拒绝、恢复原音高；pitchDelta 范围三点曲线写入及读回、旧曲线 guard 拒绝、恢复原曲线，宿主保持存活。所有请求使用驻留 MCP/官方 Lua provider，不以工程文件修改或文件/pipe Bridge 传递请求。测试只改独立未保存工程，结束后退出宿主。

复现：`python3 loader/macos/run-installed-pitch-regression.py`，在其测试宿主的“脚本 → Extend Acceptance”运行 Extend Pitch Fixture。脚本拒绝非空文件名或已有额外音符组的工程，创建普通音符组；驱动观察到一个音符后执行检查。初版准备脚本曾向轨道主组添加音符，被官方 API 拒绝；已修正并在同一宿主重新扫描后执行，失败不计为验收通过。

验证覆盖读取、修改、过期保护与恢复原值；不覆盖本轮原生 Undo、任意工程结构、所有参数、其他宿主版本或账号/完整联网隔离。
