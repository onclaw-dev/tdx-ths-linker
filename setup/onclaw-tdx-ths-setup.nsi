Unicode true
!include "LogicLib.nsh"
!include "WinMessages.nsh"
RequestExecutionLevel user
Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_EXE}"
InstallDir "$LOCALAPPDATA\OnclawTdxThs"
SetCompressor /SOLID lzma

Page license "${LICENSE_FILE}"
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "${PRODUCT_NAME}" SEC_AGENT
  FindWindow $0 "OnclawTdxThsAgentWindow"
  ${If} $0 != 0
    SendMessage $0 ${WM_CLOSE} 0 0 /TIMEOUT=5000
    Sleep 500
  ${EndIf}
  SetOutPath "$INSTDIR"
  File /oname=onclaw-tdx-ths-agent.exe "${AGENT_EXE}"
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --register' $0
  ${If} $0 != 0
    Abort "Custom protocol registration failed (exit code $0)"
  ${EndIf}
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "InstallLocation" "$INSTDIR"
SectionEnd

Section "Uninstall"
  FindWindow $1 "OnclawTdxThsAgentWindow"
  ${If} $1 != 0
    SendMessage $1 ${WM_CLOSE} 0 0 /TIMEOUT=5000
    Sleep 500
  ${EndIf}
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --verify-registration' $0
  ${If} $0 == 0
    ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --unregister'
  ${EndIf}
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs"
  Delete "$INSTDIR\onclaw-tdx-ths-agent.exe"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
