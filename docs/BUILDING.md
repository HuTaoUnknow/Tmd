# 构建与发布 / Build and release

## 中文

使用 C++17、CMake 3.21+、Ninja、Qt 6.11.1 MinGW 64 位与匹配的 MinGW 工具链。本版在该组合下构建。安装程序由 Inno Setup 6.7.3 编译。

### Qt Creator

打开 `CMakeLists.txt`，选择 Qt MinGW 64 位 Kit，配置并构建 `tree_md`。首次开发运行从 `examples/` 创建本地 `md_data` / `md_photo`，已有知识库保留。这两个工作目录已被 Git 忽略。

### 命令行

在包含 Qt、MinGW、CMake 和 Ninja 的终端运行：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt/6.11.1/mingw_64" -DBUILD_TESTING=ON
cmake --build build-release --parallel 4
```

可以用 `-DBUILD_TESTING=OFF` 只构建应用。测试使用 Qt Test 与临时目录，不修改实际知识库。

### 生成安装包

安装 [Inno Setup](https://jrsoftware.org/isdl.php)，然后运行：

```powershell
.\scripts\Build-Release.ps1 -QtDirectory "D:\Qt\6.11.1\mingw_64" -InnoCompiler "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
```

脚本依次构建 Release、部署测试运行依赖、串行运行九组回归检查、部署程序依赖、创建便携 ZIP、编译安装包并生成 SHA256 校验文件。默认输出到 `dist/`。`-BuildDirectory` 和 `-OutputDirectory` 可指定其他位置。

`-SkipBuild` 用于已有构建；`-SkipTests` 仅适合已经验证的同一构建。发布包始终使用 `examples/` 中的五篇公开示例，不打包开发者的本地知识库。

### 发布约定

更新 `CMakeLists.txt` 中的版本和 `CHANGELOG.md`，完整运行构建与安装验证，再建立 `vX.Y.Z` 标签。GitHub Release 上传 Setup、Portable、源代码包和 `SHA256SUMS.txt`，使用中英文说明。

仅源码发布时，完成源码构建与检查、建立版本标签并发布中英文说明即可，不调用安装包生成脚本。v1.0.2 为历史源码发布；当前 v1.0.3 同时提供安装版和便携版。生成新版安装包时执行安装、升级及卸载验证。

`.github/workflows/build.yml` 在推送或 PR 时进行 Windows 构建与测试，并保存测试记录。初版安装包在本地完成实际安装、升级、注册与卸载检查后发布。

## English

Use C++17, CMake 3.21+, Ninja, Qt 6.11.1 for MinGW 64-bit and a matching MinGW toolchain. This release was built with that combination. Inno Setup 6.7.3 builds the installer.

Open `CMakeLists.txt` in Qt Creator, select the Qt MinGW 64-bit kit, and build the `tree_md` target. Development startup seeds a new local library from `examples/`; existing libraries are preserved. Local `md_data` and `md_photo` are ignored by Git.

From a configured development terminal, use the commands above or run `scripts/Build-Release.ps1` with your Qt and ISCC paths. The script builds Release, deploys test runtimes, runs nine regression groups serially, deploys application runtimes, creates the portable ZIP, compiles Setup and writes SHA256 checksums to `dist/`.

`-BuildDirectory` and `-OutputDirectory` customize locations. `-SkipBuild` reuses a build; `-SkipTests` is intended only for an already verified identical build. Packages contain the reviewed starter files from `examples/`, never the developer's working library.

For a release, update the CMake version and changelog, validate build and installation behavior, tag `vX.Y.Z`, and upload Setup, Portable, source and checksums with bilingual release notes. The GitHub workflow performs Windows builds/tests for pushes and pull requests.

For a source-only release, build and check the source, create the version tag and publish bilingual notes without running the installer generation script. v1.0.2 was a source-only release; v1.0.3 includes installer and portable downloads. Installation, upgrade and removal checks are required when a new installer is produced.
