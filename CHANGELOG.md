# Changelog / 更新记录

## v1.0.2 — 2026-10-06

### 中文

- 第三版以源码发布，本次不生成新版安装包或便携包；现有安装下载保留 v1.0.1。
- 切换知识树中心保留同一棵树的布局、节点大小和连线位置，更新中心高亮及五层显示范围。
- 新增“设置”菜单，包含“MD加载样式”“图片保存方式”“关于 Tmd”；移除菜单按钮下箭头和菜单选项省略号。
- MD 样式设置分为五组折叠项，支持字体、六级标题、间距、缩进、引用、代码、链接和表格等；代码背景默认变浅，输入三个反引号提供语言提示和补全。
- 配置使用紧凑字段与按位开关，保存在十六进制 TXT 中，支持导入、导出及旧版格式读取；解析和存储位于 core。
- 独立图片保存窗口提供相对路径、绝对路径和图床三种方式，支持保存位置和命名图床链接管理。
- 提供可配置 multipart/form-data 上传接口、鉴权方式和返回链接字段；Token/Key 使用 Windows DPAPI 本机保护并从配置导出中省略。未调用真实图床服务，兼容性尚未验证。
- 本地回归检查 73 项通过，另有知识库初始化及菜单、设置窗口检查通过。

### English

- Third release, distributed as source code. No new installer or portable package is produced; v1.0.1 downloads remain available.
- Preserve the same tree's layout, card sizes and connector positions when changing the center; update the highlight and five-step visible scope.
- Add a Settings menu for Markdown loading styles, image storage and About Tmd, with menu button arrows and entry ellipses removed.
- Group Markdown appearance controls into five collapsible sections covering fonts, six heading levels, spacing, indentation, quotes, code, links and tables. Use lighter default code backgrounds and offer fenced-code language suggestions and completion.
- Store compact settings with bit-packed switches in hexadecimal TXT, with import/export and older-format compatibility; parsing and storage remain in core.
- Provide a separate image storage window for relative paths, absolute paths and named hosted-image links.
- Add a configurable multipart/form-data upload interface, authentication and returned-URL fields. Protect local Token/Key credentials with Windows DPAPI and omit them from exports. No real image hosting service has been tested.
- Pass 73 local regression checks, plus library initialization and focused menu/settings checks.

## v1.0.1 — 2026-10-05

- 左侧文件列表右键可重命名、删除所点击的 Markdown 文档；目录和空白区域可新建文件夹，支持空目录与嵌套目录。
- 文件夹删除会连同全部内容移入 Windows 系统回收站。文件菜单与右键删除统一使用回收站，失败不会永久删除。
- 相关知识树连线同步清理；未保存的当前文档可以先保存或取消操作，关联图片保留用于还原或共享引用。
- Added document context actions, folder creation and empty-folder display in the file sidebar.
- Document and recursive folder removal now use the Windows Recycle Bin, with no permanent-delete fallback.
- Preserve unrelated edits and shared images; synchronize relationship cleanup and protect unsaved current documents.

## v1.0.0 — 2026-10-04

### 中文

- 发布首个 Windows x64 安装版与便携版。
- 提供中英文安装向导、可选桌面/开始菜单快捷方式、应用列表登记、登记名称及安装目录选择。
- 安装版将个人知识库存放在系统“文档/Tmd”，保留已有文档，升级与卸载保留个人数据。
- 支持 Markdown 文档与原文编辑、图片管理、携带图片导出、目录及快速搜索。
- 支持五层知识树索引、双向学习关系、同级替代、嵌套详解与拖放整理。
- 提供五篇 `markdown学习` 示例和中英文使用、安装与构建文档。

### English

- First Windows x64 installer and portable release.
- Bilingual setup with installation directory, display name, optional desktop/Start menu shortcuts and Installed apps registration.
- Installed libraries live under Documents/Tmd; existing documents and user data survive upgrades and removal.
- Editable Markdown document/source views, images, image-bundled export, heading navigation and quick search.
- Five-step knowledge-tree indexing, reciprocal learning routes, shared alternatives, nested details and drag-and-drop relationships.
- Five starter documents and bilingual usage, installation and build documentation.
