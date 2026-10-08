# 安装、升级与卸载 / Installation, upgrade and removal

## 中文

### 运行环境与下载

- Windows 10 或 Windows 11，64 位桌面环境；本版在 Windows 11 验证。
- 安装包包含 Qt 和 MinGW 运行依赖，不需要自行安装开发环境。
- 从 [GitHub Releases](https://github.com/HuTaoUnknow/Tmd/releases/latest) 下载安装程序或便携 ZIP，以及 `SHA256SUMS.txt`。
- 本版安装包未使用商业代码签名证书。下载后可用下面的命令核对文件摘要与 Release 中的校验文件。

```powershell
Get-FileHash .\Tmd-1.0.3-Windows-x64-Setup.exe -Algorithm SHA256
```

### 安装向导

1. 选择简体中文或 English。
2. 选择仅为当前用户安装，或为所有用户安装。默认当前用户；为所有用户安装需要管理员权限。
3. 阅读授权说明，设置**应用登记名称**，默认 `Tmd`。此名称用于快捷方式和 Windows 应用列表，不改变程序或知识库格式。
4. 选择**程序安装位置**。当前用户默认安装到本地应用目录中的 `Programs\Tmd`；所有用户模式使用 Program Files。
5. 选择开始菜单文件夹。
6. 勾选或取消**桌面快捷方式**、**开始菜单快捷方式**、**Windows 应用列表登记**。
7. 核对安装摘要并安装；完成页可选择立即启动。

如果取消应用列表登记，仍会生成安装目录中的 `unins000.exe`，可以直接运行它卸载。

### 知识库位置

安装版首次启动使用系统“文档”目录下的 `Tmd`：

```text
文档/Tmd/md_data
文档/Tmd/md_photo
```

“文档”目录可能被 Windows 重定向到其他磁盘或 OneDrive，实际位置以系统目录为准。示例只在创建新知识库时初始化；删除示例后，后续启动和升级不会重新生成。已有知识库不会被默认示例覆盖。

使用其他知识库时，可以创建快捷方式并增加：

```powershell
tree_md.exe --data-dir "D:\MyNotes\md_data"
```

此时图片目录为 `D:\MyNotes\md_photo`。需要初始化一个新知识库，可以使用：

```powershell
tree_md.exe --library-root "D:\MyNotes"
```

### 便携版

完整解压 `Tmd-1.0.3-Windows-x64-Portable.zip` 到有写入权限的目录，再运行 `tree_md.exe`。不要只移动 EXE，其他 DLL、插件、说明和图片也需要保留。

便携版在程序旁使用 `md_data` 与 `md_photo`。迁移时关闭程序，一起移动整个目录即可；放到 Program Files 等受保护目录时，改用安装版或明确指定可写知识库。

### 升级、备份与卸载

- 升级前关闭 Tmd，备份整个 `md_data` / `md_photo`，包含隐藏的 `.tree-md-relations.json`。
- 安装新版到同一程序目录。安装程序更新程序文件，安装版个人知识库保留。
- 通过 Windows“已安装的应用”中的登记名称卸载，或运行程序目录内的 `unins000.exe`。
- 卸载移除程序、快捷方式及登记信息；系统“文档/Tmd”中的个人数据保留。
- 从开发版或便携版迁移时，把两组数据目录完整复制到“文档/Tmd”，再启动安装版。不要把包含个人关系的文件只复制一半。

### 静默安装

```powershell
Tmd-1.0.3-Windows-x64-Setup.exe /CURRENTUSER /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="D:\Apps\Tmd" /APPNAME="Tmd" /TASKS="desktopicon,startmenu,registerapp"
```

`/TASKS=""` 可取消全部三个可选项；`/ALLUSERS` 选择所有用户模式；`/LANG=en` 选择英文。静默安装不会自动启动 Tmd。

## English

### Requirements and downloads

Use a 64-bit Windows 10/11 desktop environment. This release was verified on Windows 11. Qt and MinGW runtimes are bundled.

Download the Setup EXE or portable ZIP and `SHA256SUMS.txt` from [Releases](https://github.com/HuTaoUnknow/Tmd/releases/latest). This release does not have a commercial code-signing certificate. Compare the downloaded file's SHA256 with the published checksum using `Get-FileHash`.

### Installer steps

1. Choose Simplified Chinese or English.
2. Select current-user or all-users installation. Current-user is the default; all-users requires administrator rights.
3. Read the license and choose the application display name, defaulting to `Tmd`.
4. Choose an installation directory. Current-user installation defaults to `Programs\Tmd` under Local AppData; all-users installation uses Program Files.
5. Choose a Start menu folder.
6. Select optional desktop/Start menu shortcuts and registration in Windows Installed apps.
7. Review the summary and install. The finish page offers to launch Tmd.

The display name changes shortcuts and the Installed apps entry. It does not change the document format. If application registration is disabled, uninstall through `unins000.exe` in the installation directory.

### Data storage

The installed edition initializes a library under the current user's **Documents/Tmd**, with `md_data` and `md_photo` folders. Windows may redirect Documents to another drive or OneDrive.

Starter files are initialized once for a new library. Existing libraries are preserved, and deleted examples are not recreated on later launches or upgrades. Application files and personal documents are stored separately.

Use `tree_md.exe --data-dir "D:\MyNotes\md_data"` to open an existing library, or `--library-root "D:\MyNotes"` to initialize a new one. Its image folder is `D:\MyNotes\md_photo`.

### Portable use

Extract the entire portable ZIP into a writable directory and run `tree_md.exe`. Keep all DLLs and plugin folders. The portable edition uses `md_data` and `md_photo` beside the executable. Close the application before moving the complete folder.

### Upgrade, backup and uninstall

Close Tmd and back up both data folders, including `.tree-md-relations.json`. Install the next version to the same application directory. Uninstalling removes the application, shortcuts and registration while preserving the installed edition's **Documents/Tmd** library.

To migrate from a development or portable library, copy both data folders into Documents/Tmd before starting the installed edition. Preserve relative paths and relationship metadata.

### Unattended installation

```powershell
Tmd-1.0.3-Windows-x64-Setup.exe /CURRENTUSER /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="D:\Apps\Tmd" /APPNAME="Tmd" /TASKS="desktopicon,startmenu,registerapp" /LANG=en
```

`/TASKS=""` disables the three optional tasks. `/ALLUSERS` selects administrator installation. Silent installation does not launch the application.
