; Serum 2 Preset Maker for FL Studio — Windows installer (Inno Setup 6)
;
; Build (from the repository's Serum2-Preset-Maker directory):
;   iscc /DAppVersion=3.0.0 /DOutputDir="<path>" Installer\SerumPresetMaker.iss
;
; Installs for the current user only (no admin prompt) to
;   %LOCALAPPDATA%\Programs\Serum 2 Preset Maker
; adds Start menu (and optional desktop) shortcuts that open the tool in its own window (Microsoft Edge's
; app mode, which every Windows 10/11 PC has; the default browser if Edge is missing), and
; creates the folder the tool saves presets to:
;   Documents\Xfer\Serum 2 Presets\Presets\User\Preset Maker   when Serum 2's presets folder exists
;   Documents\Serum 2 Preset Maker\Presets                      otherwise
; That folder is written to {app}\config.js for the tool, and is kept on uninstall (it holds the user's presets).
; FL Studio's own settings are not changed: the user adds the folder to FL Studio's Browser (see FL-Studio-Setup.txt).

#ifndef AppVersion
  #define AppVersion "3.0.0"
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
VersionInfoDescription=Serum 2 Preset Maker for FL Studio installer
VersionInfoProductName=Serum 2 Preset Maker
DefaultDirName={autopf}\Serum 2 Preset Maker
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Serum 2 Preset Maker for FL Studio Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoAfterFile=FL-Studio-Setup.txt
UninstallDisplayName=Serum 2 Preset Maker {#AppVersion}
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\icon.ico

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\index.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\engine.js"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\synth.js"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\app.js"; DestDir: "{app}"; Flags: ignoreversion
; Replaced after install with the real presets folder (see CurStepChanged).
Source: "..\config.js"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "FL-Studio-Setup.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Serum 2 Preset Maker\Serum 2 Preset Maker"; Filename: "{code:LauncherExe}"; Parameters: "{code:LauncherParams}"; WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"; Comment: "Make Serum 2 presets from a text description"
Name: "{autoprograms}\Serum 2 Preset Maker\Presets folder"; Filename: "{code:GetPresetDir}"; Comment: "The folder your presets are saved to"
Name: "{autoprograms}\Serum 2 Preset Maker\FL Studio setup steps"; Filename: "{app}\FL-Studio-Setup.txt"
Name: "{autodesktop}\Serum 2 Preset Maker"; Filename: "{code:LauncherExe}"; Parameters: "{code:LauncherParams}"; WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"; Comment: "Make Serum 2 presets from a text description"; Tasks: desktopicon

[Run]
Filename: "{code:GetPresetDir}"; Description: "Open the presets folder (to drag into FL Studio's Browser)"; Flags: postinstall shellexec nowait skipifsilent
Filename: "{code:LauncherExe}"; Parameters: "{code:LauncherParams}"; Description: "Open Serum 2 Preset Maker now"; Flags: postinstall shellexec nowait skipifsilent

[InstallDelete]
; Version 1.0 put the Start menu shortcut directly in Programs; it now lives in a folder.
Type: files; Name: "{autoprograms}\Serum 2 Preset Maker.lnk"

[Code]
var
  PresetDir: String;

// Microsoft Edge, used to open the tool in its own app window.
function EdgePath(): String;
var
  Candidates: array of String;
  I: Integer;
begin
  Result := '';
  SetArrayLength(Candidates, 3);
  Candidates[0] := ExpandConstant('{commonpf32}\Microsoft\Edge\Application\msedge.exe');
  if IsWin64 then
    Candidates[1] := ExpandConstant('{commonpf64}\Microsoft\Edge\Application\msedge.exe');
  Candidates[2] := ExpandConstant('{localappdata}\Microsoft\Edge\Application\msedge.exe');
  for I := 0 to GetArrayLength(Candidates) - 1 do
    if (Candidates[I] <> '') and FileExists(Candidates[I]) then
    begin
      Result := Candidates[I];
      Exit;
    end;
end;

// file:///C:/Users/.../index.html with spaces escaped.
function PageUrl(): String;
begin
  Result := ExpandConstant('{app}\index.html');
  StringChangeEx(Result, '\', '/', True);
  StringChangeEx(Result, '%', '%25', True);
  StringChangeEx(Result, ' ', '%20', True);
  StringChangeEx(Result, '#', '%23', True);
  Result := 'file:///' + Result;
end;

function LauncherExe(Param: String): String;
begin
  Result := EdgePath();
  if Result = '' then
    Result := ExpandConstant('{app}\index.html');
end;

function LauncherParams(Param: String): String;
begin
  if EdgePath() <> '' then
    Result := '--app="' + PageUrl() + '" --window-size=1280,900'
  else
    Result := '';
end;

function GetPresetDir(Param: String): String;
var
  SerumPresets: String;
begin
  if PresetDir = '' then
  begin
    SerumPresets := ExpandConstant('{userdocs}\Xfer\Serum 2 Presets');
    if DirExists(SerumPresets) then
      PresetDir := SerumPresets + '\Presets\User\Preset Maker'
    else
      PresetDir := ExpandConstant('{userdocs}\Serum 2 Preset Maker\Presets');
  end;
  Result := PresetDir;
end;

// A JavaScript string literal in plain ASCII (non-ASCII characters become \uXXXX).
function JsString(S: String): String;
var
  I: Integer;
  C: Char;
begin
  Result := '"';
  for I := 1 to Length(S) do
  begin
    C := S[I];
    if (C = '\') or (C = '"') then
      Result := Result + '\' + C
    else if (Ord(C) < 32) or (Ord(C) > 126) then
      Result := Result + '\u' + Format('%.4x', [Ord(C)])
    else
      Result := Result + C;
  end;
  Result := Result + '"';
end;

function JsBool(B: Boolean): String;
begin
  if B then Result := 'true' else Result := 'false';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Dir, Js: String;
  InSerum, HasFL: Boolean;
begin
  // Create the folder before the shortcuts to it are made.
  if CurStep = ssInstall then
  begin
    if not ForceDirectories(GetPresetDir('')) then
      Log('Could not create presets folder: ' + GetPresetDir(''));
  end;
  if CurStep = ssPostInstall then
  begin
    Dir := GetPresetDir('');
    InSerum := Pos('\Xfer\Serum 2 Presets\', Dir) > 0;
    HasFL := RegKeyExists(HKCU, 'Software\Image-Line') or DirExists(ExpandConstant('{userdocs}\Image-Line'));
    Js := '// Written by the Serum 2 Preset Maker installer.' + #13#10 +
          'window.PRESET_MAKER_CONFIG = { presetDir: ' + JsString(Dir) +
          ', insideSerumPresets: ' + JsBool(InSerum) + ', flStudioFound: ' + JsBool(HasFL) + ' };' + #13#10;
    if not SaveStringToFile(ExpandConstant('{app}\config.js'), Js, False) then
      Log('Could not write config.js');
    Log('Presets folder: ' + Dir);
  end;
end;
