; ngPost Inno Setup Script

#ifndef IncludeVCRedist
  #define IncludeVCRedist "1"
#endif

#if IncludeVCRedist == "1"
  #define MyOutputSuffix ""
  #define MyExtraExcludes ""
#else
  #define MyOutputSuffix "-no-runtime"
  #define MyExtraExcludes ",vc_redist.x64.exe"
#endif

#ifndef MyAppName
  #define MyAppName "ngPost"
#endif
#ifndef MyAppId
  #define MyAppId "{{0D9414AA-FE44-4964-BB95-0143DBDDBF09}}"
#endif
#define MyAppVersion "5.1.2"
#define MyAppPublisher "spotnet.team"
#define MyAppExeName "ngPost.exe"
#define MyAppDir "..\dist-qt6"
#ifndef MyOutputDir
  #define MyOutputDir "..\release\5.1.2-UNSIGNED"
#endif
#define MyAppIcon "..\src\ngPost.ico"
#ifndef MyPortableBase
  #define MyPortableBase "{userdocs}"
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
VersionInfoVersion=5.1.2.0
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#MyOutputDir}
OutputBaseFilename=ngPost-setup-v{#MyAppVersion}{#MyOutputSuffix}-UNSIGNED
SetupIconFile={#MyAppIcon}
UninstallDisplayIcon={app}\ngPost.ico
PrivilegesRequired=lowest
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LanguageDetectionMethod=none
UsePreviousLanguage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "terms-en.txt"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"; LicenseFile: "voorwaarden.txt"

[CustomMessages]
english.InstallMode=Installation mode
dutch.InstallMode=Installatiemodus
english.ModeTitle=Choose how to use NgPost +.
dutch.ModeTitle=Kies hoe je NgPost + wilt gebruiken.
english.ModeBody=Standard installation stores settings in your user profile. Portable mode stores settings next to ngPost.exe.
dutch.ModeBody=Normale installatie bewaart instellingen in je gebruikersprofiel. Portable modus bewaart instellingen lokaal naast ngPost.exe.
english.StandardMode=Standard installation (recommended)
dutch.StandardMode=Normale installatie (aanbevolen)
english.PortableMode=Portable installation
dutch.PortableMode=Portable installatie
english.PortableFolder=Portable folder
dutch.PortableFolder=Portable map
english.PortableTitle=Where should the portable version be installed?
dutch.PortableTitle=Waar moet de portable versie komen?
english.PortableBody=Choose the folder for the portable version. NgPost + stores its configuration next to ngPost.exe in this mode.
dutch.PortableBody=Kies de map waarin de portable versie geplaatst wordt. In deze modus bewaart NgPost + de configuratie lokaal naast ngPost.exe.
english.InstallRuntime=Installing Visual C++ Redistributable...
dutch.InstallRuntime=Visual C++ Redistributable installeren...

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "installer\*,tmp_test\*,ngPost.conf,portable.mode{#MyExtraExcludes}"
Source: "{#MyAppIcon}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\ngPost.ico"; Check: not IsPortableMode
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\ngPost.ico"; Tasks: desktopicon; Check: not IsPortableMode

[Run]
#if IncludeVCRedist == "1"
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "{cm:InstallRuntime}"; Flags: waituntilterminated; Check: VCRedistNeedsInstall
#endif
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  InstallModePage: TInputOptionWizardPage;
  PortableDirPage: TInputDirWizardPage;

function StandardModeDir: string;
begin
  Result := ExpandConstant('{localappdata}\{#MyAppName}');
end;

function PortableModeDir: string;
begin
  Result := ExpandConstant('{#MyPortableBase}\{#MyAppName}-portable');
end;

function IsPortableMode: Boolean;
begin
  Result := Assigned(InstallModePage) and InstallModePage.Values[1];
end;

procedure ApplyInstallModeDefaults();
var
  CurrentDir: string;
begin
  CurrentDir := WizardForm.DirEdit.Text;

  if IsPortableMode then
  begin
    if (CompareText(CurrentDir, StandardModeDir()) = 0) or (CurrentDir = '') then
      WizardForm.DirEdit.Text := PortableModeDir();
    if Assigned(PortableDirPage) and ((PortableDirPage.Values[0] = '') or (CompareText(PortableDirPage.Values[0], StandardModeDir()) = 0)) then
      PortableDirPage.Values[0] := PortableModeDir();
  end
  else
  begin
    if (CompareText(CurrentDir, PortableModeDir()) = 0) or (CurrentDir = '') then
      WizardForm.DirEdit.Text := StandardModeDir();
  end;
end;

procedure InitializeWizard();
var
  RequestedMode: string;
begin
  InstallModePage := CreateInputOptionPage(
    wpWelcome,
    CustomMessage('InstallMode'),
    CustomMessage('ModeTitle'),
    CustomMessage('ModeBody'),
    True,
    False);
  InstallModePage.Add(CustomMessage('StandardMode'));
  InstallModePage.Add(CustomMessage('PortableMode'));
  RequestedMode := ExpandConstant('{param:MODE|auto}');
  InstallModePage.Values[1] := (CompareText(RequestedMode, 'portable') = 0) or
    ((CompareText(RequestedMode, 'auto') = 0) and FileExists(AddBackslash(WizardForm.DirEdit.Text) + 'portable.mode'));
  InstallModePage.Values[0] := not InstallModePage.Values[1];

  PortableDirPage := CreateInputDirPage(
    InstallModePage.ID,
    CustomMessage('PortableFolder'),
    CustomMessage('PortableTitle'),
    CustomMessage('PortableBody'),
    False,
    SetupMessage(msgNewFolderName));
  PortableDirPage.Add(CustomMessage('PortableFolder') + ':');
  PortableDirPage.Values[0] := WizardForm.DirEdit.Text;
  if (PortableDirPage.Values[0] = '') or
     (CompareText(PortableDirPage.Values[0], StandardModeDir()) = 0) then
    PortableDirPage.Values[0] := PortableModeDir();
  Log('Selected setup language: ' + ActiveLanguage());
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if Assigned(InstallModePage) and (CurPageID = InstallModePage.ID) then
    ApplyInstallModeDefaults();
  if Assigned(PortableDirPage) and (CurPageID = PortableDirPage.ID) then
    WizardForm.DirEdit.Text := PortableDirPage.Values[0];

  Result := True;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if Assigned(InstallModePage) and ((CurPageID = InstallModePage.ID) or (CurPageID = wpSelectDir)) then
    ApplyInstallModeDefaults();
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;

  if Assigned(PortableDirPage) and (PageID = PortableDirPage.ID) and (not IsPortableMode) then
    Result := True
  else if (PageID = wpSelectDir) and IsPortableMode then
    Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  MarkerPath: string;
begin
  if CurStep = ssPostInstall then
  begin
    MarkerPath := ExpandConstant('{app}\portable.mode');
    if IsPortableMode then
      SaveStringToFile(MarkerPath, 'portable=1' + #13#10, False)
    else if FileExists(MarkerPath) then
      DeleteFile(MarkerPath);
  end;
end;

#if IncludeVCRedist == "1"
function VCRedistNeedsInstall: Boolean;
var
  Installed: Cardinal;
begin
  Result := not RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) or (Installed <> 1);
end;
#endif
