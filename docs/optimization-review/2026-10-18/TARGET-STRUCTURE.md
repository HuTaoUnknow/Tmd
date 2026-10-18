# 调优目标目录及逐文件职责

这是目标设计，不是已迁移结构。现有文件的建议位置逐个映射；已生成草案位于 proposals/modular-editing。标记“后续建议”的类尚未生成。改目录须同步 CMake、include、资源和测试，不可只搬文件。

```text
tree_md/
├── .github/
│   └── workflows/
│       └── build.yml  # GitHub Actions Windows 构建和回归流水线
├── cmake/
│   └── Targets.cmake  # 后续建议：显式定义 foundation/infrastructure/application/ui 构建边界
├── docs/
│   ├── images/
│   │   ├── document.png  # 已有项目文档或语法审查资料
│   │   └── knowledge-tree.png  # 已有项目文档或语法审查资料
│   ├── syntax-review/
│   │   ├── assets/
│   │   │   └── sample.svg  # 已有项目文档或语法审查资料
│   │   ├── previews/
│   │   │   ├── S01.png  # 已有项目文档或语法审查资料
│   │   │   ├── S02.png  # 已有项目文档或语法审查资料
│   │   │   ├── S03.png  # 已有项目文档或语法审查资料
│   │   │   ├── S04.png  # 已有项目文档或语法审查资料
│   │   │   ├── S05.png  # 已有项目文档或语法审查资料
│   │   │   ├── S06.png  # 已有项目文档或语法审查资料
│   │   │   ├── S07.png  # 已有项目文档或语法审查资料
│   │   │   ├── S08.png  # 已有项目文档或语法审查资料
│   │   │   ├── S09.png  # 已有项目文档或语法审查资料
│   │   │   ├── S10.png  # 已有项目文档或语法审查资料
│   │   │   ├── S11.png  # 已有项目文档或语法审查资料
│   │   │   ├── S12.png  # 已有项目文档或语法审查资料
│   │   │   ├── S13.png  # 已有项目文档或语法审查资料
│   │   │   ├── S14.png  # 已有项目文档或语法审查资料
│   │   │   ├── S15.png  # 已有项目文档或语法审查资料
│   │   │   ├── S16.png  # 已有项目文档或语法审查资料
│   │   │   ├── S17.png  # 已有项目文档或语法审查资料
│   │   │   ├── S18.png  # 已有项目文档或语法审查资料
│   │   │   ├── S19.png  # 已有项目文档或语法审查资料
│   │   │   ├── S20.png  # 已有项目文档或语法审查资料
│   │   │   ├── S21.png  # 已有项目文档或语法审查资料
│   │   │   ├── S22.png  # 已有项目文档或语法审查资料
│   │   │   ├── S23.png  # 已有项目文档或语法审查资料
│   │   │   ├── S24.png  # 已有项目文档或语法审查资料
│   │   │   ├── S25.png  # 已有项目文档或语法审查资料
│   │   │   ├── S26.png  # 已有项目文档或语法审查资料
│   │   │   ├── S27.png  # 已有项目文档或语法审查资料
│   │   │   ├── S28.png  # 已有项目文档或语法审查资料
│   │   │   ├── S29.png  # 已有项目文档或语法审查资料
│   │   │   └── S30.png  # 已有项目文档或语法审查资料
│   │   ├── catalog.json  # 已有项目文档或语法审查资料
│   │   ├── FINDINGS.md  # 已有项目文档或语法审查资料
│   │   ├── IMPLEMENTATION.md  # 已有项目文档或语法审查资料
│   │   ├── OPEN-QUESTIONS.md  # 已有项目文档或语法审查资料
│   │   ├── README.md  # 已有项目文档或语法审查资料
│   │   ├── render-observations.json  # 已有项目文档或语法审查资料
│   │   ├── RULES.md  # 已有项目文档或语法审查资料
│   │   └── verification.json  # 已有项目文档或语法审查资料
│   ├── ARCHITECTURE.md  # 已有项目文档或语法审查资料
│   ├── BUILDING.md  # 已有项目文档或语法审查资料
│   ├── EDITING_AUDIT.md  # 已有项目文档或语法审查资料
│   ├── INSTALLATION.md  # 已有项目文档或语法审查资料
│   ├── SETTINGS_FORMAT.md  # 已有项目文档或语法审查资料
│   ├── USER_GUIDE.md  # 已有项目文档或语法审查资料
│   └── 使用教程.md  # 已有项目文档或语法审查资料
├── examples/
│   ├── md_data/
│   │   ├── markdown学习/
│   │   │   ├── md语法基础.md  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   │   │   ├── md语法详解.md  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   │   │   ├── md语法详解示例对照.md  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   │   │   ├── tmd作用及其意义.md  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   │   │   └── 个人知识树示例.md  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   │   └── .tree-md-relations.json  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
│   └── md_photo/
│       └── markdown学习/
│           └── 学习闭环.png  # 可公开打包的示例文档/关系/图片，与私人知识库隔离
├── installer/
│   ├── languages/
│   │   └── ChineseSimplified.isl  # 安装程序的语言资源
│   └── Tmd.iss  # Inno Setup 安装、升级和卸载配置
├── licenses/
│   ├── AFL-2.1.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Apache-2.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Bitstream-Vera.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── blessing.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── BSD-2-Clause.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── BSD-3-Clause.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── BSD-4-Clause.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── BSL-1.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── CC0-1.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── DejaVuSansMono_LICENSE  # 第三方许可文本，保留原始文件名与来源
│   ├── FONT-NOTICES.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── FTL.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── gcc-COPYING.RUNTIME.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── gcc-COPYING3.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── GFDL-1.3-no-invariants-only.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── GPL-2.0-only.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── GPL-2.0-or-later.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── GPL-3.0-only.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── HPND-sell-variant.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── HPND.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── IJG.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Imlib2.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Inno-Setup-LICENSE.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── IPL-1.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LGPL-2.1-or-later.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LGPL-3.0-only.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── libpng-2.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Libpng.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LicenseRef-ICC-License.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LicenseRef-Lcs-Telegraphics.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LicenseRef-Qt-Commercial.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── LicenseRef-SHA1-Public-Domain.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Linux-syscall-note.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── md4c-MIT.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MinGW-w64-COPYING.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MinGW-w64-runtime-COPYING.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MIT-Khronos-old.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MIT-open-group.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MIT.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── MPL-2.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── open-sans-LICENSE.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Qt-GPL-exception-1.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── qtbase-6.11.1.spdx.json  # 第三方许可文本，保留原始文件名与来源
│   ├── qtsvg-6.11.1.spdx.json  # 第三方许可文本，保留原始文件名与来源
│   ├── SPL-1.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Unicode-3.0.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── winpthreads-COPYING.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── X11.txt  # 第三方许可文本，保留原始文件名与来源
│   ├── Xerox.txt  # 第三方许可文本，保留原始文件名与来源
│   └── Zlib.txt  # 第三方许可文本，保留原始文件名与来源
├── resources/
│   ├── fonts/
│   │   ├── DejaVuSansMono-Bold.ttf  # 字体文件、许可或资源声明
│   │   ├── DejaVuSansMono.ttf  # 字体文件、许可或资源声明
│   │   ├── DejaVuSansMono_LICENSE  # 字体文件、许可或资源声明
│   │   ├── FONT-NOTICES.txt  # 字体文件、许可或资源声明
│   │   ├── open-sans-LICENSE.txt  # 字体文件、许可或资源声明
│   │   ├── OpenSans-Bold.ttf  # 字体文件、许可或资源声明
│   │   ├── OpenSans-Italic.ttf  # 字体文件、许可或资源声明
│   │   ├── OpenSans-Regular.ttf  # 字体文件、许可或资源声明
│   │   └── OpenSans-SemiBold.ttf  # 字体文件、许可或资源声明
│   ├── branding.qrc  # 品牌图标资源清单
│   ├── tmd.ico  # 程序图标二进制资源
│   ├── tree_md.rc  # Windows 程序图标及版本资源模板
│   └── typography.qrc  # 随程序部署的字体资源清单
├── scripts/
│   ├── Build-Release.ps1  # Release 构建、部署、测试和安装包/归档制作
│   └── Start-Debug.ps1  # 验证部署依赖并从桌面快捷方式启动当前 Debug
├── src/
│   ├── application/
│   │   ├── documents/
│   │   │   └── DocumentSessionController.h  # 后续建议：协调原文 undo、文档视图和光标记录，不复制 Manager 节点库
│   │   ├── editing/
│   │   │   ├── EditCommandRouter.cpp  # 已生成草案：按现有优先级分发核心编辑交易
│   │   │   └── EditCommandRouter.h  # 已生成草案：语义命令、模式、源码选区、表格地址和处理结果
│   │   ├── images/
│   │   │   ├── ImageInsertJob.cpp  # 捕获文档/设置后编排本地、剪贴板、远程和上传插图流程（实现）
│   │   │   ├── ImageInsertJob.h  # 捕获文档/设置后编排本地、剪贴板、远程和上传插图流程（声明）
│   │   │   ├── MarkdownExportJob.cpp  # 携图导出、资源收集、相对路径改写、提交与取消（实现）
│   │   │   └── MarkdownExportJob.h  # 携图导出、资源收集、相对路径改写、提交与取消（声明）
│   │   └── library/
│   │       ├── LibraryImport.cpp  # MarkdownManager 的目录批量导入实现，整批后统一扫描；不是独立类（实现）
│   │       ├── MarkdownDocumentOperations.cpp  # MarkdownManager 的保存/重载/增删/重命名/另存为交易实现；不是独立类（实现）
│   │       ├── MarkdownManager.cpp  # 唯一持有文档节点/关系状态，扫描、缓存更新和变更信号（实现）
│   │       ├── MarkdownManager.h  # 唯一持有文档节点/关系状态，扫描、缓存更新和变更信号（声明）
│   │       ├── MarkdownManagerRelations.cpp  # MarkdownManager 的关系交易、持久化和节点关联更新实现；不是独立类（实现）
│   │       └── MarkdownPathMoves.cpp  # MarkdownManager 的目录/批量文档迁移与失败回滚实现；不是独立类（实现）
│   ├── bootstrap/
│   │   ├── CommandLineRunner.h  # 后续建议：导入/导出/截图命令行编排
│   │   └── main.cpp  # 进程入口、参数解析、库初始化、导入导出命令行与 GUI 启动（实现）
│   ├── core/
│   │   ├── documents/
│   │   │   ├── MarkdownNode.cpp  # 文档路径、已保存/编辑原文、冲突状态、目录缓存和关联角色（实现）
│   │   │   └── MarkdownNode.h  # 文档路径、已保存/编辑原文、冲突状态、目录缓存和关联角色（声明）
│   │   ├── knowledge/
│   │   │   ├── KnowledgeGraphSnapshot.h  # 后续建议：索引只读输入，解除 KnowledgeIndex 对 MarkdownManager 的依赖
│   │   │   ├── KnowledgeIndex.cpp  # 从 MarkdownManager 获取节点/关系，按深度构建知识树范围索引（实现）
│   │   │   ├── KnowledgeIndex.h  # 从 MarkdownManager 获取节点/关系，按深度构建知识树范围索引（声明）
│   │   │   ├── KnowledgeTopology.cpp  # 稳定规范化知识树索引；为命名空间（实现）
│   │   │   ├── KnowledgeTopology.h  # 稳定规范化知识树索引；为命名空间（声明）
│   │   │   ├── MarkdownRelation.cpp  # 关系角色、互逆/替代路径规则，同时包含关系 JSON 读写（实现）
│   │   │   ├── MarkdownRelation.h  # 关系角色、互逆/替代路径规则，同时包含关系 JSON 读写（声明）
│   │   │   └── NodeRelationType.h  # 五种关系角色枚举（声明）
│   │   ├── markdown/
│   │   │   ├── analysis/
│   │   │   │   ├── MarkdownAnalysis.cpp  # 源码行、成对围栏、行内代码和字面区域扫描；维护原始 UTF-16 偏移（实现）
│   │   │   │   ├── MarkdownAnalysis.h  # 源码行、成对围栏、行内代码和字面区域扫描；维护原始 UTF-16 偏移（声明）
│   │   │   │   ├── MarkdownCodeBlocks.cpp  # 成对代码区的语言、原始正文及行位置查询；为命名空间（实现）
│   │   │   │   ├── MarkdownCodeBlocks.h  # 成对代码区的语言、原始正文及行位置查询；为命名空间（声明）
│   │   │   │   ├── MarkdownOutline.h  # MarkdownHeading 记录及 MarkdownOutline 容器别名（声明）
│   │   │   │   ├── MarkdownParser.cpp  # 标题标记、目录与代码围栏的共用查询入口（实现）
│   │   │   │   ├── MarkdownParser.h  # 标题标记、目录与代码围栏的共用查询入口（声明）
│   │   │   │   ├── MarkdownSnapshot.cpp  # 已生成草案：按需复用分析、语义、图片和代码区查询
│   │   │   │   └── MarkdownSnapshot.h  # 已生成草案：指定 Raw/Rendering 的不可变源码快照接口
│   │   │   ├── editing/
│   │   │   │   ├── MarkdownCodeEditing.cpp  # 代码区缩进、换行、退格、退出及空元数据删除（实现）
│   │   │   │   ├── MarkdownCodeEditing.h  # 代码区缩进、换行、退格、退出及空元数据删除（声明）
│   │   │   │   ├── MarkdownEdit.h  # 原文、选区和 handled/changed 编辑交易值对象（声明）
│   │   │   │   ├── MarkdownImport.cpp  # 导入/粘贴中实际缩进代码转围栏的纯文本适配（实现）
│   │   │   │   ├── MarkdownImport.h  # 导入/粘贴中实际缩进代码转围栏的纯文本适配（声明）
│   │   │   │   ├── MarkdownListEditing.cpp  # 列表续写、任务状态、父子项缩进和退格（实现）
│   │   │   │   ├── MarkdownListEditing.h  # 列表续写、任务状态、父子项缩进和退格（声明）
│   │   │   │   ├── MarkdownParagraphEditing.cpp  # 标题/引用/普通段落的 Enter、Backspace、Delete 和分隔复用（实现）
│   │   │   │   ├── MarkdownParagraphEditing.h  # 标题/引用/普通段落的 Enter、Backspace、Delete 和分隔复用（声明）
│   │   │   │   ├── MarkdownTableEditing.cpp  # MD4C 确认表格、格子偏移、规范化、整表增删行列和网格粘贴；为命名空间（实现）
│   │   │   │   ├── MarkdownTableEditing.h  # MD4C 确认表格、格子偏移、规范化、整表增删行列和网格粘贴；为命名空间（声明）
│   │   │   │   ├── TextPatch.cpp  # 已生成草案：共用替换范围和完整代理对边界
│   │   │   │   └── TextPatch.h  # 已生成草案：最小 UTF-16 文本替换值对象
│   │   │   ├── mapping/
│   │   │   │   ├── MarkdownSourceMap.cpp  # 原文与显示 UTF-16 位置映射、隐藏标记边界、空白恢复及选区编辑计划（实现）
│   │   │   │   ├── MarkdownSourceMap.h  # 原文与显示 UTF-16 位置映射、隐藏标记边界、空白恢复及选区编辑计划（声明）
│   │   │   │   ├── MarkdownSpelling.cpp  # 链接、图片、实体、定义等原文展开区域及部分删除保护（实现）
│   │   │   │   └── MarkdownSpelling.h  # 链接、图片、实体、定义等原文展开区域及部分删除保护（声明）
│   │   │   └── syntax/
│   │   │       ├── CodeSyntax.cpp  # 按语言的有状态代码分词及配色，不绘制 Qt 控件（实现）
│   │   │       ├── CodeSyntax.h  # 按语言的有状态代码分词及配色，不绘制 Qt 控件（声明）
│   │   │       ├── LanguageCatalog.cpp  # 语言名、别名、显示标签和补全目录（实现）
│   │   │       ├── LanguageCatalog.h  # 语言名、别名、显示标签和补全目录（声明）
│   │   │       ├── MarkdownBlockSyntax.cpp  # 行前缀识别、渲染准备与位置映射、N55 首个空行判定（实现）
│   │   │       ├── MarkdownBlockSyntax.h  # 行前缀识别、渲染准备与位置映射、N55 首个空行判定（声明）
│   │   │       ├── MarkdownImages.cpp  # Markdown 图片语法识别、地址处理和源码引用替换（实现）
│   │   │       ├── MarkdownImages.h  # Markdown 图片语法识别、地址处理和源码引用替换（声明）
│   │   │       ├── MarkdownSyntax.cpp  # MD4C 语义事件、标题/行内格式、元数据及渲染功能适配（实现）
│   │   │       └── MarkdownSyntax.h  # MD4C 语义事件、标题/行内格式、元数据及渲染功能适配（声明）
│   │   └── settings/
│   │       ├── MarkdownSettings.cpp  # 样式/图片设置数据和范围描述；当前头文件还声明 MarkdownSettingsStore（实现）
│   │       └── MarkdownSettings.h  # 样式/图片设置数据和范围描述；当前头文件还声明 MarkdownSettingsStore（声明）
│   ├── infrastructure/
│   │   ├── filesystem/
│   │   │   ├── DefaultLibrary.cpp  # 安装版用户库位置与公开示例初始化（实现）
│   │   │   ├── DefaultLibrary.h  # 安装版用户库位置与公开示例初始化（声明）
│   │   │   ├── FileWatcher.cpp  # 文件变化监听；Windows 递归异步句柄与其他平台监听适配（实现）
│   │   │   ├── FileWatcher.h  # 文件变化监听；Windows 递归异步句柄与其他平台监听适配（声明）
│   │   │   ├── LibraryPathPolicy.cpp  # 库内路径、名称及链接越界约束（实现）
│   │   │   ├── LibraryPathPolicy.h  # 库内路径、名称及链接越界约束（声明）
│   │   │   ├── MarkdownDirectoryMove.cpp  # MarkdownFileIO::renameDirectory 的 Windows 重试/暂存/回退实现；不是独立类（实现）
│   │   │   ├── MarkdownFileIO.cpp  # 原子写入、读取、文件改名、回收站与文件身份（实现）
│   │   │   └── MarkdownFileIO.h  # 原子写入、读取、文件改名、回收站与文件身份（声明）
│   │   ├── images/
│   │   │   ├── ImageLoader.cpp  # 本地/内嵌/HTTP 图片读取与解码、超时/大小限制和取消（实现）
│   │   │   ├── ImageLoader.h  # 本地/内嵌/HTTP 图片读取与解码、超时/大小限制和取消（声明）
│   │   │   ├── ImageStorage.cpp  # 平铺哈希去重、绝对引用、导入/迁移资源准备及已建资源回滚（实现）
│   │   │   ├── ImageStorage.h  # 平铺哈希去重、绝对引用、导入/迁移资源准备及已建资源回滚（声明）
│   │   │   ├── ImageUploadJob.cpp  # 图床 HTTP 上传、凭据参数、响应 URL 提取、超时和取消（实现）
│   │   │   └── ImageUploadJob.h  # 图床 HTTP 上传、凭据参数、响应 URL 提取、超时和取消（声明）
│   │   ├── persistence/
│   │   │   └── RelationFileStore.h  # 后续建议：从 MarkdownRelation 拆出 JSON 文件读写
│   │   ├── platform/
│   │   │   ├── FileManager.cpp  # 系统文件管理器打开或定位（实现）
│   │   │   ├── FileManager.h  # 系统文件管理器打开或定位（声明）
│   │   │   ├── LocalCredentials.cpp  # Windows 本机凭据加密/解密适配（实现）
│   │   │   └── LocalCredentials.h  # Windows 本机凭据加密/解密适配（声明）
│   │   └── settings/
│   │       ├── KnowledgeViewSettings.cpp  # 知识树缩放等共享视图设置（实现）
│   │       ├── KnowledgeViewSettings.h  # 知识树缩放等共享视图设置（声明）
│   │       ├── MarkdownSettingsCodec.cpp  # 版本化设置校验、二进制与十六进制编解码（实现）
│   │       ├── MarkdownSettingsCodec.h  # 版本化设置校验、二进制与十六进制编解码（声明）
│   │       ├── MarkdownSettingsStore.cpp  # MarkdownSettingsStore 的本地持久化、凭据保护调用和缓存实现；声明在 MarkdownSettings.h（实现）
│   │       └── MarkdownSettingsStore.h  # 后续建议：从 MarkdownSettings.h 独立出持久化类声明
│   └── ui/
│       ├── common/
│       │   ├── AppIcon.cpp  # treeMdIcon：读取品牌图标的函数（实现）
│       │   ├── AppIcon.h  # treeMdIcon：读取品牌图标的函数（声明）
│       │   ├── Theme.cpp  # applyTreeMdTheme：全局 Qt 主题和弹窗样式函数（实现）
│       │   └── Theme.h  # applyTreeMdTheme：全局 Qt 主题和弹窗样式函数（声明）
│       ├── document/
│       │   ├── CodeBlockDecoration.cpp  # 代码区语言标签、复制按钮、位置命中和装饰绘制（实现）
│       │   ├── CodeBlockDecoration.h  # 代码区语言标签、复制按钮、位置命中和装饰绘制（声明）
│       │   ├── CodeBlockFormat.h  # 代码块 Qt 格式属性常量和同一区域判断；无独立类（声明）
│       │   ├── CodeSyntaxHighlighter.cpp  # 把核心分词结果映射成 QSyntaxHighlighter 文本格式（实现）
│       │   ├── CodeSyntaxHighlighter.h  # 把核心分词结果映射成 QSyntaxHighlighter 文本格式（声明）
│       │   ├── DocumentImagePresenter.h  # 后续建议：预览图片占位、代次和尺寸，复用现有 ImageLoader
│       │   ├── DocumentSelectionController.h  # 后续建议：框选、标记命中和源码/显示选区状态
│       │   ├── ImageInput.cpp  # 从 Qt MIME 数据识别文件、图像和允许的图床地址（实现）
│       │   ├── ImageInput.h  # 从 Qt MIME 数据识别文件、图像和允许的图床地址（声明）
│       │   ├── InlineMarkupDecoration.cpp  # 行内符号、圆角背景、边界光标和选中色的绘制（实现）
│       │   ├── InlineMarkupDecoration.h  # 行内符号、圆角背景、边界光标和选中色的绘制（声明）
│       │   ├── MarkdownDocumentView.cpp  # QTextBrowser 文档编辑宿主；目前还承载渲染管线、源码映射、选择/输入和图片状态（实现）
│       │   ├── MarkdownDocumentView.h  # QTextBrowser 文档编辑宿主；目前还承载渲染管线、源码映射、选择/输入和图片状态（声明）
│       │   ├── MarkdownEditor.cpp  # QTextEdit 原文编辑宿主、核心编辑分发、粘贴/图片输入、滚动/缩放（实现）
│       │   ├── MarkdownEditor.h  # QTextEdit 原文编辑宿主、核心编辑分发、粘贴/图片输入、滚动/缩放（声明）
│       │   ├── MarkdownFenceCompleter.cpp  # 围栏语言提示与创建交易的 Qt 输入控制（实现）
│       │   ├── MarkdownFenceCompleter.h  # 围栏语言提示与创建交易的 Qt 输入控制（声明）
│       │   ├── MarkdownRenderPipeline.h  # 后续建议：分阶段导入、源码映射、空白恢复和样式应用
│       │   ├── MarkdownTypography.cpp  # 设置驱动的字体、字号、行距与滚轮步数辅助（实现）
│       │   ├── MarkdownTypography.h  # 设置驱动的字体、字号、行距与滚轮步数辅助（声明）
│       │   ├── MarkdownViewport.cpp  # 视口三分之一锚点对齐及约半页底部滚动空间（实现）
│       │   ├── MarkdownViewport.h  # 视口三分之一锚点对齐及约半页底部滚动空间（声明）
│       │   ├── QtTextEditAdapter.cpp  # 已生成草案：单次撤销交易及过期源码校验
│       │   └── QtTextEditAdapter.h  # 已生成草案：把源码交易应用到原文 QTextDocument 的适配接口
│       ├── knowledge/
│       │   ├── KnowledgeGraphWidget.cpp  # 知识树面板/完善窗口的宿主及关系修改交互（实现）
│       │   ├── KnowledgeGraphWidget.h  # 知识树面板/完善窗口的宿主及关系修改交互（声明）
│       │   ├── KnowledgeNodeWidget.cpp  # 知识节点卡片和关系端口的绘制与鼠标交互（实现）
│       │   ├── KnowledgeNodeWidget.h  # 知识节点卡片和关系端口的绘制与鼠标交互（声明）
│       │   ├── KnowledgeTreeCanvas.cpp  # 图形场景、卡片/连线生命周期、选择/拖动/缩放/请求信号（实现）
│       │   ├── KnowledgeTreeCanvas.h  # 图形场景、卡片/连线生命周期、选择/拖动/缩放/请求信号（声明）
│       │   ├── KnowledgeTreeLayout.cpp  # 知识树节点矩形、层级、路由锚点和标题几何布局（实现）
│       │   ├── KnowledgeTreeLayout.h  # 知识树节点矩形、层级、路由锚点和标题几何布局（声明）
│       │   ├── KnowledgeTreeRouter.cpp  # 端口、方向、连线路径和避障几何；为命名空间（实现）
│       │   ├── KnowledgeTreeRouter.h  # 端口、方向、连线路径和避障几何；为命名空间（声明）
│       │   ├── RelationDialog.cpp  # 关联节点选择及关系类型编辑对话框（实现）
│       │   └── RelationDialog.h  # 关联节点选择及关系类型编辑对话框（声明）
│       ├── library/
│       │   ├── DocumentFileTree.cpp  # 文字命中拖动、多选框选、拖动临时展开、边缘滚动及移动请求（实现）
│       │   ├── DocumentFileTree.h  # 文字命中拖动、多选框选、拖动临时展开、边缘滚动及移动请求（声明）
│       │   ├── DocumentSidebar.cpp  # 左侧文件/目录、搜索、开合持久化及菜单意图（实现）
│       │   ├── DocumentSidebar.h  # 左侧文件/目录、搜索、开合持久化及菜单意图（声明）
│       │   ├── DocumentTreeBuilder.cpp  # 两个文件列表共用的建树、元数据角色、标题/未保存点更新和过滤（实现）
│       │   ├── DocumentTreeBuilder.h  # 两个文件列表共用的建树、元数据角色、标题/未保存点更新和过滤（声明）
│       │   ├── OutlineSidebar.cpp  # 正文标题目录控件及位置激活信号（实现）
│       │   └── OutlineSidebar.h  # 正文标题目录控件及位置激活信号（声明）
│       ├── settings/
│       │   ├── ImageSettingsDialog.cpp  # 图片保存目录、图床配置及链接管理界面（实现）
│       │   ├── ImageSettingsDialog.h  # 图片保存目录、图床配置及链接管理界面（声明）
│       │   ├── MarkdownSettingsDialog.cpp  # Markdown 外观设置界面，应用后通知刷新（实现）
│       │   └── MarkdownSettingsDialog.h  # Markdown 外观设置界面，应用后通知刷新（声明）
│       └── shell/
│           ├── MainWindow.cpp  # 装配界面，同时编排当前文档、保存/移动、双视图同步、撤销光标和图片作业（实现）
│           ├── MainWindow.h  # 装配界面，同时编排当前文档、保存/移动、双视图同步、撤销光标和图片作业（声明）
│           └── MainWindowLayout.h  # 后续建议：主窗口响应式布局和面板尺寸
├── tests/
│   ├── core/
│   │   └── MarkdownCoreTests.cpp  # 核心解析、源码映射、图片导入、批量导入及另存为回归（实现）
│   ├── infrastructure/
│   │   └── DefaultLibraryTests.cpp  # 示例初始化与安装库保护回归（实现）
│   ├── integration/
│   │   └── RefactorIntegrationTests.cpp  # 标题/正文修改时知识树增量刷新及对象保留回归（实现）
│   ├── proposals/
│   │   └── ModularEditingTests.cpp  # 已生成并单独编译的草案检查，不替代正式回归
│   ├── ui/
│   │   ├── editing/
│   │   │   ├── EditingCodeAuditTests.cpp  # 代码区、选择和跨围栏编辑审查（实现）
│   │   │   ├── EditingHeadingsAuditTests.cpp  # 标题、引用、空白及相关输入审查（实现）
│   │   │   ├── EditingIntegrationAuditTests.cpp  # 跨元素编辑及撤销等集成审查（实现）
│   │   │   ├── EditingListAuditTests.cpp  # 列表、缩进、退格及任务规则审查（实现）
│   │   │   └── EditingPolicyTests.cpp  # 已确认输入规则和 N55 空行策略专项回归（实现）
│   │   └── TreeMdTests.cpp  # 综合交互、文件/关系/图片/设置回归；由 RegressionGroups.cmake 分组（实现）
│   └── RegressionGroups.cmake  # 综合用例分组注册及遗漏检查
├── third_party/
│   └── md4c/
│       ├── md4c.c  # MD4C 第三方原件或版本来源，不在业务重构中改写
│       ├── md4c.h  # MD4C 第三方原件或版本来源，不在业务重构中改写
│       └── README.md  # MD4C 第三方原件或版本来源，不在业务重构中改写
├── .gitignore  # 排除本地知识库、构建和私有产物
├── AGENTS.md  # 项目协作、构建和发布约定
├── CHANGELOG.md  # 项目说明或变更记录
├── CMakeLists.txt  # 显式构建目标、源文件、Qt 依赖、安装和测试入口
├── LICENSE  # 项目许可
├── README.md  # 项目说明或变更记录
└── THIRD_PARTY_NOTICES.md  # 第三方组件说明
```

## 迁移条件

- MarkdownRelation 的领域规则留下，JSON 读写迁到 RelationFileStore 后才能隔离领域与磁盘。
- KnowledgeIndex 必须先接受 KnowledgeGraphSnapshot，不能把依赖 MarkdownManager 的现有实现直接声明为纯领域层。
- MarkdownSettings.h 拆分数据与 Store 声明，调用方显式包含各自接口。
- MarkdownManagerRelations.cpp 等仍实现 MarkdownManager 的成员，迁移目录不会自动生成新类。
- 保留根目录第三方、资源、示例、许可、安装脚本和文档；保留私人知识库的实际目录与配置，不跟随 src 迁移。
