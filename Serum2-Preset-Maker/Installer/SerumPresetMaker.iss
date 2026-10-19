; Serum 2 Preset Maker — Windows installer (Inno Setup 6)
;
; Build (from the repository's Serum2-Preset-Maker directory):
;   iscc /DAppVersion=1.0.0 /DOutputDir="<path>" Installer\SerumPresetMaker.iss
;
; Installs for the current user only (no admin prompt) to
;   %LOCALAPPDATA%\Programs\Serum 2 Preset Maker
; and adds Start menu (and optional desktop) shortcuts that open the tool in the default browser.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
AppId={{4C8B1F62-0E4D-4C7B-9A54-2D7E61B3A9C5}
AppName=Serum 2 Preset Maker
AppVersion={#AppVersion}
AppVerName=Serum 2 Preset Maker {#AppVersion}
AppPublisher=ME Audio
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Serum 2 Preset Maker installer
VersionInfoProductName=Serum 2 Preset Maker
DefaultDirName={autopf}\Serum 2 Preset Maker
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Serum 2 Preset Maker Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Serum 2 Preset Maker {#AppVersion}

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\index.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\engine.js"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Serum 2 Preset Maker"; Filename: "{app}\index.html"; Comment: "Make Serum 2 presets from a text description"
Name: "{autodesktop}\Serum 2 Preset Maker"; Filename: "{app}\index.html"; Comment: "Make Serum 2 presets from a text description"; Tasks: desktopicon

[Run]
Filename: "{app}\index.html"; Description: "Open Serum 2 Preset Maker now"; Flags: postinstall shellexec nowait skipifsilent
