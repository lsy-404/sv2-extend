# 调查与决策

- [问题现象] 两平台外观、入口和分组不同 -> [源码核对] Windows 当前已使用 SV 宿主组件，macOS 使用自绘 AppKit；Windows 未处理 settings.section/detail，缺少 Extend 总入口显隐和脚本面板打开按钮 -> [结论] 补齐可观察行为并将 macOS 承载层改为宿主组件，不延续双套视觉风格。
- [分隔线] 两平台分类栏均处理 12px 分隔位置，但尚未用运行时证据确认横线缺失原因 -> [方案] 实际查看布局/绘制实现并验证，不把留白视为线条已绘制。
- [工具] 当前仓库没有文件编辑、Git 或跨设备 shell MCP -> [方案] 本地命令用于必要文件、Git、构建；如需 macOS shell，使用现有已配置访问，不读取或输出凭据。
- [审计] list_workspaces 未返回 sv2-extend -> [结论] 使用本检出 agents，现有 Boxy 的记录与未提交工作不动。
- [远端环境] 用户暂无 Mac SSH 地址 -> [尝试] 通过 IRIS 的 Mac mini/Workspaces 发起环境与组件调研 -> [结论] 模型参数必须为 provider/model；首次未带 provider 的请求未启动并取消，补齐后成功排队。远端任务由 macOS 子 agent 协调。
- [Windows 分组] settings 的 section/detail 没有被渲染 -> [实现] 复用宿主 Label 显示并校验描述字段，定向 contract 通过；入口隐藏必须与 Scripts 恢复按钮一起交付。
- [分支保护] GitHub branches/main 返回 protected=false -> [结论] 完成验证后自动本地合并并推送 main；不建 PR。
- [Mac 通道] IRIS 设备虽 ready，但任务持续 queued，未产生执行工具记录 -> [纠正] 已取消该排队任务；用户提供现有 SSH 连接信息后，成功连接 arm64 Mac，子 agent 在自己的认证会话中开展真实调研，不将凭据写入文件、审计或代码。
- [Windows 恢复] 独立审查发现恢复后开关显示、同步失败与合法容量边界问题 -> [处理] Windows 子 agent 已一并修正并继续验证；不把持久化成功等同于分类实际同步。
- [Windows 原 Scripts] 解包后的真实宿主确认 UserScriptPanel 类型、原内容组件和生命周期 -> [实现] 底部添加宿主 Button，缩小原内容区域；隔离探针验证隐藏/持久化/恢复/重新打开路径。
- [GUI 范围] 隔离 profile 未复制用户认证数据，首次登录弹窗覆盖界面 -> [结论] CUA截图只证明控件实际存在，不把未执行的鼠标交互或被遮挡线条检查写作通过；不自动操作认证弹窗。
- [Windows 文案] 新按钮 literal 曾被错误转码 -> [核对] 用 Unicode 转义检查并修复，真实宿主探针读取底层原生文本 -> [结果] Scripts 按钮、入口开关的中文值均精确匹配。
- [Windows 分隔] 实际宿主分类栏 paint 以 LICENSE 按钮 y−6 绘制横线，现有布局保留 12px 间距 -> [验证] 原生探针确认锚点在栏内 -> [结论] 不改宿主已有绘制；内容分组相关需求另核对用户所指位置。
- [Mac 运行证据] 独立宿主已通过 Component/Viewport/Button/Toggle/Label/Choice 组件树的构造、挂载、解绑和析构；按钮虚槽回调亦通过 -> [范围] 尚未等同可见面板、完整设置交互或安装验收。
- [Mac 探针失败] 首次取 dyld 第 0 映像作为主程序 slide 导致 SIGBUS -> [修复] 通过 MH_EXECUTE 识别真实主程序后同一基础控件探针通过。
- [Windows 安装版] 原有正式 profile 可以正常打开，无首次登录遮挡 -> [实测] 从原 Scripts 按钮打开 Extend、物理点击隐藏总入口、再次点击恢复并核对开关 On/分类 visible/open/磁盘状态一致；原脚本组件不被按钮遮挡，MCP 原生 Refresh 正确刷新 Editor ready -> [结论] 安装版的真实鼠标交互已通过，隔离 profile 的 modal 限制不再影响此项结论。
- [Windows 进程] 正式入口启动后 Core 与两个原生包位于同一宿主 PID，launcher 正常退出；原有另一个 candidate 进程保持不变。
