import { createRequire } from "node:module";
import { join } from "node:path";

import { app, BrowserWindow, ipcMain } from "electron";
import { createNodeClient, createStockLinkRuntime } from "tdx-ths-linker";

const require = createRequire(import.meta.url);
const nativePath = process.env.TDX_THS_LINKER_NODE_PATH
  ?? join(app.getAppPath(), "native", "win-x64", "tdx-ths-linker.node");
const native = require(nativePath);
const client = createNodeClient(native);
let window;

const runtime = createStockLinkRuntime({
  client,
  mode: "off",
  onState: (state) => window?.webContents.send("linker:state", state),
  onCommand: (command) => window?.webContents.send("linker:command", command),
  onError: (error) => window?.webContents.send("linker:error", String(error)),
});

const linkModes = new Set(["off", "read-only", "ths-to-tdx", "tdx-to-ths"]);

ipcMain.handle("linker:initialize", (_event, accepted) => client.initialize(accepted === true));
ipcMain.handle("linker:get-state", () => client.getState());
ipcMain.handle("linker:set-code", (_event, target, code) => {
  if (target !== "tdx" && target !== "ths") throw new Error("target must be tdx or ths");
  if (typeof code !== "string" || !/^\d{6}$/.test(code)) throw new Error("code must be six digits");
  return client.setCode(target, code);
});
ipcMain.handle("linker:set-mode", (_event, mode) => {
  if (!linkModes.has(mode)) throw new Error("unsupported link mode");
  runtime.setMode(mode);
  return mode;
});
ipcMain.handle("linker:start", () => {
  runtime.start();
  return true;
});
ipcMain.handle("linker:stop", () => {
  runtime.stop();
  return true;
});

app.whenReady().then(() => {
  window = new BrowserWindow({
    width: 720,
    height: 640,
    webPreferences: {
      preload: join(app.getAppPath(), "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  void window.loadFile("index.html");
});

app.on("before-quit", () => runtime.stop());
app.on("window-all-closed", () => app.quit());
