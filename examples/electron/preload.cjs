const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("tdxThsLinker", {
  initialize: (licenseAccepted) => ipcRenderer.invoke("linker:initialize", licenseAccepted),
  getState: () => ipcRenderer.invoke("linker:get-state"),
  setCode: (target, code) => ipcRenderer.invoke("linker:set-code", target, code),
  setMode: (mode) => ipcRenderer.invoke("linker:set-mode", mode),
  start: () => ipcRenderer.invoke("linker:start"),
  stop: () => ipcRenderer.invoke("linker:stop"),
  onState: (listener) => ipcRenderer.on("linker:state", (_event, state) => listener(state)),
  onCommand: (listener) => ipcRenderer.on("linker:command", (_event, command) => listener(command)),
  onError: (listener) => ipcRenderer.on("linker:error", (_event, error) => listener(error)),
});
