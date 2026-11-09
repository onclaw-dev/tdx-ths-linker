Unicode true
!include "LogicLib.nsh"
!include "WinMessages.nsh"
RequestExecutionLevel user
Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_EXE}"
InstallDir "$LOCALAPPDATA\TdxThsLinker"
SetCompressor /SOLID lzma

Page license "${LICENSE_FILE}"
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "${PRODUCT_NAME}" SEC_AGENT
  FindWindow $0 "TdxThsLinkerAgentWindow"
  ${If} $0 != 0
    SendMessage $0 ${WM_CLOSE} 0 0 /TIMEOUT=5000
    Sleep 500
  ${EndIf}
  SetOutPath "$INSTDIR"
  File /oname=tdx-ths-linker-agent.exe "${AGENT_EXE}"
  ExecWait '"$INSTDIR\tdx-ths-linker-agent.exe" --register' $0
  ${If} $0 != 0
    Abort "Custom protocol registration failed (exit code $0)"
  ${EndIf}
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TdxThsLinker" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TdxThsLinker" "ReleaseChannel" "${RELEASE_CHANNEL}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TdxThsLinker" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TdxThsLinker" "InstallLocation" "$INSTDIR"
SectionEnd

Section "Uninstall"
  FindWindow $1 "TdxThsLinkerAgentWindow"
  ${If} $1 != 0
    SendMessage $1 ${WM_CLOSE} 0 0 /TIMEOUT=5000
    Sleep 500
  ${EndIf}
  ExecWait '"$INSTDIR\tdx-ths-linker-agent.exe" --verify-registration' $0
  ${If} $0 == 0
    ExecWait '"$INSTDIR\tdx-ths-linker-agent.exe" --unregister'
  ${EndIf}
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TdxThsLinker"
  Delete "$INSTDIR\tdx-ths-linker-agent.exe"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
