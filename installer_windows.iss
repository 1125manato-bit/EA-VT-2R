; ==============================================================================
; EA VT-2R - EMU AUDIO
; Inno Setup Script (Windows Installer)
; ==============================================================================

#define MyAppName "EA VT-2R"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "EMU AUDIO"
#define MyAppURL "https://emuaudio.com"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
AppId={{D7E3E1D5-2B3C-5D4E-9F5G-6H7I8J9K0L1M}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}

AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commoncf64}\VST3
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=installer
OutputBaseFilename=EA_VT-2R_Installer_Windows
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
; VST3 Plugin
Source: "build\EA_VT_2R_artefacts\Release\VST3\EA VT-2R.vst3\*"; DestDir: "{commoncf64}\VST3\EA VT-2R.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
BeveledLabel=EMU AUDIO - EA VT-2R Installer

