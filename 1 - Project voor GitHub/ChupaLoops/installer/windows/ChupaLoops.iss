; Inno Setup script for Chupa Loops (compile with ISCC.exe)
; iscc /DAppVersion=1.1.23 /DSourceDir=..\..\dist\win /DOutputDir=..\..\dist ChupaLoops.iss

#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\win"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{6A7C2F4E-3B1D-4E8A-9C5F-2D7B1A6E0C93}
AppName=Chupa Loops
AppVersion={#AppVersion}
AppVerName=Chupa Loops {#AppVersion}
AppPublisher=Percep-tion
AppPublisherURL=https://thebeginningofhouse.nl
DefaultDirName={autopf}\Chupa Loops
DefaultGroupName=Chupa Loops
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=ChupaLoops-Windows-Setup
SetupIconFile=..\..\Resources\icon.ico
UninstallDisplayIcon={app}\Chupa Loops.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
MinVersion=10.0

[Types]
Name: "full"; Description: "Everything (recommended)"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Fender Studio Pro, Cubase, Ableton Live, FL Studio, Bitwig, Reaper...)"; Types: full custom; Flags: fixed
Name: "app"; Description: "Standalone app"; Types: full custom
Name: "manual"; Description: "Manual (PDF)"; Types: full custom

[Files]
Source: "{#SourceDir}\Chupa Loops.vst3\*"; DestDir: "{commoncf64}\VST3\Chupa Loops.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Chupa Loops.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#SourceDir}\Chupa Loops Manual.pdf"; DestDir: "{app}"; Components: manual; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Chupa Loops"; Filename: "{app}\Chupa Loops.exe"; Components: app
Name: "{group}\Chupa Loops Manual"; Filename: "{app}\Chupa Loops Manual.pdf"; Components: manual
Name: "{group}\Uninstall Chupa Loops"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Chupa Loops.vst3"

[Run]
Filename: "{app}\Chupa Loops.exe"; Description: "Start Chupa Loops"; Flags: nowait postinstall skipifsilent unchecked; Components: app

[Messages]
FinishedLabel=Chupa Loops is installed. Open your DAW and rescan plug-ins if needed: you'll find it under instruments (Percep-tion > Chupa Loops).
