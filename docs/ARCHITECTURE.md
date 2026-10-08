# Tmd 项目结构

## 模块与边界

`tree_md_core` 依赖 Qt Core、Gui 和 Network，不依赖 Widgets。它处理原文、文档状态、路径、关系、图片和配置。`tree_md_ui` 负责 Qt 控件、交互、排版及绘图，调用核心操作完成保存和导入。

| 目录 | 职责 |
| --- | --- |
| `core/markdown` | Markdown 区域分类、标题与图片语法、原文位置映射、语言列表 |
| `core/library` | 文档集合与状态、路径约束、原子文件操作、文档交易、批量导入、文件监听 |
| `core/knowledge` | 关系角色、双向及同级规则、持久化、范围索引、稳定拓扑 |
| `core/images` | 图片读取、复制和路径改写、插入、上传、携图导出 |
| `core/settings` | 设置数据与范围、十六进制配置编解码、持久化缓存、共享知识树缩放 |
| `core/platform` | Windows 本机凭据保护 |
| `ui/document` | 文档与原文视图、排版、高亮、语言补全、图片输入事件 |
| `ui/library` | 文件侧栏、快速搜索、目录列表构建和过滤、正文目录 |
| `ui/knowledge` | 几何布局、连线路由、卡片绘制、画布和完善知识树窗口 |
| `ui/settings` | MD 加载样式和图片保存方式设置窗口 |
| `ui/common` | 主题、圆角弹窗和 Tmd 图标 |

`MarkdownManager` 是文档集合和关系状态的唯一持有者。集合扫描与状态更新、文档交易、关系修改分别实现于 `MarkdownManager.cpp`、`MarkdownDocumentOperations.cpp`、`MarkdownManagerRelations.cpp`，避免拆出多个互相复制状态的管理器。

## 共用的处理入口

- `MarkdownAnalysis::scan` 提供原始 UTF-16 偏移下的代码块、行内代码、HTML 与注释范围。目录、图片发现、代码块渲染和原文映射共用这些结果。代码示例和注释中的图片不参与导入和改写。
- `LibraryPathPolicy` 共用文件及目录的库内路径校验，检查最近存在的父目录，防止目录链接让路径越过知识库。
- `MarkdownManager::copyDocument` 统一另存为及图片迁移交易。`importDirectory` 保留逐文档错误和部分成功，完成整批后统一扫描，避免每导入一篇就重新读取全部文档。
- `ImageInsertJob` 负责本地、剪贴板、远程与图床图片的处理，开始时捕获目标文档和图片保存配置。主窗口负责进度、错误展示和编辑光标，不再包含图片文件复制和上传后的链接管理逻辑。
- `DocumentTreeBuilder` 供左侧文件列表和完善知识树的未显示文档列表共用，统一目录、文档元数据、标题更新及搜索过滤。

## 刷新与缓存

`documentChanged` 表达内容或保存状态变化，`documentTitleChanged` 只表达标题变化，`documentsChanged` 表达集合刷新，`relationsChanged` 表达知识关系变化。关系修改无需重建左侧文件列表。

每个文档按内容缓存目录和标题。知识树使用集合路径及关系的修订号判断是否需要重建；正文与标题变化保留卡片和连线，标题仅更新文字和提示。切换中心仍使用完整关联树计算稳定布局，再显示五层范围。

`MarkdownSourceMap` 在重建或增量编辑后一次性索引行内代码的显示范围，光标查询使用二分搜索；普通插入无需遍历全部格式容器进行删除判断。文件监听的路径集合在一次扫描结束时统一恢复，包括扫描失败时。

## 配置与平台实现

`MarkdownSettings.cpp` 定义设置范围和描述，`MarkdownSettingsCodec.cpp` 负责验证、版本兼容和十六进制编码，`MarkdownSettingsStore.cpp` 负责文件读写及缓存。便携导出依旧不带本机图床 Token，Windows 凭据保护归 `LocalCredentials`，原有配置格式保持兼容。

## 构建和检查

CMake 显式列出参与编译的源文件。新增源文件需同时加入对应目标；不要用目录通配自动纳入遗留实现。旧空白 `mainwindow.ui` 与未参与编译且无法独立编译的 `MarkdownOutline.cpp` 已清理。

`tmd_markdown_tests` 检查核心解析、图片导入、原文映射、批量导入和另存为；`tmd_refactor_tests` 检查知识树卡片在正文及标题修改后保持存在；`tmd_library_tests` 检查示例初始化。原综合测试程序仍可直接完整运行，CTest 按文档库、图片、编辑器、知识树、文件界面及设置分组执行。配置阶段检查每个综合测试已被登记，避免新增测试遗漏。

```powershell
cmake --build <Debug 构建目录> --parallel 4
ctest --test-dir <Debug 构建目录> --output-on-failure
```

各组默认按顺序运行，使用临时知识库和隔离配置；Qt DLL 与平台插件需在运行环境可用。原生弹窗键盘检查另用 Windows 平台复验。

尚未统一为完整 Markdown AST，也未将所有图片处理改成后台线程；这两项应由后续大文档及大图实测决定。当前高亮规则保持原有行为，语言分类扩展可使用共享 `LanguageCatalog`。真实图床 API 未接入验证。
