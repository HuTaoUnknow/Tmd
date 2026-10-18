# Tmd 项目代码分类、分层与优化报告

> 实施更新：已开始将审查通过的草案接入项目。下面的源码清单、136 文件统计和初次验证记录保留为审查基线；当前接入范围与验证结果见 [IMPLEMENTATION.md](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/IMPLEMENTATION.md)。

评审日期：2026-10-18。对象是当前工作区，而非仅 Git 已提交版本；包含此前尚未提交的语法修复。代码基线提交为 `fc96f1d9fab5656c904b308b0d1994b1ccbddb4e`，版本声明为 1.1.0。

## 结论与交付

项目已有有效的核心/UI 边界，编辑规则、原子写入、路径保护和知识树增量刷新也有共用实现。优先调优点是：**收拢重复编辑分发，拆开文档渲染与输入状态，隔离管理器周围的文件设施，复用同一修订的解析结果。** 仅按名称重新分文件夹不能解决这些问题。

本轮完成全量源码/头文件清单与包含关系扫描，重点逐段核查编辑分发、渲染与映射、主窗口同步、管理器交易、知识索引及图片/配置边界。统计覆盖 136 个自有 C/C++ 文件、16,988 行，其中正式程序 127 个文件、测试 9 个文件；不含第三方 MD4C 源码、自动生成文件和私人知识库。包含依赖由脚本提取，主要调用路径另行核查；这不是完整的动态调用图或性能剖析。

| 交付物 | 内容 |
| --- | --- |
| [CURRENT-STRUCTURE.md](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/CURRENT-STRUCTURE.md) | 当前目录逐层展开，分别显示 `.h`、`.cpp` 和配套文件，并说明用途 |
| [TARGET-STRUCTURE.md](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/TARGET-STRUCTURE.md) | 调优目标目录逐层展开，每个现有文件的建议位置及新增类位置 |
| [CALL-GRAPHS.md](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/CALL-GRAPHS.md) | 当前构建/类调用/目录依赖环，以及草案与完整目标模式的 Mermaid 图 |
| [inventory.json](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/inventory.json) | 文件行数、哈希、职责、符号、include 证据和建议路径 |
| [模块化代码草案](D:/PC/Project/Qt/tree_md/proposals/modular-editing/README.md) | 4 个已生成、独立编译验证的模块及测试、CMake |
| [verification.json](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/verification.json) | 本次实际构建、检查结果及原源码未被替换的核对记录 |

原有应用源文件和本地知识库保留。目标结构是设计稿，生成的优化代码位于 `proposals/modular-editing`，没有声称整个项目已经完成迁移。

## 1. 当前核心代码分布与名称

表内文件数量包括头文件和实现，行数包括空行与注释。

| 当前目录 | 文件 / 行数 | 核心名字与作用 |
| --- | --- | --- |
| `core/markdown` | 32 / 2,961 | `MarkdownAnalysis` 区域扫描；`MarkdownBlockSyntax` 输入前缀/空行；`MarkdownSyntax` 语义；`MarkdownParser` 导航查询；4 个 Editing 规则模块；`MarkdownSourceMap`/`MarkdownSpelling` 映射和原文展开；`MarkdownImages`/`MarkdownCodeBlocks`/`LanguageCatalog`/`CodeSyntax` |
| `core/library` | 16 / 1,295 | `MarkdownManager` 文档与关系状态；`MarkdownNode` 文档记录和目录缓存；操作、移动、导入实现；`MarkdownFileIO`、`FileWatcher`、`LibraryPathPolicy`、`DefaultLibrary` |
| `core/knowledge` | 8 / 486 | `NodeRelationType`、`MarkdownRelation`、`KnowledgeIndex`、`KnowledgeTopology`；以及 Manager 关系成员实现 |
| `core/images` | 10 / 525 | `ImageStorage` 资源保存/迁移；`ImageLoader` 读取解码；`ImageUploadJob` 上传；`ImageInsertJob` 插图编排；`MarkdownExportJob` 携图导出 |
| `core/settings` | 7 / 312 | `MarkdownSettings` 数据；`MarkdownSettingsCodec` 编解码；`MarkdownSettingsStore` 持久化缓存；`KnowledgeViewSettings` 视图设置 |
| `core/platform` | 4 / 66 | `LocalCredentials` 本机凭据保护；`FileManager` 系统目录打开/定位 |
| `ui/document` | 19 / 2,490 | `MarkdownDocumentView`、`MarkdownEditor`；`InlineMarkupDecoration`、`CodeBlockDecoration`、`CodeSyntaxHighlighter`；补全、视口、字体及 MIME 输入适配 |
| `ui/library` | 8 / 778 | `DocumentSidebar`、`DocumentFileTree`、`DocumentTreeBuilder`、`OutlineSidebar` |
| `ui/knowledge` | 12 / 1,086 | `KnowledgeGraphWidget`、`KnowledgeTreeCanvas`、`KnowledgeNodeWidget`、`KnowledgeTreeLayout`、`KnowledgeTreeRouter`、`RelationDialog` |
| `ui/settings` | 4 / 352 | `MarkdownSettingsDialog`、`ImageSettingsDialog` |
| `ui/common` | 4 / 126 | 主题与程序图标函数 |
| `ui/MainWindow.h/.cpp` | 2 / 774 | 主窗口、当前文档、两种视图、undo 光标、文件操作、图片作业和布局编排 |
| `main.cpp` | 1 / 116 | 参数解析、知识库初始化、CLI 和 GUI 入口 |
| `tests` | 9 / 5,621 | 综合回归、核心回归、知识树集成、初始化及五类编辑规则审查 |

