; NSIS installer for 3D Car Game + Map Editor.
; Build: "C:\Program Files (x86)\NSIS\makensis.exe" installer\installer.nsi
; (run from the repo root so the relative paths below resolve)

!define APP_NAME "3D Car Game"
!define APP_VERSION "1.7.0"
!define APP_PUBLISHER "syedammarali294-hub"
!define INSTALL_DIR_NAME "3D Car Game"

Name "${APP_NAME}"
OutFile "..\release\3DCarGame-Setup-${APP_VERSION}.exe"
InstallDir "$PROGRAMFILES64\${INSTALL_DIR_NAME}"
InstallDirRegKey HKCU "Software\${INSTALL_DIR_NAME}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icons\game.ico"
!define MUI_UNICON "..\assets\icons\game.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\CarGame3D.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch 3D Car Game now"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "3D Car Game (required)" SecGame
    SectionIn RO
    SetOutPath "$INSTDIR"
    File "..\build\Release\CarGame3D.exe"
    SetOutPath "$INSTDIR\shaders"
    File /r "..\build\Release\shaders\*.*"
    SetOutPath "$INSTDIR\assets"
    File /r "..\build\Release\assets\*.*"

    SetOutPath "$INSTDIR"
    CreateDirectory "$SMPROGRAMS\${INSTALL_DIR_NAME}"
    CreateShortCut "$SMPROGRAMS\${INSTALL_DIR_NAME}\3D Car Game.lnk" "$INSTDIR\CarGame3D.exe"
    CreateShortCut "$DESKTOP\3D Car Game.lnk" "$INSTDIR\CarGame3D.exe"

    WriteRegStr HKCU "Software\${INSTALL_DIR_NAME}" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}" "DisplayIcon" "$INSTDIR\CarGame3D.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}" "Publisher" "${APP_PUBLISHER}"
SectionEnd

Section "Map Editor" SecEditor
    SetOutPath "$INSTDIR"
    File "..\build\Release\MapEditor.exe"
    CreateShortCut "$SMPROGRAMS\${INSTALL_DIR_NAME}\Map Editor.lnk" "$INSTDIR\MapEditor.exe"
    CreateShortCut "$DESKTOP\3D Car Game - Map Editor.lnk" "$INSTDIR\MapEditor.exe"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\CarGame3D.exe"
    Delete "$INSTDIR\MapEditor.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir /r "$INSTDIR\shaders"
    RMDir /r "$INSTDIR\assets"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\${INSTALL_DIR_NAME}\3D Car Game.lnk"
    Delete "$SMPROGRAMS\${INSTALL_DIR_NAME}\Map Editor.lnk"
    RMDir "$SMPROGRAMS\${INSTALL_DIR_NAME}"
    Delete "$DESKTOP\3D Car Game.lnk"
    Delete "$DESKTOP\3D Car Game - Map Editor.lnk"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALL_DIR_NAME}"
    DeleteRegKey HKCU "Software\${INSTALL_DIR_NAME}"
SectionEnd
