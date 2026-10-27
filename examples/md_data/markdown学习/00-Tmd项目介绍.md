# Tmd 项目介绍

Tmd 是一个用 C++17 和 Qt Widgets 开发的 Windows 桌面 Markdown 知识库。它把真实的 `.md` 文件作为内容来源，在可编辑的“文档”视图和完整的“原文”视图之间切换，让写作者兼顾阅读观感与 Markdown 原文。

文件夹负责整理文档，知识树负责连接知识：前后节点表示学习顺序，上节点表示可替换的知识，下节点用于补充详解、实例和实践。文档、图片和关系保存在本地，可以自行查看、迁移和备份。

## 1. 项目提供什么

- **写作与阅读**：编辑标题、正文、列表、引用、代码块、表格、链接和图片，支持标题目录、查找替换和撤销重做。
- **文档管理**：在侧栏新建、重命名、移动和删除文档，记录未保存状态，检查链接和图片是否缺失。
- **知识连接**：从当前文档查看学习路线、替代方案和下层说明，按内容关系组织知识树。
- **图片与导出**：统一管理本地图片，导出时携带图片并调整导出副本中的引用地址。
- **本地使用**：编辑本地知识库无需账号；网络链接、网络图片等功能按需访问网络。

## 2. 源码目录与职责

以下结构以项目根目录为起点，列出主要目录及关键入口。同名的 `.h` 通常声明接口和数据类型，`.cpp` 实现具体逻辑。

```text
tree_md/
├─ main.cpp                         程序入口：创建应用、解析启动参数、初始化知识库和主窗口
├─ CMakeLists.txt                   构建入口：登记源文件、模块依赖、测试与安装内容
│
├─ core/                            核心层：处理 Markdown、文件、关系和配置，不依赖界面控件
│  ├─ markdown/                     Markdown 语义与编辑规则
│  │  ├─ analysis/                  MarkdownSnapshot：复用同一份原文的解析查询
│  │  ├─ editing/                   TextPatch：表达一次原文替换，供编辑交易共用
│  │  ├─ MarkdownSyntax.*           标准语义识别，向其他模块提供一致的解析结果
│  │  ├─ MarkdownBlockSyntax.*      Tmd 的块语法、输入前缀和结构空行规则
│  │  ├─ MarkdownSourceMap.*        文档显示位置与 Markdown 原文位置之间的映射
│  │  ├─ Markdown*Editing.*         段落、列表、表格、代码和选区的原文修改规则
│  │  ├─ MarkdownParser.*           提取标题目录等结构信息
│  │  ├─ MarkdownReferences.*       识别链接、图片和引用定义中的真实地址
│  │  ├─ MarkdownImport.*           导入与粘贴时的语法整理
│  │  └─ LanguageCatalog.* / CodeSyntax.*  代码语言、别名及语法着色规则
│  ├─ library/                      文档库、文件状态和磁盘操作
│  │  ├─ MarkdownManager.*          文档集合与关系状态的统一管理入口
│  │  ├─ MarkdownNode.*             单篇文档的路径、正文、标题和修改状态
│  │  ├─ MarkdownDocumentOperations.cpp  新建、保存、重命名、删除等文档交易
│  │  ├─ MarkdownPathMoves.cpp      文件及文件夹移动，协调引用与关系更新
│  │  ├─ MarkdownReferenceMaintenance.cpp  文件变化后同步相关地址，保留未保存编辑
│  │  ├─ MarkdownFileIO.*           UTF-8 文件读写和原子保存
│  │  ├─ LibraryPathPolicy.*        知识库路径边界和名称约束
│  │  ├─ DocumentLinkResolver.*     区分库内 MD、本地其他文件和网络链接
│  │  ├─ LibraryFileIndex.*         记录文件身份，辅助识别移动与缺失
│  │  ├─ FileWatcher.*             监听文件系统变化，通知文档库刷新
│  │  └─ DefaultLibrary.*          首次使用时初始化公开示例，保留已有个人知识库
│  ├─ knowledge/                    知识关系的数据与规则
│  │  ├─ MarkdownRelation.*         关系角色和关系文件读写
│  │  ├─ MarkdownManagerRelations.cpp  同一管理器中的关系增删及一致性维护
│  │  └─ KnowledgeIndex.* / KnowledgeTopology.*  关系范围查询与稳定拓扑
│  ├─ images/                       图片加载、存储、插入、上传与携图导出
│  ├─ settings/                     设置定义、范围校验、配置编解码和持久化
│  └─ platform/                     系统能力封装，如文件管理器定位和本机凭据保护
│
├─ application/                     应用层：协调可复用的操作流程
│  └─ editing/
│     └─ EditCommandRouter.*        文档与原文共用的按键命令、上下文和规则分发顺序
│
├─ ui/                              界面层：Qt 控件、鼠标键盘交互、排版和绘图
│  ├─ MainWindow.*                  主窗口：组合侧栏、编辑区、菜单和文档切换流程
│  ├─ document/                     文档与原文编辑界面
│  │  ├─ MarkdownDocumentView.*     可编辑的文档排版视图
│  │  ├─ MarkdownEditor.*           Markdown 原文编辑器
│  │  ├─ QtTextEditAdapter.*        将核心修改应用到 Qt 文档，维护选区和撤销交易
│  │  ├─ MarkdownSelectionController.*  选区包裹等输入过程中的界面状态
│  │  ├─ *Decoration.*              行内语法、列表标记和代码块的视觉装饰
│  │  ├─ DocumentFindDialog.*       查找、替换窗口及匹配项导航
│  │  └─ MarkdownTypography.* / MarkdownViewport.*  字体排版与视口定位
│  ├─ library/                      文档侧栏、文件树、筛选搜索和标题目录
│  ├─ knowledge/                    知识树画布、节点卡片、布局、连线和关系操作窗口
│  ├─ settings/                     MD 加载样式、图片保存方式的设置窗口
│  └─ common/                       共用主题、弹窗样式与应用图标
│
├─ resources/                       图标、字体和 Qt 资源清单，以及 Windows 版本资源
├─ third_party/
│  └─ md4c/                         第三方 Markdown 解析器及来源说明
├─ examples/                        随项目公开分发的示例知识库
│  ├─ md_data/                      示例 Markdown 文档及示例知识关系
│  └─ md_photo/                     示例使用的图片
├─ tests/                           核心与界面回归测试，使用临时知识库和隔离配置
│  └─ RegressionGroups.cmake        综合测试的分组和登记
├─ docs/                            使用、安装、构建、架构与验证资料
│  ├─ ARCHITECTURE.md               模块边界、关键实现与调用说明
│  ├─ BUILDING.md                   开发环境、构建、测试与打包方法
│  ├─ 使用教程.md / USER_GUIDE.md   中文及英文使用指南
│  ├─ INSTALLATION.md              安装、升级、备份和迁移说明
│  ├─ images/                      项目文档插图
│  ├─ releases/                    各版本介绍
│  ├─ syntax-review/               写入语法约定与审查记录
│  ├─ optimization-review/         代码结构分析和优化记录
│  └─ verification/                实际检查结果与验证记录
├─ proposals/                       模块化方案原型，用于说明和独立验证设计
├─ scripts/
│  ├─ Start-Debug.ps1               桌面 Debug 快捷方式调用的启动入口
│  ├─ Start-Tmd.cmd                 便携版启动入口
│  └─ Build-Release.ps1             Release 构建、运行库收集及发布产物准备
├─ installer/
│  ├─ Tmd.iss                      Windows 安装包配置
│  └─ languages/                   安装器语言文件
├─ .github/workflows/               GitHub 自动构建与测试流程
├─ licenses/                        随分发提供的第三方许可文本
├─ README.md                        项目首页：功能、安装和开发入口
├─ CHANGELOG.md                     版本与修改记录
├─ AGENTS.md                        项目协作、环境与验证约定
├─ LICENSE                          Tmd 自身的开源许可
└─ THIRD_PARTY_NOTICES.md            Qt、字体、解析器等第三方组件的许可说明
```