注意文件名与类的区别：`MarkdownDocumentOperations.cpp`、`MarkdownPathMoves.cpp`、`LibraryImport.cpp`、`MarkdownManagerRelations.cpp` 都在实现 **MarkdownManager 同一个类**；`MarkdownDirectoryMove.cpp` 在实现 **MarkdownFileIO::renameDirectory**。不能把它们误计为独立类。`MarkdownOutline.h` 和 `MarkdownEdit.h` 是数据定义；多个功能模块实际为命名空间/函数。

## 2. 同类操作如何归并

| 操作类别 | 当前分布 | 应共用的部分 | 应保留的差异 |
| --- | --- | --- | --- |
| 行区间、语法和原文偏移 | Analysis、BlockSyntax、Syntax、Parser、SourceMap、TableEditing | 原始 UTF-16 坐标、同一文本/模式的分析上下文 | Raw 与 Rendering；原文与带临时标记的 renderSource；MD4C 不同事件消费者 |
| Enter / Tab / Backspace / Delete | MarkdownEditor 与 MarkdownDocumentView 分别编排，调用 4 个 Editing 模块 | 命令数据、处理优先级、统一 MarkdownEdit 返回值 | 文档模式自动分隔、表格上下文、隐藏标记、可视折行、IME |
| 将新原文写入 Qt undo | MainWindow::sourceEdited 回调、MarkdownEditor::keyPressEvent 局部 apply | 前后缀最小替换、UTF-16 边界、一次 QTextCursor 交易 | 主窗口还保存文档模式交易前后光标，不能删掉这层记录 |
| 渲染和映射重建 | DocumentView::setMarkdownSource 与 SourceMap | 同修订的分析/语义、输入/输出阶段契约 | Qt 导入后的表格占位、空白恢复、图片尺寸及装饰属于 UI 适配 |
| 保存、改名、移动、导入 | Manager 多个实现文件，图片和关系配合回滚 | 预检→准备资源→写入/迁移→关系提交→发布变更的交易流程 | 单文件/批量/目录的部分失败、目录身份及 Windows 占用恢复 |
| 图片保存、插入、上传、导出 | Storage、Loader、InsertJob、UploadJob、ExportJob | 路径/哈希去重、读取解码、取消与作业身份 | 导出才改相对引用；上传不可当作本地文件复制；网络异步不等于 CPU 后台线程 |
| 知识节点与几何表现 | Relation、Index、Topology 与 Canvas/Layout/Router | 关系规则、只读索引输入和稳定拓扑 | 矩形布局、绘制、命中和 Qt 图形项生命周期留在 UI |
| 配置数据、编码、存储、界面 | Settings.h/Codec/Store/Dialog | 设置值、版本化编解码和明确的快照传递 | 本机凭据及文件 IO 留在设施；弹窗与提示留在 UI |

