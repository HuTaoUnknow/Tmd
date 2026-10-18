# Tmd 模块化编辑代码草案

本目录包含实际可编译的重构切片，复用当前工作区的核心算法。应用入口、原有 core/ui、已确认语法和本地知识库没有被替换。完整评审见 [REPORT.md](D:/PC/Project/Qt/tree_md/docs/optimization-review/2026-10-18/REPORT.md)。

## 已生成代码

| 文件 | 职责 |
| --- | --- |
| `core/markdown/editing/TextPatch.h/.cpp` | 替换两处重复的公共前后缀差分；使用 UTF-16，替换范围不拆开合法代理对 |
| `core/markdown/analysis/MarkdownSnapshot.h/.cpp` | 一份不可变原文，显式 Raw/Rendering 模式，分析和派生查询按需复用 |
| `application/editing/EditCommandRouter.h/.cpp` | 从 Qt 按键分支提取核心编辑分发；保留原文/文档模式顺序差异 |
| `ui/document/QtTextEditAdapter.h/.cpp` | 校验原文未过期，以一次 QTextCursor 交易应用差分并恢复选区 |
| `tests/ModularEditingTests.cpp` | Unicode 替换、Qt undo/redo、过期交易拒绝、快照语义和与实际原文编辑器的一致性检查 |
| `CMakeLists.txt` | 明确登记上述源文件，单独构建草案，复用原项目静态库 |

## 接入形态

```cpp
using namespace tmd::proposal;
// source、anchor、caret 由 Qt 适配层转换为真实 Markdown 的 UTF-16 坐标。
EditRequest request{source, {anchor, caret}, EditCommand::Newline, ViewMode::Source};
const auto result = EditCommandRouter::dispatch(request);
if (result.edit.handled) {
    auto cursor = sourceEditor->textCursor();
    QString error;
    if (QtTextEditAdapter::apply(cursor, source, result.edit, &error))
        sourceEditor->setTextCursor(cursor);
    // 过期/无效交易应走调用方已有错误通道；不可静默覆盖。
}
```

这是调用形态示例，`sourceEditor` 由现有窗口注入，不是另建文档缓冲。尚未接入原事件函数。

表格键由 Qt 适配层转成 `NextCell` / `AddColumn` / `AddRow`，并提供 table/row/column；表格普通 Enter 使用 `Newline`。格式末尾光标由 `paragraphSelection` 显式传入，避免隐藏定界符落到下一段。处理器返回未接管时，继续保留原有 Qt/隐藏符号/IME 分支，不能直接吞掉按键。

## 构建与运行

在项目根目录，用已配置的 Qt/MinGW 工具链运行：

```powershell
cmake -S proposals/modular-editing -B build-architecture-proposal -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=D:/PC/EXE/QtCreator/6.11.1/mingw_64 `
  -DCMAKE_MAKE_PROGRAM=D:/PC/EXE/QtCreator/Tools/Ninja/ninja.exe
cmake --build build-architecture-proposal --target tmd_proposal_tests --parallel 4
ctest --test-dir build-architecture-proposal --output-on-failure
```

终端 PATH 需要包含 Qt 和匹配 MinGW 的 bin；设置 `QT_QPA_PLATFORM_PLUGIN_PATH` 为开发 Qt 的 plugins/platforms、`QT_QPA_PLATFORM=offscreen`、`QT_QPA_FONTDIR=C:/Windows/Fonts`。受限环境将 TEMP/TMP 指向构建目录内可写目录。若默认编译器未配置，首轮配置显式传入 MinGW 的 gcc.exe/g++.exe。

## 范围与限制

- 快照的惰性缓存由 GUI 单线程持有，不宣称线程安全；换原文时创建新快照，配置导致的语义变更也应使上层快照失效。
- 快照只复用已有允许传入 analysis 的查询。表格解析、SourceMap 和渲染中间文本的接入需要后续显式重载，不能自动减少整篇重建。
- `TextPatch` 是单段替换，不是通用 diff/merge，也不自动合并并发修改；`expectedSource` 校验负责拒绝过期交易。
- `QtTextEditAdapter` 针对原文文档；文档视图的隐藏符号映射、图片选择、原生 IME 和视觉方向移动仍保留在现有 Qt 层。
- 本轮没有把所有文件迁到目标目录，没有更改现有 app 的运行路径或发布配置。目标结构和后续类在报告中明确标注。
