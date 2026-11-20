Unicode true
ManifestDPIAware true
RequestExecutionLevel user
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

!ifndef BUILD_DIR
    !error "Pass BUILD_DIR with the desktop executable and assets."
!endif
!ifndef PROJECT_DIR
    !error "Pass PROJECT_DIR containing LICENSE."
!endif
!ifndef OUTPUT_FILE
    !error "Pass OUTPUT_FILE for the installer executable."
!endif

!define PRODUCT "benno111 engene"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\benno111-engene"

Name "${PRODUCT}"
Caption "${PRODUCT} Setup"
BrandingText "${PRODUCT}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\benno111-engene"
ShowInstDetails show

Var DesktopCheckbox
Var StartMenuCheckbox
Var DesktopShortcut
Var StartMenuShortcut
Var LaunchCheckbox

Page custom ShowSetup SaveSetup
!insertmacro MUI_PAGE_LICENSE "${PROJECT_DIR}\LICENSE"
!insertmacro MUI_PAGE_INSTFILES
Page custom ShowComplete LeaveComplete
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
    SetShellVarContext current
    StrCpy $INSTDIR "$LOCALAPPDATA\benno111-engene"
    StrCpy $StartMenuShortcut ${BST_CHECKED}
    StrCpy $DesktopShortcut ${BST_UNCHECKED}
    IfFileExists "$INSTDIR\.benno111-install" ready
    IfFileExists "$INSTDIR\." occupied ready
    occupied:
        MessageBox MB_ICONSTOP "Setup found an existing folder it does not manage:$\r$\n$INSTDIR" /SD IDOK
        SetErrorLevel 2
        Quit
    ready:
        ${If} ${FileExists} "$INSTDIR\.benno111-install"
            ${IfNot} ${FileExists} "$SMPROGRAMS\${PRODUCT}.lnk"
                StrCpy $StartMenuShortcut ${BST_UNCHECKED}
            ${EndIf}
            ${If} ${FileExists} "$DESKTOP\${PRODUCT}.lnk"
                StrCpy $DesktopShortcut ${BST_CHECKED}
            ${EndIf}
        ${EndIf}
FunctionEnd

Function ShowSetup
    !insertmacro MUI_HEADER_TEXT "Install ${PRODUCT}" "Choose where to put shortcuts for your account."
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}
    ${NSD_CreateLabel} 0 0 100% 24u "The engine demo, game assets, and shaders will be installed for your Windows account."
    Pop $0
    ${NSD_CreateLabel} 0 32u 100% 32u "$INSTDIR"
    Pop $0
    ${NSD_CreateCheckbox} 0 70u 100% 14u "Add a Start menu shortcut"
    Pop $StartMenuCheckbox
    ${NSD_SetState} $StartMenuCheckbox $StartMenuShortcut
    ${NSD_CreateCheckbox} 0 90u 100% 14u "Add a desktop shortcut"
    Pop $DesktopCheckbox
    ${NSD_SetState} $DesktopCheckbox $DesktopShortcut
    nsDialogs::Show
FunctionEnd

Function SaveSetup
    ${NSD_GetState} $StartMenuCheckbox $StartMenuShortcut
    ${NSD_GetState} $DesktopCheckbox $DesktopShortcut
FunctionEnd

Section "Install"
    ClearErrors
    SetOutPath "$INSTDIR"
    FileOpen $0 "$INSTDIR\.benno111-install" w
    FileWrite $0 "benno111-engene"
    FileClose $0
    File "${BUILD_DIR}\benno111-engene.exe"
    File "${PROJECT_DIR}\LICENSE"
    SetOutPath "$INSTDIR\assets"
    File /r "${BUILD_DIR}\assets\*"
    SetOutPath "$INSTDIR\shaders"
    File /r "${BUILD_DIR}\shaders\*"
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    IfErrors installFailed

    ${If} $StartMenuShortcut == ${BST_CHECKED}
        CreateShortCut "$SMPROGRAMS\${PRODUCT}.lnk" "$INSTDIR\benno111-engene.exe"
    ${Else}
        Delete "$SMPROGRAMS\${PRODUCT}.lnk"
    ${EndIf}
    ${If} $DesktopShortcut == ${BST_CHECKED}
        CreateShortCut "$DESKTOP\${PRODUCT}.lnk" "$INSTDIR\benno111-engene.exe"
    ${Else}
        Delete "$DESKTOP\${PRODUCT}.lnk"
    ${EndIf}

    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\benno111-engene.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\uninstall.exe$\"'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    Goto installed

    installFailed:
        MessageBox MB_ICONSTOP "Setup could not write the game files. Close the game and run setup again." /SD IDOK
        SetErrorLevel 1
        Abort
    installed:
SectionEnd

Function ShowComplete
    !insertmacro MUI_HEADER_TEXT "Setup complete" "${PRODUCT} is ready to play."
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}
    ${NSD_CreateLabel} 0 0 100% 26u "You can uninstall the game from Windows Settings under Installed apps."
    Pop $0
    ${NSD_CreateCheckbox} 0 38u 100% 14u "Launch ${PRODUCT}"
    Pop $LaunchCheckbox
    nsDialogs::Show
FunctionEnd

Function LeaveComplete
    ${NSD_GetState} $LaunchCheckbox $0
    ${If} $0 == ${BST_CHECKED}
        ExecShell "open" "$INSTDIR\benno111-engene.exe"
    ${EndIf}
FunctionEnd

Function un.onInit
    SetShellVarContext current
    IfFileExists "$INSTDIR\.benno111-install" +3
    MessageBox MB_ICONSTOP "This folder does not contain a managed ${PRODUCT} installation." /SD IDOK
    Abort
FunctionEnd

Section "Uninstall"
    ClearErrors
    IfFileExists "$INSTDIR\benno111-engene.exe" 0 removeShortcuts
    Delete "$INSTDIR\benno111-engene.exe"
    IfErrors removeFailed
    removeShortcuts:
    Delete "$SMPROGRAMS\${PRODUCT}.lnk"
    Delete "$DESKTOP\${PRODUCT}.lnk"
    Delete "$INSTDIR\LICENSE"
    RMDir /r "$INSTDIR\assets"
    RMDir /r "$INSTDIR\shaders"
    Delete "$INSTDIR\.benno111-install"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    Goto removed
    removeFailed:
        MessageBox MB_ICONSTOP "Close ${PRODUCT} and run the uninstaller again." /SD IDOK
        SetErrorLevel 1
        Abort
    removed:
SectionEnd