三行的局部 `fail()`、简单行首/行尾辅助不应一概搬进“全局工具类”。只有语义、坐标和生命周期确实一致的操作才合并；通用化不能损害已确认的输入规则。

## 3. 重点问题与证据

这里的优先级表示重构价值与接入风险，不表示已经证实存在新的用户可见故障。

### P1：文档视图同时持有多条状态链

[MarkdownDocumentView.cpp](D:/PC/Project/Qt/tree_md/ui/document/MarkdownDocumentView.cpp:122) 共 1,291 行。`setMarkdownSource` 在第 122～503 行处理规范化、图片、临时标记、Qt 导入、表格锚点、映射、空白恢复和装饰；`keyPressEvent` 在第 968～1250 行处理表格、源码选区、隐藏标记和多类编辑。

风险是一个输入规则改动同时影响渲染、光标和原文。建议依次提取 `MarkdownRenderPipeline`、`DocumentSelectionController`、`DocumentImagePresenter`，让视图只承担 Qt 事件入口和最终呈现。多种布尔状态不是天然互斥，不能机械改成一个枚举；先记录“允许重建/正在映射/用户选区”等状态组合，再缩小它们的作用域。

### P1：两种视图重复编排编辑优先级

原文入口在 [MarkdownEditor.cpp:100](D:/PC/Project/Qt/tree_md/ui/document/MarkdownEditor.cpp:100)，文档入口在 [MarkdownDocumentView.cpp:1070](D:/PC/Project/Qt/tree_md/ui/document/MarkdownDocumentView.cpp:1070) 和 [MarkdownDocumentView.cpp:1120](D:/PC/Project/Qt/tree_md/ui/document/MarkdownDocumentView.cpp:1120)。两边都串联 Code/List/Paragraph/Prefix/Table，但退格顺序、文档分隔、表格和隐藏格式末尾的光标处理存在有意差异。

本轮生成 `EditCommandRouter`，将共用交易分发移入 application。模式和特殊上下文显式输入，保留已有顺序；返回实际处理器，便于定位规则由谁接管。图片两步删除、格式符号逐字移动、鼠标框选及 IME 暂不塞进核心路由。

### P1：文本差分与 Qt 交易重复

[MainWindow.cpp:172](D:/PC/Project/Qt/tree_md/ui/MainWindow.cpp:172) 与 [MarkdownEditor.cpp:73](D:/PC/Project/Qt/tree_md/ui/document/MarkdownEditor.cpp:73) 都计算公共前后缀，再通过 QTextCursor 替换一段原文。

本轮提取 `TextPatch` 与 `QtTextEditAdapter`。差分算法仍为线性扫描，不宣称算法提速；价值是只维护一个实现、避免合法代理对被拆成半个字符传给 Qt、拒绝过期交易，并保证一次 undo。主窗口现有的前后光标历史仍需由后续 Session Controller 保存。

### P1：重建流程反复分析同一份原文

文档视图第 149～151 行已经复用 analysis，这部分应保留。重复主要存在于跨方法边界：SourceMap 第 56～57 行重新分析，第 174～175 行的空白查询再次分析；文档视图第 387、413、463 行可能在同一次重建中多次调用 SourceMap::rebuild。[CodeSyntaxHighlighter.cpp:24](D:/PC/Project/Qt/tree_md/ui/document/CodeSyntaxHighlighter.cpp:24) 在源码变化后也重新扫描围栏。

本轮 `MarkdownSnapshot` 将一次指定模式的原文与惰性结果放在一起，复用现有传入 analysis 的重载。后续需要为 SourceMap/表格分析增加明确接入点，才会减少这些跨方法重复扫描。不同的 renderSource 必须新建快照。没有真实大文档延迟/内存剖析，不能承诺整体速度提升比例，也不提前引入完整 AST 或多线程。

### P2：目录层次与实际依赖不完全一致

扫描到 `core/images → core/library → core/images`、`library ↔ knowledge`，以及 `images → settings → library` 的目录包含环。当前同在一个静态库中，可以正常构建；问题是未来难以独立测试/替换和拆成小目标。

