; Project Foxy - installer (Inno Setup 6).
;
; Build it with tools\Build-Release.ps1 (which installs the program into dist\ProjectFoxy first), or by hand:
;     ISCC.exe /DAppVersion=0.1.0 installer\ProjectFoxy.iss
;
; What it does:
;   - installs the folder that `cmake --install` produces (the program, Qt, the libraries and the licences);
;   - per user by default (no administrator rights), or for all users if the person chooses so;
;   - optionally registers Project Foxy as a program that opens images: it shows up in "Open with" and in
;     Windows' Settings > Default apps, and once it is the default for a type, Explorer shows its icon on those
;     files. Windows does not let a program make itself the default, so the last step is the person's.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppName "Project Foxy"
#define AppExe "ProjectFoxy.exe"

[Setup]
AppId={{7B3E2F60-5C1A-4D8E-9A41-F0A9C2D6B8E1}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Vulito
AppCopyright=Copyright (C) 2026 Vulito. GPL-3.0-or-later.
VersionInfoVersion={#AppVersion}.0
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
LicenseFile=..\LICENSE
SetupIconFile=..\resources\icons\icon.ico
OutputDir=..\dist\installer
OutputBaseFilename=ProjectFoxy-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assoc"; Description: "Permitir abrir imágenes con Project Foxy (aparece en «Abrir con» y en Aplicaciones predeterminadas)"; GroupDescription: "Imágenes:"; Languages: spanish
Name: "assoc"; Description: "Let Project Foxy open images (shows up in ""Open with"" and in Default apps)"; GroupDescription: "Images:"; Languages: english

[Files]
; the folder produced by `cmake --install` (see tools\Build-Release.ps1)
Source: "..\dist\ProjectFoxy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; the type "an image of Project Foxy": its icon and how to open it
Root: HKA; Subkey: "Software\Classes\ProjectFoxy.Image"; ValueType: string; ValueName: ""; ValueData: "Imagen (Project Foxy)"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\ProjectFoxy.Image\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\ProjectFoxy.Image\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc
; "Open with" for each type the program reads
Root: HKA; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.gif\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.ico\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.heic\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.heif\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.avif\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.cr2\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.cr3\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.nef\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.arw\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.dng\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.orf\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.rw2\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.raf\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.srw\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.pef\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.raw\OpenWithProgids"; ValueType: string; ValueName: "ProjectFoxy.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
; the program as an option in Settings > Default apps
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#AppName}"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Visor y editor de imágenes"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webp"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".gif"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ico"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heic"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heif"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avif"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr2"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr3"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".nef"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".arw"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dng"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".orf"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rw2"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raf"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".srw"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pef"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\ProjectFoxy\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raw"; ValueData: "ProjectFoxy.Image"; Tasks: assoc
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "{#AppName}"; ValueData: "Software\ProjectFoxy\Capabilities"; Flags: uninsdeletevalue; Tasks: assoc

[Run]
Filename: "ms-settings:defaultapps"; Description: "Elegir Project Foxy como programa predeterminado para las imágenes"; Flags: postinstall shellexec skipifsilent unchecked; Tasks: assoc; Languages: spanish
Filename: "ms-settings:defaultapps"; Description: "Choose Project Foxy as the default app for images"; Flags: postinstall shellexec skipifsilent unchecked; Tasks: assoc; Languages: english
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
