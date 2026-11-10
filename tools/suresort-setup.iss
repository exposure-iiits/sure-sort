; Build with: ISCC /DAppSource="...\dist\Sure Sort 0.1.3" suresort-setup.iss
#ifndef AppSource
  #define AppSource "..\dist\Sure Sort 0.1.3"
#endif
#define AppVersion GetStringFileInfo(AppSource + "\Sure Sort.exe", "FileVersion")

[Setup]
AppId={{B72F27AF-2AD6-4F71-B813-26945EF06C5F}
AppName=Sure Sort
AppVersion={#AppVersion}
AppVerName=Sure Sort {#AppVersion}
AppPublisher=Sure Sort
DefaultDirName={localappdata}\Programs\Sure Sort
DefaultGroupName=Sure Sort
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=Sure-Sort-{#AppVersion}-Setup-Windows-x64
SetupIconFile=..\assets\suresort.ico
UninstallDisplayIcon={app}\Sure Sort.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
DisableWelcomePage=no
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Sure Sort Installer

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#AppSource}\*"; DestDir: "{app}"; Excludes: "data\*,*.sqlite,*.sqlite-*,*.lock"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Sure Sort"; Filename: "{app}\Sure Sort.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Sure Sort"; Filename: "{app}\Sure Sort.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Sure Sort.exe"; Description: "Open Sure Sort"; Flags: nowait postinstall skipifsilent
