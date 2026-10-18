"""Reproduce the baseline file catalogue; no third-party Python dependencies."""
from pathlib import Path
from datetime import datetime
import hashlib
import argparse
import json
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8')
ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--output-dir', help='Write a fresh catalogue separately from the historical review baseline.')
args = parser.parse_args()
if args.output_dir:
    OUT = (ROOT / args.output_dir).resolve()
    if not OUT.is_relative_to(ROOT / 'docs/optimization-review'):
        raise SystemExit('Output must remain under docs/optimization-review.')
GIT = 'git'

# Descriptions are reviewed manually; the generator only inventories and links.
ROLES = {
'main': '进程入口、参数解析、库初始化、导入导出命令行与 GUI 启动',
'EditCommandRouter': '应用层共用的源码编辑命令、上下文和规则优先级',
'TextPatch': 'UTF-16 公共前后缀单段替换，保留完整代理对边界',
'MarkdownSnapshot': '指定 Raw/Rendering 模式的不可变原文与惰性分析结果',
'QtTextEditAdapter': '校验预期原文，以单次 Qt undo 交易应用替换和恢复选区',
'ModularEditingTests': '模块化编辑、Unicode、undo、快照共享/失效与固定输入结果回归',
'MarkdownAnalysis': '源码行、成对围栏、行内代码和字面区域扫描；维护原始 UTF-16 偏移',
'MarkdownBlockSyntax': '行前缀识别、渲染准备与位置映射、N55 首个空行判定',
'MarkdownSyntax': 'MD4C 语义事件、标题/行内格式、元数据及渲染功能适配',
'MarkdownParser': '标题标记、目录与代码围栏的共用查询入口',
'MarkdownOutline': 'MarkdownHeading 记录及 MarkdownOutline 容器别名',
'MarkdownEdit': '原文、选区和 handled/changed 编辑交易值对象',
'MarkdownCodeEditing': '代码区缩进、换行、退格、退出及空元数据删除',
'MarkdownListEditing': '列表续写、任务状态、父子项缩进和退格',
'MarkdownParagraphEditing': '标题/引用/普通段落的 Enter、Backspace、Delete 和分隔复用',
'MarkdownTableEditing': 'MD4C 确认表格、格子偏移、规范化、整表增删行列和网格粘贴；为命名空间',
'MarkdownSourceMap': '原文与显示 UTF-16 位置映射、隐藏标记边界、空白恢复及选区编辑计划',
'MarkdownSpelling': '链接、图片、实体、定义等原文展开区域及部分删除保护',
'MarkdownImport': '导入/粘贴中实际缩进代码转围栏的纯文本适配',
'MarkdownImages': 'Markdown 图片语法识别、地址处理和源码引用替换',
'MarkdownCodeBlocks': '成对代码区的语言、原始正文及行位置查询；为命名空间',
'LanguageCatalog': '语言名、别名、显示标签和补全目录',
'CodeSyntax': '按语言的有状态代码分词及配色，不绘制 Qt 控件',
'MarkdownManager': '唯一持有文档节点/关系状态，扫描、缓存更新和变更信号',
'MarkdownNode': '文档路径、已保存/编辑原文、冲突状态、目录缓存和关联角色',
'MarkdownDocumentOperations': 'MarkdownManager 的保存/重载/增删/重命名/另存为交易实现；不是独立类',
'MarkdownPathMoves': 'MarkdownManager 的目录/批量文档迁移与失败回滚实现；不是独立类',
'LibraryImport': 'MarkdownManager 的目录批量导入实现，整批后统一扫描；不是独立类',
'MarkdownFileIO': '原子写入、读取、文件改名、回收站与文件身份',
'MarkdownDirectoryMove': 'MarkdownFileIO::renameDirectory 的 Windows 重试/暂存/回退实现；不是独立类',
'LibraryPathPolicy': '库内路径、名称及链接越界约束',
'FileWatcher': '文件变化监听；Windows 递归异步句柄与其他平台监听适配',
'DefaultLibrary': '安装版用户库位置与公开示例初始化',
'MarkdownManagerRelations': 'MarkdownManager 的关系交易、持久化和节点关联更新实现；不是独立类',
'MarkdownRelation': '关系角色、互逆/替代路径规则，同时包含关系 JSON 读写',
'NodeRelationType': '五种关系角色枚举',
'KnowledgeIndex': '从 MarkdownManager 获取节点/关系，按深度构建知识树范围索引',
'KnowledgeTopology': '稳定规范化知识树索引；为命名空间',
'ImageLoader': '本地/内嵌/HTTP 图片读取与解码、超时/大小限制和取消',
'ImageStorage': '平铺哈希去重、绝对引用、导入/迁移资源准备及已建资源回滚',
'ImageInsertJob': '捕获文档/设置后编排本地、剪贴板、远程和上传插图流程',
'ImageUploadJob': '图床 HTTP 上传、凭据参数、响应 URL 提取、超时和取消',
'MarkdownExportJob': '携图导出、资源收集、相对路径改写、提交与取消',
'MarkdownSettings': '样式/图片设置数据和范围描述；当前头文件还声明 MarkdownSettingsStore',
'MarkdownSettingsCodec': '版本化设置校验、二进制与十六进制编解码',
'MarkdownSettingsStore': 'MarkdownSettingsStore 的本地持久化、凭据保护调用和缓存实现；声明在 MarkdownSettings.h',
'KnowledgeViewSettings': '知识树缩放等共享视图设置',
'LocalCredentials': 'Windows 本机凭据加密/解密适配',
'FileManager': '系统文件管理器打开或定位',
'MainWindow': '装配界面，同时编排当前文档、保存/移动、双视图同步、撤销光标和图片作业',
'MarkdownDocumentView': 'QTextBrowser 文档编辑宿主；目前还承载渲染管线、源码映射、选择/输入和图片状态',
'MarkdownEditor': 'QTextEdit 原文编辑宿主、核心编辑分发、粘贴/图片输入、滚动/缩放',
'MarkdownViewport': '视口三分之一锚点对齐及约半页底部滚动空间',
'MarkdownTypography': '设置驱动的字体、字号、行距与滚轮步数辅助',
'MarkdownFenceCompleter': '围栏语言提示与创建交易的 Qt 输入控制',
'CodeSyntaxHighlighter': '把核心分词结果映射成 QSyntaxHighlighter 文本格式',
'CodeBlockDecoration': '代码区语言标签、复制按钮、位置命中和装饰绘制',
'CodeBlockFormat': '代码块 Qt 格式属性常量和同一区域判断；无独立类',
'InlineMarkupDecoration': '行内符号、圆角背景、边界光标和选中色的绘制',
'ImageInput': '从 Qt MIME 数据识别文件、图像和允许的图床地址',
'DocumentSidebar': '左侧文件/目录、搜索、开合持久化及菜单意图',
'DocumentFileTree': '文字命中拖动、多选框选、拖动临时展开、边缘滚动及移动请求',
'DocumentTreeBuilder': '两个文件列表共用的建树、元数据角色、标题/未保存点更新和过滤',
'OutlineSidebar': '正文标题目录控件及位置激活信号',
'KnowledgeGraphWidget': '知识树面板/完善窗口的宿主及关系修改交互',
'KnowledgeTreeCanvas': '图形场景、卡片/连线生命周期、选择/拖动/缩放/请求信号',
'KnowledgeTreeLayout': '知识树节点矩形、层级、路由锚点和标题几何布局',
'KnowledgeTreeRouter': '端口、方向、连线路径和避障几何；为命名空间',
'KnowledgeNodeWidget': '知识节点卡片和关系端口的绘制与鼠标交互',
'RelationDialog': '关联节点选择及关系类型编辑对话框',
'MarkdownSettingsDialog': 'Markdown 外观设置界面，应用后通知刷新',
'ImageSettingsDialog': '图片保存目录、图床配置及链接管理界面',
'Theme': 'applyTreeMdTheme：全局 Qt 主题和弹窗样式函数',
'AppIcon': 'treeMdIcon：读取品牌图标的函数',
'TreeMdTests': '综合交互、文件/关系/图片/设置回归；由 RegressionGroups.cmake 分组',
'MarkdownCoreTests': '核心解析、源码映射、图片导入、批量导入及另存为回归',
'RefactorIntegrationTests': '标题/正文修改时知识树增量刷新及对象保留回归',
'DefaultLibraryTests': '示例初始化与安装库保护回归',
'EditingHeadingsAuditTests': '标题、引用、空白及相关输入审查',
'EditingCodeAuditTests': '代码区、选择和跨围栏编辑审查',
'EditingListAuditTests': '列表、缩进、退格及任务规则审查',
'EditingIntegrationAuditTests': '跨元素编辑及撤销等集成审查',
'EditingPolicyTests': '已确认输入规则和 N55 空行策略专项回归',
}


