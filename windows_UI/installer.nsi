Unicode True
RequestExecutionLevel user
!include "MUI2.nsh"
!include "LogicLib.nsh"
!ifndef PACKAGE_DIR
  !error "PACKAGE_DIR is required"
!endif
!ifndef OUTPUT_FILE
  !error "OUTPUT_FILE is required"
!endif
!ifndef APP_VERSION
  !define APP_VERSION "0.1.6"
!endif
Name "SpektraLab Windows"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\SpektraLab"
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\SpektraLabQt.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Function CheckDirectory

  IfFileExists "$INSTDIR\.spektralab-install.ini" owned
  ClearErrors
  FindFirst $0 $1 "$INSTDIR\*"
  IfErrors nofiles
  loop:
    StrCmp $1 "" done
    StrCmp $1 "." next
    StrCmp $1 ".." next
    FindClose $0
    IfSilent +2
      MessageBox MB_ICONSTOP "Please choose an empty folder. Existing files will not be replaced."
    Abort
  next:
    FindNext $0 $1
    Goto loop
  done:
    FindClose $0
    Return
  nofiles:
    ClearErrors
    Return
  owned:
    ReadINIStr $0 "$INSTDIR\.spektralab-install.ini" "SpektraLab" "AppId"
    StrCmp $0 "felixniun.SpektraLab.Qt" 0 refuse
    ReadINIStr $0 "$INSTDIR\.spektralab-install.ini" "SpektraLab" "Path"
    StrCmp $0 "$INSTDIR" 0 refuse
    Return
  refuse:
    Abort
FunctionEnd
Function .onVerifyInstDir
  Call CheckDirectory
FunctionEnd

Section "SpektraLab"
  SetShellVarContext current
  Call CheckDirectory
  SetOutPath "$INSTDIR"
  File /r "${PACKAGE_DIR}\*"
  WriteINIStr "$INSTDIR\.spektralab-install.ini" "SpektraLab" "AppId" "felixniun.SpektraLab.Qt"
  WriteINIStr "$INSTDIR\.spektralab-install.ini" "SpektraLab" "Path" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\SpektraLab Windows"
  CreateShortcut "$SMPROGRAMS\SpektraLab Windows\SpektraLab.lnk" "$INSTDIR\SpektraLabQt.exe"
  CreateShortcut "$SMPROGRAMS\SpektraLab Windows\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "DisplayName" "SpektraLab Windows (Qt)"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "Publisher" "felixniun; upstream SpektraLab contributors"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows" "URLInfoAbout" "https://github.com/felixniun/spektralab"
SectionEnd
Function un.onInit

  ReadINIStr $0 "$INSTDIR\.spektralab-install.ini" "SpektraLab" "AppId"
  StrCmp $0 "felixniun.SpektraLab.Qt" 0 refuse
  ReadINIStr $0 "$INSTDIR\.spektralab-install.ini" "SpektraLab" "Path"
  StrCmp $0 "$INSTDIR" 0 refuse
  Return
  refuse:
    Abort
FunctionEnd
Section "Uninstall"
  SetShellVarContext current
  ; onInit verifies the absolute install target and our installer-owned marker.
  ; Photo edits and the lighttable live outside this folder and are preserved.
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\SpektraLab Windows\SpektraLab.lnk"
  Delete "$SMPROGRAMS\SpektraLab Windows\Uninstall.lnk"
  RMDir "$SMPROGRAMS\SpektraLab Windows"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLab-Qt-Windows"
SectionEnd
