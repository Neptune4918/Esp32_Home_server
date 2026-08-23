const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('panelAPI', {
  expand: () => ipcRenderer.send('panel:expand'),
  collapse: () => ipcRenderer.send('panel:collapse'),
  quit: () => ipcRenderer.send('panel:quit'),
  getConfig: () => ipcRenderer.invoke('config:get'),
  saveConfig: (config) => ipcRenderer.invoke('config:save', config),
  onState: (callback) => ipcRenderer.on('panel:state', (_event, state) => callback(state)),
  onDataUpdate: (callback) => ipcRenderer.on('data:update', (_event, data) => callback(data)),
  onDataError: (callback) => ipcRenderer.on('data:error', (_event, message) => callback(message)),
});