`MarkdownManager` 的部分实现按职责放在不同 `.cpp` 中，但仍由同一个管理器持有文档和关系状态，避免多个模块各自保存一份互相矛盾的数据。`proposals/` 中的原型用于设计说明，主程序以根目录 `CMakeLists.txt` 登记的源文件为准。

## 3. 各层怎样配合

```text
用户在文档或原文中按键
  → ui：识别控件、光标、选区和输入上下文
  → application/editing：选择应执行的编辑规则
  → core/markdown：计算原文修改和新的选区位置
  → ui/document：应用修改，更新显示与撤销记录

用户保存、移动文档或修改关系
  → ui/MainWindow / 侧栏 / 知识树：发出操作请求
  → core/library / core/knowledge：校验并修改文档与关系状态
  → core/library：读写文件，维护路径引用
  → ui：接收变化通知，刷新列表、正文和知识树
```

核心层决定数据和语法规则，应用层统一共用编辑流程，界面层负责具体操作与显示。读取目录、查询文档或保存设置等操作可以直接调用对应核心接口，无需都经过编辑命令路由。

## 4. 知识库与生成目录

运行数据和源码分开管理。下面的 `md_data`、`md_photo` 位于所选知识库根目录中；安装版与 Debug 可以使用不同的知识库根目录。

```text
知识库根目录/
├─ md_data/                         用户 Markdown 文档
│  ├─ markdown学习/                项目介绍与入门教程
│  ├─ .tree-md-relations.json      文档间的知识关系
│  └─ .tree-md-files.json          运行时文件记录，用于追踪引用目标变化
└─ md_photo/                        统一保存的本地图片

项目根目录/
├─ build*/                          编译中间文件、Debug / Release 程序及测试日志
├─ dist/                            发布包及其他生成产物
└─ package/                         打包暂存内容
```

备份知识库时应一起保留文档、关系文件和图片。项目根目录的个人 `md_data/`、`md_photo/` 及生成目录不提交到 GitHub；公开分发的入门资料来自 `examples/`。

## 5. 从哪里开始阅读

本篇项目介绍位于目录首位，下面四篇教程继续沿用已有的学习路线：

```text
md语法基础 → tmd输入指南 → tmd知识树构建
    └─ md语法详解与正文对照
```

- [md语法基础](md语法基础.md)：先了解标题、代码块、列表和表格。
- [md语法详解与正文对照](md语法详解与正文对照.md)：按类别查看完整语法、原文实例和文档表现。
- [tmd输入指南](tmd输入指南.md)：按语法顺序了解 Tmd 中的输入、换行、退格和导航。
- [tmd知识树构建](tmd知识树构建.md)：用四篇教程演示学习路线和详解节点。

源码、问题反馈和版本下载见 [Tmd GitHub 仓库](https://github.com/HuTaoUnknow/Tmd)。
