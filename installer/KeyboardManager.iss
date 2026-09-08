#define MyAppName "KeyboardManager"
#define MyAppVersion "1.0.1"
#define MyAppPublisher "Moltrax"
#define MyAppExeName "KeyboardManager.exe"
#define MyAppMutex "KeyboardManager.Singleton"
#ifndef BuildOutputDir
  #define BuildOutputDir "..\build\Release"
#endif

[Setup]
AppId={{8BAA9C19-7B56-4CC0-9CC2-D734B908933A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=KeyboardManager-{#MyAppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UsePreviousAppDir=yes
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter={#MyAppExeName}
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#BuildOutputDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Sounds\*"; DestDir: "{app}\Sounds"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildOutputDir}\Sounds\*"; DestDir: "{app}\Sounds"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Dirs]
Name: "{app}\Sounds"; Permissions: users-modify

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
function StopRunningApplication(): Boolean;
var
  ResultCode: Integer;
  Attempts: Integer;
  ExistingExe: String;
begin
  ExistingExe := ExpandConstant('{app}\{#MyAppExeName}');
  if FileExists(ExistingExe) then
    Exec(ExistingExe, '/shutdown', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  Attempts := 0;
  while CheckForMutexes('{#MyAppMutex}') and (Attempts < 50) do
  begin
    Sleep(100);
    Attempts := Attempts + 1;
  end;

  Result := not CheckForMutexes('{#MyAppMutex}');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not StopRunningApplication() then
    Result := '{#MyAppName} could not be closed safely. Exit it from the system tray and retry.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := StopRunningApplication();
  if not Result then
    MsgBox('{#MyAppName} could not be closed safely. Exit it from the system tray and retry.',
      mbError, MB_OK);
end;