具体边界：`KnowledgeIndex::build` 接受 Manager，改为读取 `KnowledgeGraphSnapshot`；`MarkdownRelation` 同时做规则和 JSON 文件读写，后者提取 `RelationFileStore`；通用文件 IO/监听/目录占用逻辑迁到 infrastructure；Manager 保留为 application 的单一状态拥有者。先改接口再搬文件。

### P2：主窗口的业务编排与布局混在一起

[MainWindow.cpp](D:/PC/Project/Qt/tree_md/ui/MainWindow.cpp:47) 共 675 行，实现构造和信号装配、响应式布局、文件操作、双视图同步、保存确认、图片作业和导出。构造函数约 213 行，代码较紧凑，单看行数会低估职责数量。

后续 `DocumentSessionController` 只接管当前文档、原文 undo、预览交易光标和刷新调度，不再持有另一套节点集合；`MainWindowLayout` 接管布局；现有 `ImageInsertJob` 已抽离图片业务，应继续复用。消息框与用户选择仍由窗口/对话框提供。

### P2：配置声明和全局取值增加耦合

`MarkdownSettings.h` 同时声明设置数据和 Store；`MarkdownSettingsStore.cpp` 又依赖 `core/library/MarkdownFileIO.h`。建议拆开声明、将 Store 放设施目录，并优先给长作业传入不可变设置值。`ImageInsertJob` 已经捕获设置，`ImageStorage` 也允许显式图片目录，这些现成能力应直接沿用，避免再造封装层。

### P3：测试与构建目录可进一步对齐职责

`TreeMdTests.cpp` 有 3,524 行，涵盖多个子系统。已有 `RegressionGroups.cmake` 保证综合用例分组注册，应保留。后续按库交易、图片、文档交互、知识树和设置拆出夹具；不是删除测试或把一个断言复制到每一层。本轮草案用独立 CMake 明确注册，不改变现有 14 组的定义。

## 4. 建议的层级及目录

完整逐文件结构在两份目录文档中，下图为层级骨架：

```text
tree_md/
├── src/
│   ├── bootstrap/                 # main、CLI、实例装配
│   ├── application/
│   │   ├── editing/               # 编辑命令路由
│   │   ├── documents/             # 文档会话协调
│   │   ├── library/               # Manager 状态与文档/关系交易
│   │   └── images/                # 插入、导出作业编排
│   ├── core/
│   │   ├── markdown/
│   │   │   ├── analysis/          # 分析、快照和导航数据
│   │   │   ├── syntax/            # 语法、语言与格式识别
│   │   │   ├── editing/           # 纯原文编辑规则与交易值
│   │   │   └── mapping/           # 原文/显示坐标算法
│   │   ├── documents/             # MarkdownNode
│   │   ├── knowledge/             # 关系规则和只读图索引
│   │   └── settings/              # 配置数据与范围
│   ├── infrastructure/
│   │   ├── filesystem/            # IO、路径约束、监听、目录移动、初始化
│   │   ├── persistence/           # 关系文件读写
│   │   ├── images/                # 资源存储、读取解码、HTTP 上传
│   │   ├── settings/              # 编解码、持久化、视图偏好
│   │   └── platform/              # Windows/系统能力
│   └── ui/
│       ├── shell/                 # 主窗口与布局
│       ├── document/              # Qt 事件/渲染/选择/装饰/适配
│       ├── library/               # 文件列表与目录
│       ├── knowledge/             # 知识树视图及几何
│       ├── settings/              # 配置对话框
│       └── common/                # 图标与主题
├── tests/                         # core、infrastructure、integration、ui
├── cmake/                         # 明确的库目标及层边界
├── third_party/                   # 固定版本原件
├── resources/                     # 图标、字体和 Qt/Windows 资源
├── examples/                      # 可公开打包的示例
├── scripts/、installer/、licenses/、docs/
└── md_data/、md_photo/             # 现有私人库位置保留，不搬进 src
```

目标构建可以分为 foundation、infrastructure、application、ui、executable 五部分。foundation 可继续使用必要的 Qt Core/Gui 值类型与文本能力，不能称为无 Qt。Widgets 只进入 UI；Network 和平台链接归设施。若保留 MD4C 适配在 core/syntax，Qt 渲染特征与语义标志要在接口处说明，不让 QTextBrowser 进入核心。

