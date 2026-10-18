# 项目协作约定

## 用户要求的完成流程

- 每次完成项目修改后，说明修改结果与实际验证情况，完成必要的 Debug 构建；不再例行询问是否打开 Debug 或是否发布版本。
- 桌面的 `Tmd Debug` 快捷方式通过 `scripts/Start-Debug.ps1` 启动当前编译的 Debug，用户自行点击。仅在用户明确要求启动时直接打开应用。
- GitHub 同步、版本发布和生成安装包按用户的明确指示执行，不自动发布，也不例行发起发布询问；已明确授权的步骤继续执行，不重复询问。
- 发布前完成必要的构建、测试和版本资料准备。用户要求发布后，按项目发布约定更新版本号、更新记录、Git 标签和 GitHub Release，并将新版安装包放到系统实际桌面目录。
- 保留工作区已有修改及本地知识库。发布只使用 `examples/` 中的公开示例，不能打包开发者的 `md_data/`、`md_photo/` 或本机凭据。

## 项目入口

- Tmd 是 C++17 / Qt Widgets 桌面 Markdown 知识库，程序目标为 `tree_md`。
- `core/` 负责 Markdown、文档库、知识关系、图片、设置和平台能力；`ui/` 负责界面及交互。模块边界见 `docs/ARCHITECTURE.md`。
- 新增编译源文件时显式登记到 `CMakeLists.txt`。综合测试新增用例需登记到 `tests/RegressionGroups.cmake`。
- 构建与发布约定见 `docs/BUILDING.md`，打包脚本为 `scripts/Build-Release.ps1`，Inno Setup 配置为 `installer/Tmd.iss`。

## 本机环境（2026-10-10 核实）

- Windows 11 x64，PowerShell 7.6.5。
- Qt：`D:/PC/EXE/QtCreator/6.11.1/mingw_64`，版本 6.11.1。
- MinGW：`D:/PC/EXE/QtCreator/Tools/mingw1310_64/bin`，GCC 13.1.0。
- CMake / CTest：`D:/PC/EXE/QtCreator/Tools/CMake_64/bin`，版本 3.30.5。
- Ninja：`D:/PC/EXE/QtCreator/Tools/Ninja/ninja.exe`，版本 1.12.1。
- Debug：`build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug`，生成器为 MinGW Makefiles，`BUILD_TESTING=ON`。
- 现有 Release：`build-release-v103`，生成器为 Ninja，`BUILD_TESTING=ON`。不能直接切换已有构建目录的生成器。
- 当前终端 PATH 未包含上述 Qt 构建工具，也未发现 `gh` 或 `ISCC.exe`；后续使用前应核实工具路径及 GitHub 身份状态。
- Git 远程：`https://github.com/HuTaoUnknow/Tmd.git`。核实时分支为 `main`，源码版本为 1.0.3；后续以实际仓库状态为准。
- 现有发布产物位于 `dist/v1.0.3/`。桌面路径应通过 `[Environment]::GetFolderPath('Desktop')` 获取。

## 本机验证方式

使用上述完整路径调用 CMake / CTest，并为当前进程设置 Qt 与 MinGW 的 `bin` 路径，以及 Qt 的 `plugins/platforms` 路径。Debug 构建命令为 `cmake --build build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug --parallel 4`，回归命令为 `ctest --test-dir build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug --output-on-failure -j 1`，目前登记了十五组测试（含四组编辑交互审查、一组语法规则回归及一组模块化编辑回归），各组 QtTest 明细保存到构建目录中同名 `.txt` 文件。

受限执行环境中，默认临时目录曾使 Qt 测试无法创建子目录。运行测试前将当前进程的 `TEMP` 和 `TMP` 指向构建目录内新建的可写临时目录。测试使用临时知识库与隔离配置。若 CTest 未捕获到 QtTest 输出，对相应测试程序传入 `-o <日志路径>,txt` 获取明细。记录实际通过、失败或环境限制，不把历史日志视为本次验证结果。
