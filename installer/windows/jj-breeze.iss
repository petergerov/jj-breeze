; Inno Setup script for the jj-breeze Windows installer.
;
; Not built directly — scripts/dist-windows.ps1 compiles this and passes the
; values below in with /D, so the product name and version stay sourced from
; CMakeLists.txt rather than being duplicated here:
;
;   ISCC.exe /DProductName=... /DAppVersion=... /DArtefactsDir=... /DOutputDir=... jj-breeze.iss
;
; Requires Inno Setup 6.3 or newer (for "x64compatible").

#ifndef ProductName
  #error ProductName must be passed in with /DProductName=...
#endif
#ifndef AppVersion
  #error AppVersion must be passed in with /DAppVersion=...
#endif
#ifndef ArtefactsDir
  #error ArtefactsDir must be passed in with /DArtefactsDir=...
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

#define AppPublisher "Gerov"

[Setup]
; Never change AppId once released — Windows keys upgrades and uninstalls off
; it, and a new value would leave the previous version installed alongside
; this one instead of replacing it.
AppId={{321C4CBA-D764-404F-BD42-345BDAF236D7}
AppName={#ProductName}
AppVersion={#AppVersion}
AppVerName={#ProductName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppPublisher}\{#ProductName}
DefaultGroupName={#ProductName}
DisableProgramGroupPage=yes
; The VST3 goes to a machine-wide location under Program Files, so this needs
; elevation regardless of what the user picks on the components page.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=jj-breeze-{#AppVersion}-windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#ProductName} {#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Types]
Name: "full"; Description: "Everything"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
; VST3 on Windows is a bundle *directory* (Contents\x86_64-win\...), not a
; single DLL, so the whole tree is recursed rather than copying one file.
; {commoncf64} is C:\Program Files\Common Files — the location the VST3 spec
; mandates, and the same one JUCE's own post-build copy targets.
Source: "{#ArtefactsDir}\VST3\{#ProductName}.vst3\*"; \
    DestDir: "{commoncf64}\VST3\{#ProductName}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

Source: "{#ArtefactsDir}\Standalone\{#ProductName}.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\{#ProductName}"; Filename: "{app}\{#ProductName}.exe"; Components: standalone
Name: "{autodesktop}\{#ProductName}"; Filename: "{app}\{#ProductName}.exe"; \
    Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[UninstallDelete]
; Inno removes the files it installed, but not the bundle directory itself
; once JUCE or a host has left anything behind inside it.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#ProductName}.vst3"
