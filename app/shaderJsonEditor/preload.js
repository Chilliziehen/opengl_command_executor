const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('shaderAPI', {
  openFile: () => ipcRenderer.invoke('file:open'),
  saveFile: (shaders) => ipcRenderer.invoke('file:save', shaders),
  saveFileAs: (shaders) => ipcRenderer.invoke('file:saveAs', shaders),
});