def role(path):
    stem, suffix = path.stem, path.suffix
    if stem in ROLES:
        ending = '（声明）' if suffix == '.h' else '（实现）' if suffix in ('.cpp', '.c') else ''
        return ROLES[stem] + ending
    named = {
        'CMakeLists.txt': '显式构建目标、源文件、Qt 依赖、安装和测试入口',
        'RegressionGroups.cmake': '综合用例分组注册及遗漏检查',
        'Start-Debug.ps1': '验证部署依赖并从桌面快捷方式启动当前 Debug',
        'Build-Release.ps1': 'Release 构建、部署、测试和安装包/归档制作',
        'Tmd.iss': 'Inno Setup 安装、升级和卸载配置',
        'branding.qrc': '品牌图标资源清单', 'typography.qrc': '随程序部署的字体资源清单',
        'tree_md.rc': 'Windows 程序图标及版本资源模板', 'tmd.ico': '程序图标二进制资源',
        'build.yml': 'GitHub Actions Windows 构建和回归流水线',
        'AGENTS.md': '项目协作、构建和发布约定', '.gitignore': '排除本地知识库、构建和私有产物',
        'LICENSE': '项目许可', 'THIRD_PARTY_NOTICES.md': '第三方组件说明',
    }
    if path.name in named: return named[path.name]
    if path.parts[0] == 'licenses': return '第三方许可文本，保留原始文件名与来源'
    if path.parts[0] == 'examples': return '可公开打包的示例文档/关系/图片，与私人知识库隔离'
    if path.parts[0] == 'third_party': return 'MD4C 第三方原件或版本来源，不在业务重构中改写'
    if path.parts[0] == 'docs': return '已有项目文档或语法审查资料'
    if path.parts[0] == 'resources': return '字体文件、许可或资源声明'
    if path.parts[0] == 'installer': return '安装程序的语言资源'
    if suffix == '.md': return '项目说明或变更记录'
    raise ValueError('Missing description: ' + str(path))


