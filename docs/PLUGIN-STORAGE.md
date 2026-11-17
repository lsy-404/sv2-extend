# 插件状态存储

SDK 提供 `plugin_storage_write_scoped`（偏移 248）和 `plugin_storage_read_scoped`（偏移 256），所需最小表长为 264，当前 64 位 API 表长为 272。调用前检查结构长度、`LOADER_CAP_SCOPED_STORAGE` 和对应函数指针。macOS 和 Windows 受管宿主均提供本机实现。

每个插件应使用自己的 manifest/provider ID。新状态放在 profile 的 `plugin-data/<插件ID>/<key>`，与安装包分离，避免插件目录改名、升级、卸载或重新安装时产生冲突。目录迁移包含 plugin-data；选择已有 profile 不合并另一份状态，目标同名数据存在时拒绝覆盖。删除插件包不自动清除此数据。

ID 为最多 96 字节的 ASCII 标识符，以字母开头，其余字符为字母、数字、点、下划线或连字符。key 为最多 128 字节的单一文件名；拒绝路径分隔符、点/双点和保留的 `.loader-` 前缀。单值最多 1 MiB，可存二进制或空值。保留前缀检查不区分大小写。物理目录和文件名将大写 ASCII 编码为小写 `%xx`，超出文件名长度时使用带保留前缀的 SHA256 名称，使逻辑 ID/key 在默认不区分大小写的文件系统上仍保持区分；调用方无需自行编码。

写入是同步调用：借用输入到函数返回，创建私有临时文件、完整写入、fsync、原子替换并 fsync 目录。目录按 0700、文件按 0600 创建。协作的 SDK 读写以每个命名空间的文件锁协调。读取可能创建命名空间和锁文件；不提供账户事务或原生插件内存隔离。原生插件本身具有宿主权限，plugin_id 是命名空间，不是鉴权身份。

读取时 length 为缓冲区容量，返回时为实际所需长度。data=NULL 查询长度；BUFFER_TOO_SMALL 更新长度而不修改缓冲区。另有 NOT_FOUND、STORAGE_NOT_CONFIGURED、STORAGE_IO 和 INVALID。调用方应处理两次调用之间值变化的情况。符号链接、非单链接文件、FIFO、不安全的目录/文件权限等被拒绝。

STORAGE_IO 不保证目标仍为旧值：替换后目录 fsync 失败时，新值可能已完整提交。调用方必须重新读取并协调，不能将失败结果当成“未写入”。接口不提供多文件事务，也不能替代具备额外归属验证的私有业务 journal。

旧 `plugin_storage_write` 继续写 `plugins/network-probe/data`，仅供原型兼容。它不自动迁移到某个插件的新命名空间，因为旧接口没有插件 ID，无法可靠推断归属。

检查命令：

```sh
python3 loader/macos/run-scoped-storage-test.py
python3 loader/macos/run-scoped-storage-host-probe.py
python3 loader/macos/run-scoped-storage-migration-test.py --app /absolute/path/ExtendLoader.app
```

25 项实际 macOS 文件系统检查覆盖命名空间、权限、边界、二进制/空值、无跟随读取、硬链接/FIFO拒绝、旧路径和多进程原子快照。真实隔离 SV2 使用实际 SDK 表完成 8 项检查。启动器 configure-only 迁移检查不启动宿主，也不读取或改变原始账号数据。