## 5. 已生成的优化类及接入边界

| 实际代码 | 输入 → 输出 | 当前验证与边界 |
| --- | --- | --- |
| [TextPatch.h](D:/PC/Project/Qt/tree_md/proposals/modular-editing/core/markdown/editing/TextPatch.h) / `.cpp` | before/after → 单段替换 | Unicode、空文本、CRLF、500 组固定种子随机序列；不是通用合并器 |
| [MarkdownSnapshot.h](D:/PC/Project/Qt/tree_md/proposals/modular-editing/core/markdown/analysis/MarkdownSnapshot.h) / `.cpp` | 不可变 source + Raw/Rendering → 惰性查询 | 多次读取只各构建一次；模式有别；仅 GUI 单线程惰性缓存 |
| [EditCommandRouter.h](D:/PC/Project/Qt/tree_md/proposals/modular-editing/application/editing/EditCommandRouter.h) / `.cpp` | 命令 + 模式 + 源码选区/表格上下文 → MarkdownEdit + 处理器 | 保留现有优先级、任务续写、段落分隔和整表操作；8 组与真实 MarkdownEditor 对照 |
| [QtTextEditAdapter.h](D:/PC/Project/Qt/tree_md/proposals/modular-editing/ui/document/QtTextEditAdapter.h) / `.cpp` | 当前原文 + 交易 → 一次 Qt 替换与选区 | undo/redo、只移光标不添 undo、过期交易拒绝；只应用于原文文档 |

未生成的 `DocumentSessionController`、`MarkdownRenderPipeline`、`RelationFileStore` 等仅为下一阶段设计，已在目标目录标注。没有用空类冒充完成的重构。

## 6. 分阶段实施和验收

| 阶段 | 具体工作 | 必须满足的验收 |
| --- | --- | --- |
| 0，本轮 | 目录/调用审查，4 模块代码草案，独立编译与比较 | 当前源码不被替换，草案及相关基线检查通过 |
| 1 | 先将两处最小替换接到 TextPatch/QtAdapter，再接命令路由 | 单次 undo、前后光标、8 类对照扩展为完整编辑矩阵；14 组回归全通过 |
| 2 | 提取渲染阶段、选择和图片呈现；让同一修订共享快照 | N55 空行、表格后正文、代码围栏、中文 IME、框选和可视折行维持既有规则；大文档实测后再判断性能 |
| 3 | 拆知识图只读输入、关系存储和文件设施，再迁目标目录 | 保存冲突、Windows 占用/失败回滚、图片和关系不丢失；CMake 与 include 边界检查通过 |
| 4 | 收窄 MainWindow、分离配置声明、拆测试夹具和构建目标 | 数据/路径/配置兼容；完整回归、Windows 原生交互复验，明确发布后再打包 |

阶段 1 的风险小于直接移动全部目录；阶段 2/3 都涉及已修复过的精确光标和文件恢复行为，应单独提交/验证，不能与语法规则变更混在一次提交里。每步保留独立回退点。

## 7. 本轮实际验证

- 当前 Debug 全目标构建成功。
- 当前基线三组回归成功：`tmd_markdown_core`、`tmd_incremental_refresh`、`tmd_editing_policy`，合计 6.58 秒；不是把历史 14 组结果当作本次结果。
- 草案独立 Debug 构建成功，包含真实 core/ui 的重新编译。
- 草案 QtTest **20 项通过、0 失败、0 跳过**，包含初始化/清理；18 项功能/数据行中有 8 项与真实原文编辑器对照。Unicode 语料额外进行 169 对替换与 500 组随机替换断言。
- 草案 Qt undo/redo、过期原文拒绝、只移动光标、任务/标题/列表/表格及 Raw/Rendering 区分检查通过。
- 扫描快照用于核对当前 main/core/ui/tests 的 136 份源文件，确保本轮没有替换它们；细节见 verification.json。

这些检查证明草案的构建与已覆盖交易行为，不证明已经接入全部文档视图，也不等于完成 Windows 原生 IME/字体画面验收。Offscreen 检查有一条字体加载调试信息，未导致检查失败。实际编辑速度、峰值内存和全量跨视图行为留到集成阶段测量。本轮不发布、不更新本地安装版，也不自动启动应用。