def destination(path):
    parts, stem = path.parts, path.stem
    if path.as_posix() == 'main.cpp': return Path('src/bootstrap/main.cpp')
    if parts[0] == 'application': return Path('src') / path
    if parts[0] == 'tests' and path.suffix == '.cpp':
        category = 'core' if stem == 'MarkdownCoreTests' else 'infrastructure' if stem == 'DefaultLibraryTests' else 'integration' if stem in {'RefactorIntegrationTests', 'ModularEditingTests'} else 'ui/editing' if stem.startswith('Editing') else 'ui'
        return Path('tests') / category / path.name
    if parts[0] == 'ui':
        return Path('src/ui/shell') / path.name if len(parts) == 2 else Path('src') / path
    if parts[0] != 'core': return path
    if parts[1] == 'markdown':
        category = 'editing' if stem in {'MarkdownEdit','TextPatch','MarkdownCodeEditing','MarkdownListEditing','MarkdownTableEditing','MarkdownParagraphEditing','MarkdownImport'} else 'mapping' if stem in {'MarkdownSourceMap','MarkdownSpelling'} else 'syntax' if stem in {'MarkdownBlockSyntax','MarkdownSyntax','MarkdownImages','LanguageCatalog','CodeSyntax'} else 'analysis'
        return Path('src/core/markdown') / category / path.name
    if parts[1] == 'library':
        if stem == 'MarkdownNode': return Path('src/core/documents') / path.name
        if stem in {'MarkdownManager','MarkdownDocumentOperations','MarkdownPathMoves','LibraryImport'}:
            return Path('src/application/library') / path.name
        return Path('src/infrastructure/filesystem') / path.name
    if parts[1] == 'knowledge':
        return Path('src/application/library') / path.name if stem == 'MarkdownManagerRelations' else Path('src/core/knowledge') / path.name
    if parts[1] == 'images':
        return Path('src/application/images' if stem in {'ImageInsertJob','MarkdownExportJob'} else 'src/infrastructure/images') / path.name
    if parts[1] == 'settings':
        return Path('src/core/settings' if stem == 'MarkdownSettings' else 'src/infrastructure/settings') / path.name
    return Path('src/infrastructure/platform') / path.name


def tree(entries):
    root = {}
    for path, description in entries:
        node = root
        for part in Path(path).parts[:-1]: node = node.setdefault(part + '/', {})
        node.setdefault(Path(path).name, description)
    lines = ['tree_md/']
    def visit(node, prefix):
        ordered = sorted(node, key=lambda name: (not isinstance(node[name], dict), name.casefold()))
        for i, name in enumerate(ordered):
            last = i == len(ordered) - 1
            lines.append(prefix + ('└── ' if last else '├── ') + name + ('' if isinstance(node[name], dict) else '  # ' + node[name]))
            if isinstance(node[name], dict): visit(node[name], prefix + ('    ' if last else '│   '))
    visit(root, '')
    return '\n'.join(lines)


