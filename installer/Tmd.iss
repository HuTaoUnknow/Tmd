#ifndef AppVersion
  #define AppVersion "1.0.2"
#endif
#ifndef PayloadDir
  #define PayloadDir "..\package\installed"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{5014DC17-3B1B-4B86-A344-7E40D7296BF4}
AppName=Tmd
AppVersion={#AppVersion}
AppVerName=Tmd {#AppVersion}
AppPublisher=HuTaoUnknow
AppPublisherURL=https://github.com/HuTaoUnknow/Tmd
AppSupportURL=https://github.com/HuTaoUnknow/Tmd/issues
AppUpdatesURL=https://github.com/HuTaoUnknow/Tmd/releases
DefaultDirName={autopf}\Tmd
DefaultGroupName={code:GetDisplayName}
DisableDirPage=no
DisableProgramGroupPage=no
AllowNoIcons=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Tmd-{#AppVersion}-Windows-x64-Setup
SetupIconFile=..\resources\tmd.ico
UninstallDisplayIcon={app}\tree_md.exe
UninstallDisplayName={code:GetDisplayName}
CreateUninstallRegKey=WizardIsTaskSelected('registerapp')
Uninstallable=yes
UninstallFilesDir={app}
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=115
UsePreviousAppDir=yes
UsePreviousTasks=yes
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}
VersionInfoProductName=Tmd
VersionInfoDescription=Tmd Windows Installer
VersionInfoCompany=HuTaoUnknow
VersionInfoCopyright=Copyright (c) 2026 HuTaoUnknow

[Languages]
Name: "zhcn"; MessagesFile: "languages\ChineseSimplified.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
zhcn.NameTitle=应用登记名称
zhcn.NameDescription=设置 Windows 应用列表和快捷方式中显示的名称。
zhcn.NamePrompt=默认名称为 Tmd。此名称不改变知识库内容；可以在下一页选择程序安装位置。
zhcn.NameLabel=应用名称：
zhcn.InvalidName=请输入 1～60 个字符的名称，不能使用路径分隔符、文件名保留符号或末尾的点。
zhcn.OptionsGroup=快捷方式和应用登记：
zhcn.DesktopTask=创建桌面快捷方式
zhcn.StartMenuTask=创建开始菜单快捷方式
zhcn.RegisterTask=在 Windows 已安装的应用列表中登记，并允许通过名称启动
zhcn.LaunchTask=启动 Tmd
zhcn.UninstallShortcut=卸载
zhcn.DataInfo=个人知识库：%1\Tmd。首次启动初始化示例，升级与卸载保留个人文档和图片。
en.NameTitle=Application display name
en.NameDescription=Choose the name shown in Windows Installed apps and shortcuts.
en.NamePrompt=The default name is Tmd. This does not change your library. Choose the installation directory on the next page.
en.NameLabel=Application name:
en.InvalidName=Enter a name of 1 to 60 characters without path separators, reserved filename characters or a trailing period.
en.OptionsGroup=Shortcuts and application registration:
en.DesktopTask=Create a desktop shortcut
en.StartMenuTask=Create Start menu shortcuts
en.RegisterTask=Register in Windows Installed apps and allow launching by name
en.LaunchTask=Launch Tmd
en.UninstallShortcut=Uninstall
en.DataInfo=Personal library: %1\Tmd. Examples are initialized on first launch; upgrades and removal preserve your documents and images.

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopTask}"; GroupDescription: "{cm:OptionsGroup}"; Flags: checkedonce
Name: "startmenu"; Description: "{cm:StartMenuTask}"; GroupDescription: "{cm:OptionsGroup}"
Name: "registerapp"; Description: "{cm:RegisterTask}"; GroupDescription: "{cm:OptionsGroup}"

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\{code:GetDisplayName}"; Filename: "{app}\tree_md.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{group}\{code:GetDisplayName}"; Filename: "{app}\tree_md.exe"; WorkingDir: "{app}"; Tasks: startmenu
Name: "{group}\{cm:UninstallShortcut} {code:GetDisplayName}"; Filename: "{uninstallexe}"; Tasks: startmenu

[Registry]
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\Tmd.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\tree_md.exe"; Tasks: registerapp; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\Tmd.exe"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Tasks: registerapp

[INI]
Filename: "{app}\tmd-installation.ini"; Section: "Installation"; Key: "Mode"; String: "installed"; Flags: uninsdeletesection
Filename: "{app}\tmd-installation.ini"; Section: "Installation"; Key: "DisplayName"; String: "{code:GetDisplayName}"
Filename: "{app}\tmd-installation.ini"; Section: "Installation"; Key: "Version"; String: "{#AppVersion}"

[Run]
Filename: "{app}\tree_md.exe"; Description: "{cm:LaunchTask}"; Flags: nowait postinstall skipifsilent

[Code]
var
  NamePage: TInputQueryWizardPage;

function GetDisplayName(Param: String): String;
begin
  if Assigned(NamePage) then Result := Trim(NamePage.Values[0])
  else Result := ExpandConstant('{param:APPNAME|Tmd}');
end;

function NameIsValid: Boolean;
var
  Name: String;
  DeviceName: String;
  I: Integer;
begin
  Name := GetDisplayName('');
  Result := (Length(Name) >= 1) and (Length(Name) <= 60);
  if not Result then Exit;
  if (Name = '.') or (Name = '..') or (Name[Length(Name)] = '.') then begin Result := False; Exit; end;
  DeviceName := Uppercase(Name);
  if Pos('.', DeviceName) > 0 then DeviceName := Copy(DeviceName, 1, Pos('.', DeviceName) - 1);
  if (DeviceName = 'CON') or (DeviceName = 'PRN') or (DeviceName = 'AUX') or (DeviceName = 'NUL') then begin Result := False; Exit; end;
  if Length(DeviceName) = 4 then
    if ((Copy(DeviceName, 1, 3) = 'COM') or (Copy(DeviceName, 1, 3) = 'LPT')) and
      (DeviceName[4] >= '1') and (DeviceName[4] <= '9') then begin Result := False; Exit; end;
  for I := 1 to Length(Name) do
    if (Ord(Name[I]) < 32) or (Pos(Name[I], '\/:*?"<>|') > 0) then begin Result := False; Exit; end;
end;

procedure InitializeWizard;
begin
  NamePage := CreateInputQueryPage(wpLicense, CustomMessage('NameTitle'),
    CustomMessage('NameDescription'), CustomMessage('NamePrompt'));
  NamePage.Add(CustomMessage('NameLabel'), False);
  NamePage.Values[0] := ExpandConstant('{param:APPNAME|' + GetPreviousData('DisplayName', 'Tmd') + '}');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = NamePage.ID) and not NameIsValid then begin
    SuppressibleMsgBox(CustomMessage('InvalidName'), mbError, MB_OK, IDOK);
    Result := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not NameIsValid then Result := CustomMessage('InvalidName');
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'DisplayName', GetDisplayName(''));
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := GetDisplayName('') + NewLine + NewLine + MemoDirInfo + NewLine + NewLine;
  if MemoGroupInfo <> '' then Result := Result + MemoGroupInfo + NewLine + NewLine;
  if MemoTasksInfo <> '' then Result := Result + MemoTasksInfo + NewLine + NewLine;
  Result := Result + FmtMessage(CustomMessage('DataInfo'), [ExpandConstant('{userdocs}')]);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  RootKey: Integer;
begin
  if (CurStep = ssPostInstall) and not WizardIsTaskSelected('registerapp') then begin
    if IsAdminInstallMode then RootKey := HKLM else RootKey := HKCU;
    RegDeleteKeyIncludingSubkeys(RootKey, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{5014DC17-3B1B-4B86-A344-7E40D7296BF4}_is1');
    RegDeleteKeyIncludingSubkeys(RootKey, 'Software\Microsoft\Windows\CurrentVersion\App Paths\Tmd.exe');
  end;
end;
