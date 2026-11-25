#ifndef AppVersion
#define AppVersion "0.1.0"
#endif

[Setup]
AppId={{20307F45-1493-41B7-93D6-FA3FD75958D3}
AppName=AsciiWall
AppVersion={#AppVersion}
AppPublisher=AsciiWall contributors
AppCopyright=Copyright (c) 2026 AsciiWall contributors
DefaultDirName={localappdata}\Programs\AsciiWall
DefaultGroupName=AsciiWall
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\asciiwall.exe
OutputDir=..\build\installer
OutputBaseFilename=AsciiWall-Setup-{#AppVersion}
SetupIconFile=..\src\asciiwall.ico
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany=AsciiWall contributors
VersionInfoDescription=ASCII animated desktop wallpaper
VersionInfoProductName=AsciiWall
VersionInfoProductVersion={#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "startup"; Description: "Start AsciiWall on startup"; GroupDescription: "Windows startup:"; Flags: unchecked
Name: "desktopsettings"; Description: "Create an AsciiWall Settings shortcut on the desktop"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "..\dist\asciiwall.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\settings.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\scenes\campfire.lua"; DestDir: "{app}\scenes"; Flags: ignoreversion
Source: "..\dist\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\AsciiWall"; Filename: "{app}\asciiwall.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\AsciiWall Settings"; Filename: "{app}\asciiwall.exe"; Parameters: "--settings"; WorkingDir: "{app}"
Name: "{autodesktop}\AsciiWall Settings"; Filename: "{app}\asciiwall.exe"; Parameters: "--settings"; WorkingDir: "{app}"; Tasks: desktopsettings

[Run]
Filename: "{app}\asciiwall.exe"; Parameters: "--install"; Flags: runhidden waituntilterminated; Tasks: startup
Filename: "{app}\asciiwall.exe"; Parameters: "--uninstall"; Flags: runhidden waituntilterminated; Tasks: not startup
Filename: "{app}\asciiwall.exe"; Description: "Launch AsciiWall now"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\asciiwall.exe"; Parameters: "--quit"; Flags: runhidden waituntilterminated; RunOnceId: "StopAsciiWall"
Filename: "{app}\asciiwall.exe"; Parameters: "--uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAsciiWallStartup"

[Code]
function IsStartupEnabled: Boolean;
var
  StartupCommand: string;
begin
  Result := RegQueryStringValue(
    HKEY_CURRENT_USER,
    'Software\Microsoft\Windows\CurrentVersion\Run',
    'AsciiWall',
    StartupCommand
  );
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectTasks) and (WizardForm.TasksList.Items.Count > 0) then
    WizardForm.TasksList.Checked[0] := IsStartupEnabled;
end;
