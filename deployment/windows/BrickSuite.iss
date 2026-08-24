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
AppCopyright=Copyright (C) {#AppCopyrightYear} {#AppCompany}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UsePreviousAppDir=yes

UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\bricksuite.ico

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin

OutputDir={#OutputDir}
OutputBaseFilename=BrickSuite-{#AppVersion}-Windows-x64-Setup

Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

LicenseFile={#StageDir}\LICENSE
SetupIconFile={#StageDir}\bricksuite.ico

CloseApplications=yes
RestartApplications=no

VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppCompany}
VersionInfoDescription={#AppName} Windows Installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (C) {#AppCopyrightYear} {#AppCompany}


[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked


[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs


[Icons]
Name: "{group}\BrickSuite"; Filename: "{app}\BrickSuite.exe"; IconFilename: "{app}\bricksuite.ico"
Name: "{group}\Uninstall BrickSuite"; Filename: "{uninstallexe}"
Name: "{autodesktop}\BrickSuite"; Filename: "{app}\BrickSuite.exe"; IconFilename: "{app}\bricksuite.ico"; Tasks: desktopicon


[Run]
Filename: "{app}\BrickSuite.exe"; Description: "Launch BrickSuite"; Flags: nowait postinstall skipifsilent