tracked = subprocess.check_output([GIT, 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0')
names = set(name for name in tracked if name and (ROOT / name).is_file())
source_dirs = [name for name in ('core', 'ui', 'application', 'tests') if (ROOT / name).is_dir()]
names.update(subprocess.check_output(['rg', '--files', *source_dirs], cwd=ROOT).decode('utf-8').replace('\\', '/').splitlines())
names.add('scripts/Start-Debug.ps1')
names = sorted(name for name in names if not name.startswith(('md_data/', 'md_photo/', 'proposals/', 'docs/optimization-review/')))
source_names = [name for name in names if Path(name).suffix in ('.cpp','.h','.c') and (name.startswith(('core/','ui/','application/','tests/')) or name == 'main.cpp')]
inventory = []
for name in source_names:
    path = Path(name)
    data = (ROOT / path).read_bytes()
    text = data.decode('utf-8-sig')
    includes = []
    for match in re.finditer(r'^\s*#\s*include\s*"([^"]+)"', text, re.M):
        included = match[1]
        target = included if (ROOT / included).is_file() else (path.parent / included).as_posix()
        if (ROOT / target).is_file(): includes.append({'target': target, 'line': text[:match.start()].count('\n') + 1})
    inventory.append({'path': name, 'lines': len(text.splitlines()), 'bytes': len(data),
                      'sha256': hashlib.sha256(data).hexdigest().upper(),
                      'symbols': sorted(set(re.findall(r'^\s*(?:class|struct|namespace)\s+(\w+)\s*(?:final\s*)?(?=[:{])', text, re.M))),
                      'includes': includes, 'role': role(path), 'proposedPath': destination(path).as_posix()})
module = lambda name: '/'.join(Path(name).parts[:2]) if '/' in name else name
edges = {}
for item in inventory:
    for inc in item['includes']:
        a, b = module(item['path']), module(inc['target'])
        if a != b: edges.setdefault(a, set()).add(b)
indices, low, stack, active, components = {}, {}, [], set(), []
def visit(v):
    indices[v] = low[v] = len(indices); stack.append(v); active.add(v)
    for w in edges.get(v, []):
        if w not in indices: visit(w); low[v] = min(low[v], low[w])
        elif w in active: low[v] = min(low[v], indices[w])
    if low[v] == indices[v]:
        members = []
        while True:
            w = stack.pop(); active.remove(w); members.append(w)
            if w == v: break
        if len(members) > 1: components.append(sorted(members))
for key in sorted(set(edges) | {v for values in edges.values() for v in values}):
    if key not in indices: visit(key)
payload = {'reviewDate': '2026-10-18', 'head': subprocess.check_output([GIT, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
           'scope': 'Current working tree, including existing uncommitted source edits; exclude private/generated data.',
           'files': inventory, 'moduleIncludeEdges': {key: sorted(values) for key, values in sorted(edges.items())},
           'moduleIncludeCycles': components,
           'summary': {'sourceFiles': len(inventory), 'sourceLines': sum(i['lines'] for i in inventory),
                       'productionFiles': sum(not i['path'].startswith('tests/') for i in inventory),
                       'testFiles': sum(i['path'].startswith('tests/') for i in inventory)}}
OUT.mkdir(parents=True, exist_ok=True)
if not args.output_dir and (OUT / 'inventory.json').exists():
    baseline = json.loads((OUT / 'inventory.json').read_text(encoding='utf-8'))
    if {item['path']: item['sha256'] for item in baseline['files']} != {item['path']: item['sha256'] for item in inventory}:
        raise SystemExit('The source has changed. Use --output-dir to preserve the historical review baseline.')
(OUT / 'inventory.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')
before_entries = [(Path(name), role(Path(name))) for name in names]
before_entries += [(Path('md_data/<私人文档>'), '本地知识库，不纳入代码扫描或重构'),
                   (Path('md_photo/<私人图片>'), '本地图片，不纳入代码扫描或重构'),
                   (Path('build*/<构建产物>'), '编译器生成，不参与源码结构迁移'),
                   (Path('dist/<发布产物>'), '安装包/归档，不参与源码结构迁移'),
                   (Path('package/<私有操作记录>'), '备份和临时验证产物'),
                   (Path('.tree-md-backup-*/<历史备份>'), '保留的旧备份，不是正在运行的项目代码')]
(OUT / 'CURRENT-STRUCTURE.md').write_text('# 当前目录及逐文件职责\n\n扫描当前工作区，`.h` 和 `.cpp` 分别列出。行数/调用证据见 inventory.json；私人知识库和生成产物仅显示目录用途。\n\n```text\n' + tree(before_entries) + '\n```\n', encoding='utf-8', newline='\n')
new_entries = [(destination(Path(name)), role(Path(name))) for name in names]
new_entries += [
    (Path('src/core/markdown/editing/TextPatch.h'), '已生成草案：最小 UTF-16 文本替换值对象'),
    (Path('src/core/markdown/editing/TextPatch.cpp'), '已生成草案：共用替换范围和完整代理对边界'),
    (Path('src/core/markdown/analysis/MarkdownSnapshot.h'), '已生成草案：指定 Raw/Rendering 的不可变源码快照接口'),
    (Path('src/core/markdown/analysis/MarkdownSnapshot.cpp'), '已生成草案：按需复用分析、语义、图片和代码区查询'),
    (Path('src/application/editing/EditCommandRouter.h'), '已生成草案：语义命令、模式、源码选区、表格地址和处理结果'),
    (Path('src/application/editing/EditCommandRouter.cpp'), '已生成草案：按现有优先级分发核心编辑交易'),
    (Path('src/ui/document/QtTextEditAdapter.h'), '已生成草案：把源码交易应用到原文 QTextDocument 的适配接口'),
    (Path('src/ui/document/QtTextEditAdapter.cpp'), '已生成草案：单次撤销交易及过期源码校验'),
    (Path('src/application/documents/DocumentSessionController.h'), '后续建议：协调原文 undo、文档视图和光标记录，不复制 Manager 节点库'),
    (Path('src/ui/document/MarkdownRenderPipeline.h'), '后续建议：分阶段导入、源码映射、空白恢复和样式应用'),
    (Path('src/ui/document/DocumentSelectionController.h'), '后续建议：框选、标记命中和源码/显示选区状态'),
    (Path('src/ui/document/DocumentImagePresenter.h'), '后续建议：预览图片占位、代次和尺寸，复用现有 ImageLoader'),
    (Path('src/ui/shell/MainWindowLayout.h'), '后续建议：主窗口响应式布局和面板尺寸'),
    (Path('src/bootstrap/CommandLineRunner.h'), '后续建议：导入/导出/截图命令行编排'),
    (Path('src/infrastructure/settings/MarkdownSettingsStore.h'), '后续建议：从 MarkdownSettings.h 独立出持久化类声明'),
    (Path('src/infrastructure/persistence/RelationFileStore.h'), '后续建议：从 MarkdownRelation 拆出 JSON 文件读写'),
    (Path('src/core/knowledge/KnowledgeGraphSnapshot.h'), '后续建议：索引只读输入，解除 KnowledgeIndex 对 MarkdownManager 的依赖'),
    (Path('tests/proposals/ModularEditingTests.cpp'), '已生成并单独编译的草案检查，不替代正式回归'),
    (Path('cmake/Targets.cmake'), '后续建议：显式定义 foundation/infrastructure/application/ui 构建边界'),
]
(OUT / 'TARGET-STRUCTURE.md').write_text('# 调优目标目录及逐文件职责\n\n这是目标设计，不是已迁移结构。现有文件的建议位置逐个映射；已生成草案位于 proposals/modular-editing。标记“后续建议”的类尚未生成。改目录须同步 CMake、include、资源和测试，不可只搬文件。\n\n```text\n' + tree(new_entries) + '\n```\n\n## 迁移条件\n\n- MarkdownRelation 的领域规则留下，JSON 读写迁到 RelationFileStore 后才能隔离领域与磁盘。\n- KnowledgeIndex 必须先接受 KnowledgeGraphSnapshot，不能把依赖 MarkdownManager 的现有实现直接声明为纯领域层。\n- MarkdownSettings.h 拆分数据与 Store 声明，调用方显式包含各自接口。\n- MarkdownManagerRelations.cpp 等仍实现 MarkdownManager 的成员，迁移目录不会自动生成新类。\n- 保留根目录第三方、资源、示例、许可、安装脚本和文档；保留私人知识库的实际目录与配置，不跟随 src 迁移。\n', encoding='utf-8', newline='\n')
print(json.dumps({'summary': payload['summary'], 'largest': [{'path': i['path'], 'lines': i['lines']} for i in sorted(inventory, key=lambda i: i['lines'], reverse=True)[:6]], 'moduleIncludeCycles': components}, ensure_ascii=False, indent=2))
