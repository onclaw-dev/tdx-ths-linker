Unicode true
!include "LogicLib.nsh"
!include "WinMessages.nsh"
RequestExecutionLevel user
Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_EXE}"
InstallDir "$LOCALAPPDATA\OnclawTdxThs"
SetCompressor /SOLID lzma
Icon "${ICON_FILE}"
UninstallIcon "${ICON_FILE}"

Page license
LicenseData "${LICENSE_FILE}"
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Var TransitionHandle

Function AcquireTransitionLock
  StrCpy $0 0
  transition_retry:
    System::Call 'kernel32::CreateFileW(w "$LOCALAPPDATA\OnclawTdxThs.transition.lock", i 0x40000000, i 0, p 0, i 1, i 0x80, p 0) p .r9'
    StrCmp $9 -1 transition_busy transition_acquired
  transition_busy:
    Sleep 200
    IntOp $0 $0 + 1
    IntCmp $0 50 transition_timeout transition_retry transition_timeout
  transition_timeout:
    Abort "Timed out waiting for another Agent transition"
  transition_acquired:
    StrCpy $TransitionHandle $9
FunctionEnd

Function ReleaseTransitionLock
  StrCmp $TransitionHandle "" transition_release_done
  System::Call 'kernel32::CloseHandle(p $TransitionHandle)'
  StrCpy $TransitionHandle ""
  Delete "$LOCALAPPDATA\OnclawTdxThs.transition.lock"
  transition_release_done:
FunctionEnd

Function .onInstFailed
  Call ReleaseTransitionLock
FunctionEnd

Function un.AcquireTransitionLock
  StrCpy $0 0
  un_transition_retry:
    System::Call 'kernel32::CreateFileW(w "$LOCALAPPDATA\OnclawTdxThs.transition.lock", i 0x40000000, i 0, p 0, i 1, i 0x80, p 0) p .r9'
    StrCmp $9 -1 un_transition_busy un_transition_acquired
  un_transition_busy:
    Sleep 200
    IntOp $0 $0 + 1
    IntCmp $0 50 un_transition_timeout un_transition_retry un_transition_timeout
  un_transition_timeout:
    Abort "Timed out waiting for another Agent transition"
  un_transition_acquired:
    StrCpy $TransitionHandle $9
FunctionEnd

Function un.ReleaseTransitionLock
  StrCmp $TransitionHandle "" un_transition_release_done
  System::Call 'kernel32::CloseHandle(p $TransitionHandle)'
  StrCpy $TransitionHandle ""
  Delete "$LOCALAPPDATA\OnclawTdxThs.transition.lock"
  un_transition_release_done:
FunctionEnd

Function un.onUninstFailed
  Call un.ReleaseTransitionLock
FunctionEnd

Section "${PRODUCT_NAME}" SEC_AGENT
  Call AcquireTransitionLock
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=onclaw-tdx-ths-agent.exe "${AGENT_EXE}"
  ExecWait '"$PLUGINSDIR\onclaw-tdx-ths-agent.exe" --shutdown' $0
  ${If} $0 != 0
    Abort "Unable to stop the current Agent (exit code $0)"
  ${EndIf}
  SetOutPath "$INSTDIR"
  ClearErrors
  CopyFiles /SILENT "$PLUGINSDIR\onclaw-tdx-ths-agent.exe" "$INSTDIR\onclaw-tdx-ths-agent.exe"
  IfErrors 0 +2
    Abort "Unable to install the Agent executable"
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --register' $0
  ${If} $0 != 0
    Abort "Custom protocol registration failed (exit code $0)"
  ${EndIf}
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs" "InstallLocation" "$INSTDIR"
  Exec '"$INSTDIR\onclaw-tdx-ths-agent.exe"'
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --verify-health' $0
  ${If} $0 != 0
    Abort "Installed Agent did not become healthy (exit code $0)"
  ${EndIf}
  Call ReleaseTransitionLock
SectionEnd

Section "Uninstall"
  Call un.AcquireTransitionLock
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --shutdown' $1
  ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --verify-registration' $0
  ${If} $0 == 0
    ExecWait '"$INSTDIR\onclaw-tdx-ths-agent.exe" --unregister'
  ${EndIf}
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs"
  Delete "$INSTDIR\onclaw-tdx-ths-agent.exe"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Call un.ReleaseTransitionLock
SectionEnd
