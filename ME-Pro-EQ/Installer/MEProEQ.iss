; ME Pro EQ — Windows installer (Inno Setup 6)
;
; Build (from the repository's ME-Pro-EQ directory, after the VST3 has been built AND validated):
;   iscc /DAppVersion=0.1.0 /DBundleDir="<path>\ME Pro EQ.vst3" /DOutputDir="<path>" Installer\MEProEQ.iss
;
; Installs the 64-bit VST3 bundle to the standard location  C:\Program Files\Common Files\VST3\ME Pro EQ.vst3
; and documentation/licences to                            C:\Program Files\ME Audio\ME Pro EQ
; User presets live in Documents\ME Audio\ME Pro EQ and are NOT removed on uninstall unless the user
; explicitly confirms it.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BundleDir
  #define BundleDir "..\build\MEProEQ_artefacts\Release\VST3\ME Pro EQ.vst3"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
AppId={{77EC4E29-55C4-442B-838F-E17BDAB9B1F2}
AppName=ME Pro EQ
AppVersion={#AppVersion}
AppVerName=ME Pro EQ {#AppVersion}
AppPublisher=ME Audio
AppCopyright=Copyright (c) ME Audio
VersionInfoVersion={#AppVersion}
VersionInfoCompany=ME Audio
VersionInfoDescription=ME Pro EQ installer
VersionInfoProductName=ME Pro EQ
DefaultDirName={autopf}\ME Audio\ME Pro EQ
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=ME Pro EQ Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSES\ME_PRO_EQ_LICENSE.txt
InfoBeforeFile=..\Documentation\Installation.txt
UninstallDisplayName=ME Pro EQ {#AppVersion}
CloseApplications=yes

[Files]
; The complete VST3 bundle (binary + moduleinfo.json)
Source: "{#BundleDir}\*"; DestDir: "{commoncf64}\VST3\ME Pro EQ.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Documentation and licences
Source: "..\LICENSES\ME_PRO_EQ_LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSES\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSES\VST3_SDK_MIT.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Documentation\UserManual.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Documentation\Installation.txt"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
; Only what the installer itself created. User presets are handled in [Code] below (opt-in).
Type: dirifempty; Name: "{commoncf64}\VST3\ME Pro EQ.vst3"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  PresetDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    PresetDir := ExpandConstant('{userdocs}\ME Audio\ME Pro EQ');
    if DirExists(PresetDir) then
    begin
      if MsgBox('Also delete your ME Pro EQ user presets and settings?' + #13#10 + #13#10 + PresetDir + #13#10 + #13#10 +
                'Choose "No" to keep them (recommended).', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(PresetDir, True, True, True);
    end;
  end;
end;
