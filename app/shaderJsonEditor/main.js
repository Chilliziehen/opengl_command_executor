const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');

let mainWindow = null;
let currentFilePath = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    minWidth: 800,
    minHeight: 600,
    title: 'Shader JSON Editor',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  app.quit();
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

// --- IPC Handlers ---

// Open file dialog + read JSON
ipcMain.handle('file:open', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Open shaders.json',
    filters: [
      { name: 'JSON Files', extensions: ['json'] },
      { name: 'All Files', extensions: ['*'] },
    ],
    properties: ['openFile'],
  });

  if (result.canceled || result.filePaths.length === 0) {
    return { success: false, canceled: true };
  }

  const filePath = result.filePaths[0];
  try {
    const raw = fs.readFileSync(filePath, 'utf-8');
    const data = JSON.parse(raw);
    if (!Array.isArray(data)) {
      return { success: false, error: 'shaders.json must contain a JSON array' };
    }
    currentFilePath = filePath;
    return { success: true, filePath, shaders: data };
  } catch (err) {
    if (err instanceof SyntaxError) {
      return { success: false, error: `Failed to parse JSON: ${err.message}` };
    }
    return { success: false, error: `Failed to read file: ${err.message}` };
  }
});

// Save JSON to current file
ipcMain.handle('file:save', async (_event, shaders) => {
  if (!currentFilePath) {
    return await handleSaveAs(shaders);
  }
  return writeShadersFile(currentFilePath, shaders);
});

// Save As dialog + write JSON
ipcMain.handle('file:saveAs', async (_event, shaders) => {
  return await handleSaveAs(shaders);
});

async function handleSaveAs(shaders) {
  const result = await dialog.showSaveDialog(mainWindow, {
    title: 'Save shaders.json',
    filters: [
      { name: 'JSON Files', extensions: ['json'] },
      { name: 'All Files', extensions: ['*'] },
    ],
    defaultPath: 'shaders.json',
  });

  if (result.canceled) {
    return { success: false, canceled: true };
  }

  const filePath = result.filePath;
  return writeShadersFile(filePath, shaders);
}

function writeShadersFile(filePath, shaders) {
  // Validate shader structure
  for (let i = 0; i < shaders.length; i++) {
    const s = shaders[i];
    if (typeof s.id !== 'number') {
      return { success: false, error: `Invalid shader at index ${i}: missing or invalid 'id' field` };
    }
    if (typeof s.label !== 'string') {
      return { success: false, error: `Invalid shader at index ${i}: missing or invalid 'label' field` };
    }
    if (typeof s.shaderType !== 'number') {
      return { success: false, error: `Invalid shader at index ${i}: missing or invalid 'shaderType' field` };
    }
    if (typeof s.source !== 'string') {
      return { success: false, error: `Invalid shader at index ${i}: missing or invalid 'source' field` };
    }
  }

  try {
    const json = JSON.stringify(shaders, null, 2);
    fs.writeFileSync(filePath, json, 'utf-8');
    currentFilePath = filePath;
    return { success: true, filePath };
  } catch (err) {
    return { success: false, error: `Failed to write file: ${err.message}` };
  }
}
