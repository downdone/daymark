Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
Name "Daymark ${APP_VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Daymark"
RequestExecutionLevel user
SetCompressor /SOLID zlib
!uninstfinalize '"${SIGN_TOOL}" sign /fd SHA256 /s My /sha1 ${CERT_THUMBPRINT} /d "Daymark Uninstaller" "%1"' = 0
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "Daymark"
VIAddVersionKey "FileDescription" "Daymark 安装程序"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "FileVersion" "${APP_VERSION}.0"
VIAddVersionKey "LegalCopyright" "Daymark Contributors"
!define MUI_ICON "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "安装 Daymark"
!define MUI_WELCOMEPAGE_TEXT "任务与时间，清晰有序。$\r$\n$\r$\n安装到当前用户，无需管理员权限。更新将继续使用内置签名校验。$\r$\n$\r$\n重新安装前，请先从托盘退出 Daymark。个人任务和账号资料会保留。"
!define MUI_FINISHPAGE_RUN "$INSTDIR\daymark.exe"
!define MUI_FINISHPAGE_RUN_TEXT "运行 Daymark"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!define MUI_UNCONFIRMPAGE_TEXT_TOP "将删除 Daymark 程序与快捷方式。个人任务、账号资料以及您添加或修改的文件会保留。请先从托盘退出 Daymark。"
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "SimpChinese"
Var NoIntegration
Var ExistingRoot
Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Daymark 需要 64 位 Windows。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  SetShellVarContext current
  StrCpy $NoIntegration "0"
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/NOINTEGRATION" $1
  ${IfNot} ${Errors}
    StrCpy $NoIntegration "1"
  ${EndIf}
  ReadRegStr $ExistingRoot HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "InstallLocation"
  ${If} $ExistingRoot != ""
    StrCpy $INSTDIR $ExistingRoot
  ${EndIf}
FunctionEnd
Section "Daymark（必需）" SecCore
  SectionIn RO
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\bundle"
  File /r "${BUNDLE_DIR}\*"
  SetOutPath "$PLUGINSDIR"
  File /oname=setup-helper.exe "${HELPER_FILE}"
  DetailPrint "校验签名并安装程序…"
  nsExec::ExecToStack '"$PLUGINSDIR\setup-helper.exe" install --source "$PLUGINSDIR\bundle" --root "$INSTDIR"'
  Pop $0
  Pop $1
  ${If} $0 != "0"
    DetailPrint "$1"
    IfSilent +2
    MessageBox MB_ICONSTOP|MB_OK "安装未完成。请先从托盘退出 Daymark，检查安装目录的空间和权限后重试。$\r$\n$\r$\n详情：$1"
    SetErrorLevel 1
    Abort
  ${EndIf}
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  nsExec::ExecToStack '"$PLUGINSDIR\setup-helper.exe" seal-uninstaller --root "$INSTDIR"'
  Pop $0
  Pop $1
  ${If} $0 != "0"
    DetailPrint "$1"
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${If} $NoIntegration != "1"
    CreateDirectory "$SMPROGRAMS\Daymark"
    CreateShortcut "$SMPROGRAMS\Daymark\Daymark.lnk" "$INSTDIR\daymark.exe"
    CreateShortcut "$SMPROGRAMS\Daymark\卸载 Daymark.lnk" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "DisplayName" "Daymark"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "Publisher" "Daymark Contributors"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "DisplayIcon" "$INSTDIR\daymark.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "NoRepair" 1
  ${EndIf}
SectionEnd
Section /o "桌面快捷方式" SecDesktop
  ${If} $NoIntegration != "1"
    CreateShortcut "$DESKTOP\Daymark.lnk" "$INSTDIR\daymark.exe"
  ${EndIf}
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  CopyFiles /SILENT "$INSTDIR\daymark-setup-helper.exe" "$PLUGINSDIR\setup-helper.exe"
  nsExec::ExecToStack '"$PLUGINSDIR\setup-helper.exe" uninstall --root "$INSTDIR"'
  Pop $0
  Pop $1
  ${If} $0 != "0"
    DetailPrint "$1"
    IfSilent +2
    MessageBox MB_ICONSTOP|MB_OK "卸载未完成。请先从托盘退出 Daymark 后重试。个人数据已保留。$\r$\n$1"
    SetErrorLevel 1
    Abort
  ${EndIf}
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark" "InstallLocation"
  ${If} $0 == $INSTDIR
    Delete "$DESKTOP\Daymark.lnk"
    Delete "$SMPROGRAMS\Daymark\Daymark.lnk"
    Delete "$SMPROGRAMS\Daymark\卸载 Daymark.lnk"
    RMDir "$SMPROGRAMS\Daymark"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark"
  ${EndIf}
  ; No recursive deletion: unknown files, modified programs and user data survive.
  RMDir "$INSTDIR"
SectionEnd
