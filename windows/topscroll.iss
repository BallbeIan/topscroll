; Inno Setup script for the Windows installer. Built by .github/workflows/windows.yml:
;   iscc /DAppVersion=0.1.0 /DSourceDir=<windeployqt output> windows\topscroll.iss
; Installs per user (no administrator rights), starts the background service
; right away and at every login, and stops it again on uninstall.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\topscroll"
#endif

[Setup]
AppId={{6F3C2A8E-4B1D-4E7A-9C55-0D2B7A51E9F3}
AppName=topscroll
AppVersion={#AppVersion}
AppVerName=topscroll {#AppVersion}
AppPublisher=BallbeIan
AppPublisherURL=https://github.com/BallbeIan/topscroll
AppSupportURL=https://github.com/BallbeIan/topscroll/issues
DefaultDirName={localappdata}\Programs\topscroll
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=topscroll-{#AppVersion}-windows-x64-setup
SetupIconFile=topscroll.ico
UninstallDisplayIcon={app}\topscroll.exe
UninstallDisplayName=topscroll
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\topscroll"; Filename: "{app}\topscroll.exe"; Comment: "Scroll with one finger"

[Registry]
; Start the background service at every login.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "topscroll"; ValueData: """{app}\topscroll.exe"" daemon"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\topscroll.exe"; Parameters: "daemon"; Flags: nowait
Filename: "{app}\topscroll.exe"; Description: "Open topscroll"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM topscroll.exe"; Flags: runhidden; RunOnceId: "StopTopscroll"

[Code]
// An upgrade must stop the running service first, or its files can't be replaced.
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM topscroll.exe', '', SW_HIDE,
         ewWaitUntilTerminated, ResultCode);
end;
