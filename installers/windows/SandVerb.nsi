; Установщик SandVerb для Windows (NSIS 3.x).
; Копирует SandVerb.vst3 в C:\Program Files\Common Files\VST3 (папку можно сменить на странице выбора).
;
; Сборка:
;   makensis /DSRC=<путь к собранной папке SandVerb.vst3> [/DVERSION=0.1.0] [/DOUT=SandVerb-Setup.exe] SandVerb.nsi

Unicode true
ManifestDPIAware true
RequestExecutionLevel admin

!ifndef SRC
  !error "Укажите путь к собранной папке SandVerb.vst3: makensis /DSRC=<путь> SandVerb.nsi"
!endif
!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef OUT
  !define OUT "SandVerb-Setup.exe"
!endif

!define NAME      "SandVerb"
!define BUNDLE    "SandVerb.vst3"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
!define APPKEY    "Software\${NAME}"

!include "MUI2.nsh"
!include "LogicLib.nsh"

Name "${NAME} ${VERSION}"
OutFile "${OUT}"
InstallDir "$COMMONFILES64\VST3"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName"     "${NAME}"
VIAddVersionKey "FileDescription" "${NAME} VST3 installer"
VIAddVersionKey "FileVersion"     "${VERSION}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"

Function .onInit
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Install"
  ; прошлую версию убираем целиком, чтобы не остались лишние файлы
  RMDir /r "$INSTDIR\${BUNDLE}"

  SetOutPath "$INSTDIR\${BUNDLE}"
  File /r "${SRC}\*.*"

  ; деинсталлятор лежит отдельно от плагина: хосты сканируют папку VST3
  CreateDirectory "$PROGRAMFILES64\${NAME}"
  WriteUninstaller "$PROGRAMFILES64\${NAME}\Uninstall.exe"

  WriteRegStr HKLM "${APPKEY}" "VST3Dir" "$INSTDIR"
  WriteRegStr   HKLM "${UNINSTKEY}" "DisplayName"     "${NAME} ${VERSION} (VST3)"
  WriteRegStr   HKLM "${UNINSTKEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINSTKEY}" "UninstallString" '"$PROGRAMFILES64\${NAME}\Uninstall.exe"'
  WriteRegStr   HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR\${BUNDLE}"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ReadRegStr $0 HKLM "${APPKEY}" "VST3Dir"
  ${If} $0 == ""
    StrCpy $0 "$COMMONFILES64\VST3"
  ${EndIf}
  RMDir /r "$0\${BUNDLE}"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "${APPKEY}"
SectionEnd
