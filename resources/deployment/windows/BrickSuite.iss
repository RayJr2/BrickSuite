; ====================================================================
; BrickSuite Windows Installer
; ====================================================================
;
; App/product metadata comes from BrickSuiteVersion.iss, which is generated
; by CMake from the same constants used by BrickSuite itself.
;
; StageDir and OutputDir are supplied by package_windows.bat when ISCC.exe
; is invoked.
; ====================================================================

#include "BrickSuiteVersion.iss"

#ifndef StageDir
    #error StageDir must be supplied by package_windows.bat.
#endif

#ifndef OutputDir
    #error OutputDir must be supplied by package_windows.bat.
#endif


[Setup]
AppId={{A5B79B27-27A4-4C22-95E8-2B45C603D85F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}

AppPublisher={#AppCompany}
AppPublisherURL=https://{#AppDomain}
AppSupportURL=https://{#AppDomain}
AppUpdatesURL=https://{#AppDomain}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

UninstallDisplayName={#AppName} {#AppVersion}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

PrivilegesRequired=admin

OutputDir={#OutputDir}
OutputBaseFilename=BrickSuite-{#AppVersion}-Windows-x64-Setup

Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes


[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs


[Icons]
Name: "{group}\BrickSuite"; Filename: "{app}\BrickSuite.exe"
Name: "{group}\Uninstall BrickSuite"; Filename: "{uninstallexe}"


[Run]
Filename: "{app}\BrickSuite.exe"; Description: "Launch BrickSuite"; Flags: nowait postinstall skipifsilent
