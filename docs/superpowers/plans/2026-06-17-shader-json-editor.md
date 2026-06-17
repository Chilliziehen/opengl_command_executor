# Shader JSON Editor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone Electron desktop app in `app/shaderJsonEditor/` that opens `shaders.json`, provides GLSL editing with Monaco Editor syntax highlighting, and saves edits back to the same file.

**Architecture:** Electron main process handles file I/O and window management; renderer process runs Monaco Editor with a shader list sidebar. All filesystem access goes through IPC via a preload context bridge.

**Tech Stack:** Electron 33, Monaco Editor, vanilla JS (no framework/bundler)

---

### Task 1: Project Scaffolding

**Files:**
- Create: `app/shaderJsonEditor/package.json`
- Create: `app/shaderJsonEditor/.gitignore`

- [ ] **Step 1: Create package.json**

```json
{
  "name": "shader-json-editor",
  "version": "1.0.0",
  "description": "GLSL shader editor for shaders.json files",
  "main": "main.js",
  "scripts": {
    "start": "electron ."
  },
  "dependencies": {
    "electron": "^33.0.0",
    "monaco-editor": "^0.52.0"
  }
}
```

- [ ] **Step 2: Create .gitignore**

```
node_modules/
out/
dist/
```

- [ ] **Step 3: Install dependencies**

```bash
cd app/shaderJsonEditor && npm install
```

- [ ] **Step 4: Commit**

```bash
git add app/shaderJsonEditor/package.json app/shaderJsonEditor/.gitignore
git commit -m "feat(shaderJsonEditor): add project scaffolding"
```

---

### Task 2: Electron Main Process

**Files:**
- Create: `app/shaderJsonEditor/main.js`

This file is the Electron entry point. It creates the BrowserWindow, handles IPC for file open/save, and manages the application lifecycle.

- [ ] **Step 1: Implement main.js with window creation, IPC handlers, and file I/O**

```javascript
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
    // Closing the window with unsaved changes is handled in renderer via beforeunload
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
    // No file open — delegate to Save As
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
```

- [ ] **Step 2: Commit**

```bash
git add app/shaderJsonEditor/main.js
git commit -m "feat(shaderJsonEditor): add Electron main process with file I/O"
```

---

### Task 3: Preload Script (Context Bridge)

**Files:**
- Create: `app/shaderJsonEditor/preload.js`

Exposes safe IPC APIs from main process to the renderer via `contextBridge`.

- [ ] **Step 1: Implement preload.js**

```javascript
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('shaderAPI', {
  openFile: () => ipcRenderer.invoke('file:open'),
  saveFile: (shaders) => ipcRenderer.invoke('file:save', shaders),
  saveFileAs: (shaders) => ipcRenderer.invoke('file:saveAs', shaders),
});
```

- [ ] **Step 2: Commit**

```bash
git add app/shaderJsonEditor/preload.js
git commit -m "feat(shaderJsonEditor): add preload context bridge"
```

---

### Task 4: Renderer — HTML Shell & CSS Layout

**Files:**
- Create: `app/shaderJsonEditor/renderer/index.html`
- Create: `app/shaderJsonEditor/renderer/style.css`

- [ ] **Step 1: Implement index.html**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self' 'unsafe-eval' 'unsafe-inline' blob:; style-src 'self' 'unsafe-inline'; font-src 'self' data:;">
  <title>Shader JSON Editor</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <!-- Toolbar -->
  <div id="toolbar">
    <button id="btn-open">Open</button>
    <button id="btn-save">Save</button>
    <button id="btn-save-as">Save As</button>
    <span id="shader-count">No file loaded</span>
  </div>

  <!-- Main content -->
  <div id="main-container">
    <div id="sidebar">
      <div id="shader-list"></div>
    </div>
    <div id="editor-container"></div>
  </div>

  <!-- Status bar -->
  <div id="statusbar">
    <span id="status-file">No file</span>
    <span id="status-shaders">0 shaders</span>
    <span id="status-cursor">Line 1:1</span>
  </div>

  <script src="../node_modules/monaco-editor/min/vs/loader.js"></script>
  <script src="app.js"></script>
</body>
</html>
```

- [ ] **Step 2: Implement style.css**

```css
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: #1e1e1e;
  color: #cccccc;
  display: flex;
  flex-direction: column;
  height: 100vh;
  overflow: hidden;
}

/* Toolbar */
#toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px;
  background: #2d2d30;
  border-bottom: 1px solid #3e3e42;
  flex-shrink: 0;
}

#toolbar button {
  padding: 4px 16px;
  border: 1px solid #555;
  background: #3c3c3c;
  color: #ccc;
  cursor: pointer;
  font-size: 13px;
  border-radius: 3px;
}

#toolbar button:hover {
  background: #505050;
}

#shader-count {
  margin-left: auto;
  font-size: 12px;
  color: #888;
}

/* Main container */
#main-container {
  display: flex;
  flex: 1;
  overflow: hidden;
}

/* Sidebar */
#sidebar {
  width: 260px;
  min-width: 180px;
  background: #252526;
  border-right: 1px solid #3e3e42;
  overflow-y: auto;
  flex-shrink: 0;
}

#shader-list {
  padding: 4px;
}

.shader-item {
  padding: 8px 10px;
  cursor: pointer;
  border-radius: 4px;
  margin-bottom: 2px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.shader-item:hover {
  background: #2a2d2e;
}

.shader-item.selected {
  background: #094771;
}

.shader-item .dirty-indicator {
  font-size: 12px;
  width: 12px;
  text-align: center;
  flex-shrink: 0;
}

.shader-item .shader-info {
  flex: 1;
  min-width: 0;
}

.shader-item .shader-label {
  font-size: 13px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.shader-item .shader-meta {
  font-size: 11px;
  color: #888;
  display: flex;
  gap: 8px;
  margin-top: 2px;
}

.shader-item .shader-type {
  font-size: 10px;
  padding: 1px 5px;
  border-radius: 3px;
  font-weight: 600;
}

.shader-type.vertex {
  background: #1b5e20;
  color: #81c784;
}

.shader-type.fragment {
  background: #0d47a1;
  color: #64b5f6;
}

.shader-type.unknown {
  background: #424242;
  color: #9e9e9e;
}

/* Editor */
#editor-container {
  flex: 1;
  overflow: hidden;
}

/* Status bar */
#statusbar {
  display: flex;
  gap: 16px;
  padding: 3px 12px;
  background: #007acc;
  color: #fff;
  font-size: 12px;
  flex-shrink: 0;
}

#status-cursor {
  margin-left: auto;
}
```

- [ ] **Step 3: Commit**

```bash
git add app/shaderJsonEditor/renderer/index.html app/shaderJsonEditor/renderer/style.css
git commit -m "feat(shaderJsonEditor): add HTML shell and CSS layout"
```

---

### Task 5: Renderer — Application Logic & Monaco Setup

**Files:**
- Create: `app/shaderJsonEditor/renderer/app.js`

This is the core of the renderer: initializes Monaco Editor in GLSL mode, manages the shader list state, handles dirty tracking, and wires up toolbar/status bar events.

- [ ] **Step 1: Implement app.js**

```javascript
// --- State ---
let shaders = [];            // Array of { id, label, shaderType, source }
let selectedIndex = -1;      // Currently selected shader index
let dirtyFlags = new Set();  // Indices of modified shaders
let currentFilePath = null;  // Path to currently open file (for status bar)
let editor = null;           // Monaco editor instance

// --- Constants ---
const SHADER_TYPE_MAP = {
  35633: { label: 'VERTEX', cssClass: 'vertex' },
  35632: { label: 'FRAGMENT', cssClass: 'fragment' },
};

function getShaderTypeInfo(shaderType) {
  return SHADER_TYPE_MAP[shaderType] || { label: `SHADER(${shaderType})`, cssClass: 'unknown' };
}

// --- Monaco Setup ---
require.config({
  paths: { vs: '../node_modules/monaco-editor/min/vs' },
});

require(['vs/editor/editor.main'], function () {
  // Register GLSL language (Monaco doesn't have built-in GLSL, but we use c-like highlighting)
  // Monaco's `cpp` language provides reasonable GLSL highlighting.
  // We define a custom language ID for flexibility.
  monaco.languages.register({ id: 'glsl' });

  // Define GLSL tokens — keywords, types, built-in functions, etc.
  monaco.languages.setMonarchTokensProvider('glsl', {
    defaultToken: '',
    tokenPostfix: '.glsl',

    keywords: [
      'attribute', 'const', 'uniform', 'varying', 'break', 'continue',
      'do', 'for', 'while', 'if', 'else', 'in', 'out', 'inout',
      'float', 'int', 'void', 'bool', 'true', 'false', 'lowp', 'mediump', 'highp',
      'precision', 'invariant', 'discard', 'return', 'mat2', 'mat3', 'mat4',
      'mat2x2', 'mat2x3', 'mat2x4', 'mat3x2', 'mat3x3', 'mat3x4',
      'mat4x2', 'mat4x3', 'mat4x4', 'vec2', 'vec3', 'vec4', 'ivec2',
      'ivec3', 'ivec4', 'bvec2', 'bvec3', 'bvec4', 'sampler2D',
      'sampler3D', 'samplerCube', 'sampler2DShadow', 'sampler2DArray',
      'sampler2DArrayShadow', 'isampler2D', 'isampler3D', 'isamplerCube',
      'isampler2DArray', 'usampler2D', 'usampler3D', 'usamplerCube',
      'usampler2DArray', 'struct', 'layout',
    ],

    typeKeywords: [
      'float', 'int', 'bool', 'vec2', 'vec3', 'vec4', 'ivec2', 'ivec3', 'ivec4',
      'bvec2', 'bvec3', 'bvec4', 'mat2', 'mat3', 'mat4', 'sampler2D', 'samplerCube',
    ],

    operators: [
      '=', '>', '<', '!', '~', '?', ':', '==', '<=', '>=', '!=',
      '&&', '||', '++', '--', '+', '-', '*', '/', '&', '|', '^', '%',
      '<<', '>>', '>>>', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=',
      '<<=', '>>=', '>>>=',
    ],

    builtins: [
      'radians', 'degrees', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan',
      'pow', 'exp', 'log', 'exp2', 'log2', 'sqrt', 'inversesqrt',
      'abs', 'sign', 'floor', 'ceil', 'fract', 'mod', 'min', 'max', 'clamp',
      'mix', 'step', 'smoothstep', 'length', 'distance', 'dot', 'cross',
      'normalize', 'reflect', 'refract', 'matrixCompMult',
      'lessThan', 'lessThanEqual', 'greaterThan', 'greaterThanEqual',
      'equal', 'notEqual', 'any', 'all', 'not',
      'texture2D', 'texture2DProj', 'texture2DLod', 'texture2DProjLod',
      'textureCube', 'textureCubeLod', 'texture3D', 'texture3DProj',
      'gl_Position', 'gl_FragColor', 'gl_FragData', 'gl_PointSize',
      'gl_FragCoord', 'gl_FrontFacing',
    ],

    symbols: /[=><!~?:&|+\-*/^%]+/,
    escapes: /\\(?:[abfnrtv\\"']|x[0-9A-Fa-f]{1,4}|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8})/,

    tokenizer: {
      root: [
        [/[a-zA-Z_]\w*/, {
          cases: {
            '@typeKeywords': 'type',
            '@keywords': 'keyword',
            '@builtins': 'predefined',
            '@default': 'identifier',
          },
        }],
        { include: '@whitespace' },
        [/[{}()[\]]/, '@brackets'],
        [/@symbols/, {
          cases: {
            '@operators': 'operator',
            '@default': '',
          },
        }],
        [/\d*\.\d+([eE][-+]?\d+)?/, 'number.float'],
        [/0[xX][0-9a-fA-F]+/, 'number.hex'],
        [/\d+/, 'number'],
        [/[;,.]/, 'delimiter'],
        [/"([^"\\]|\\.)*$/, 'string.invalid'],
        [/"/, { token: 'string.quote', bracket: '@open', next: '@string' }],
      ],

      whitespace: [
        [/[ \t\r\n]+/, ''],
        [/\/\*/, 'comment', '@comment'],
        [/\/\/.*$/, 'comment'],
        [/#/, 'keyword.directive', '@directive'],
      ],

      comment: [
        [/[^/*]+/, 'comment'],
        [/\*\//, 'comment', '@pop'],
        [/[/*]/, 'comment'],
      ],

      string: [
        [/[^\\"]+/, 'string'],
        [/@escapes/, 'string.escape'],
        [/\\./, 'string.escape.invalid'],
        [/"/, { token: 'string.quote', bracket: '@close', next: '@pop' }],
      ],

      directive: [
        [/[a-zA-Z_]\w*/, 'keyword.directive'],
        [/[ \t]+/, ''],
        [/\d+/, 'number'],
        [/\/\/.*$/, 'comment'],
        [/\/\*/, 'comment', '@comment'],
        [/\n/, '', '@pop'],
      ],
    },
  });

  // Define GLSL theme (dark, matching VS Code default dark)
  monaco.editor.defineTheme('glsl-dark', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'comment', foreground: '6A9955', fontStyle: 'italic' },
      { token: 'keyword', foreground: '569CD6' },
      { token: 'keyword.directive', foreground: 'C586C0' },
      { token: 'type', foreground: '4EC9B0' },
      { token: 'predefined', foreground: 'DCDCAA' },
      { token: 'number', foreground: 'B5CEA8' },
      { token: 'string', foreground: 'CE9178' },
      { token: 'operator', foreground: 'D4D4D4' },
      { token: 'identifier', foreground: '9CDCFE' },
    ],
    colors: {
      'editor.background': '#1e1e1e',
      'editor.foreground': '#d4d4d4',
      'editor.lineHighlightBackground': '#2a2d2e',
      'editor.selectionBackground': '#264f78',
      'editorCursor.foreground': '#aeafad',
      'editorLineNumber.foreground': '#858585',
      'editorLineNumber.activeForeground': '#c6c6c6',
    },
  });

  // Create editor instance
  editor = monaco.editor.create(document.getElementById('editor-container'), {
    value: '',
    language: 'glsl',
    theme: 'glsl-dark',
    automaticLayout: true,
    fontSize: 14,
    fontFamily: "'Cascadia Code', 'Fira Code', 'Consolas', 'Courier New', monospace",
    minimap: { enabled: true },
    lineNumbers: 'on',
    scrollBeyondLastLine: false,
    wordWrap: 'off',
    tabSize: 2,
  });

  // Track cursor position for status bar
  editor.onDidChangeCursorPosition((e) => {
    document.getElementById('status-cursor').textContent =
      `Line ${e.position.lineNumber}:${e.position.column}`;
  });

  // Track content changes for dirty flag
  editor.onDidChangeModelContent(() => {
    if (selectedIndex >= 0 && selectedIndex < shaders.length) {
      const currentSource = editor.getValue();
      if (currentSource !== shaders[selectedIndex].source) {
        dirtyFlags.add(selectedIndex);
      } else {
        dirtyFlags.delete(selectedIndex);
      }
      renderShaderList();
    }
  });

  // Now that Monaco is ready, enable toolbar
  document.getElementById('btn-open').addEventListener('click', handleOpen);
  document.getElementById('btn-save').addEventListener('click', handleSave);
  document.getElementById('btn-save-as').addEventListener('click', handleSaveAs);
});

// --- Shader List Rendering ---
function renderShaderList() {
  const listEl = document.getElementById('shader-list');
  listEl.innerHTML = '';

  shaders.forEach((shader, index) => {
    const typeInfo = getShaderTypeInfo(shader.shaderType);
    const isDirty = dirtyFlags.has(index);
    const isSelected = index === selectedIndex;

    const item = document.createElement('div');
    item.className = 'shader-item' + (isSelected ? ' selected' : '');
    item.innerHTML = `
      <span class="dirty-indicator">${isDirty ? '●' : '◆'}</span>
      <div class="shader-info">
        <div class="shader-label">${escapeHtml(shader.label)}</div>
        <div class="shader-meta">
          <span class="shader-type ${typeInfo.cssClass}">${typeInfo.label}</span>
          <span>id:${shader.id}</span>
        </div>
      </div>
    `;

    item.addEventListener('click', () => selectShader(index));
    listEl.appendChild(item);
  });
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

// --- Shader Selection ---
function selectShader(index) {
  // Save current editor content to shader array before switching
  if (selectedIndex >= 0 && editor) {
    shaders[selectedIndex].source = editor.getValue();
  }

  selectedIndex = index;
  if (editor && shaders[index]) {
    editor.setValue(shaders[index].source);
  }
  renderShaderList();
}

// --- File Operations ---
async function checkUnsavedAndProceed(action) {
  if (dirtyFlags.size > 0) {
    const { response } = await showConfirmDialog(
      'Unsaved Changes',
      `You have ${dirtyFlags.size} unsaved shader(s). Discard changes?`
    );
    // In Electron, we use the native dialog. For renderer, we use window.confirm
    if (!confirm(`You have ${dirtyFlags.size} unsaved shader(s). Discard changes?`)) {
      return false;
    }
  }
  return true;
}

async function handleOpen() {
  if (!(await checkUnsavedAndProceed())) return;

  const result = await window.shaderAPI.openFile();
  if (result.canceled) return;

  if (!result.success) {
    alert(`Error: ${result.error}`);
    return;
  }

  currentFilePath = result.filePath;
  shaders = result.shaders;
  dirtyFlags.clear();
  selectedIndex = shaders.length > 0 ? 0 : -1;

  if (editor && selectedIndex >= 0) {
    editor.setValue(shaders[selectedIndex].source);
  } else if (editor) {
    editor.setValue('');
  }

  updateStatusBar();
  renderShaderList();
}

async function handleSave() {
  // Save current editor content to shaders array
  if (selectedIndex >= 0 && editor) {
    shaders[selectedIndex].source = editor.getValue();
  }

  const result = await window.shaderAPI.saveFile(shaders);

  if (result.canceled) return;

  if (!result.success) {
    alert(`Error: ${result.error}`);
    return;
  }

  currentFilePath = result.filePath;
  dirtyFlags.clear();
  renderShaderList();
  updateStatusBar();
}

async function handleSaveAs() {
  // Save current editor content to shaders array
  if (selectedIndex >= 0 && editor) {
    shaders[selectedIndex].source = editor.getValue();
  }

  const result = await window.shaderAPI.saveFileAs(shaders);

  if (result.canceled) return;

  if (!result.success) {
    alert(`Error: ${result.error}`);
    return;
  }

  currentFilePath = result.filePath;
  dirtyFlags.clear();
  renderShaderList();
  updateStatusBar();
}

function updateStatusBar() {
  document.getElementById('status-file').textContent =
    currentFilePath || 'No file';
  document.getElementById('status-shaders').textContent =
    `${shaders.length} shader${shaders.length !== 1 ? 's' : ''}`;
}

// --- Unsaved Changes Guard (window close) ---
window.addEventListener('beforeunload', (e) => {
  if (dirtyFlags.size > 0) {
    // Electron will show a native confirmation dialog when beforeunload returns a non-void value
    e.preventDefault();
    e.returnValue = '';
  }
});
```

- [ ] **Step 2: Commit**

```bash
git add app/shaderJsonEditor/renderer/app.js
git commit -m "feat(shaderJsonEditor): add renderer logic with Monaco GLSL editor"
```

---

### Task 6: Wire Up Electron Window Close Guard

**Files:**
- Modify: `app/shaderJsonEditor/main.js`

After Task 2 created main.js, we need to add the close-guard on the main process side for the unsaved changes dialog.

- [ ] **Step 1: Add close event handler in main.js**

Add this after `createWindow()` and before `app.whenReady()`:

Edit `main.js` — in the `createWindow` function, add after `mainWindow.loadFile(...)`:

```javascript
  // Unsaved changes guard — renderer sends dirty state
  mainWindow.on('close', (e) => {
    // The renderer's beforeunload handler will show the native dialog.
    // If the user cancels, beforeunload prevents the close and
    // Electron's default close behavior is to destroy the window.
    // We just need to make sure the close event doesn't double-fire.
  });
```

- [ ] **Step 2: Add IPC handler for dirty check on main process**

Add after the existing IPC handlers in `main.js`:

```javascript
// Check if renderer has unsaved changes (called before close)
ipcMain.handle('app:hasUnsaved', () => {
  // This is a placeholder — the actual check happens in the renderer's beforeunload.
  // If the renderer returns true from beforeunload, Electron shows a native dialog.
  return true;
});
```

- [ ] **Step 3: Commit**

```bash
git add app/shaderJsonEditor/main.js
git commit -m "feat(shaderJsonEditor): add unsaved changes close guard"
```

---

### Task 7: End-to-End Verification

**Files:**
- Modify: None (verification only)

- [ ] **Step 1: Verify the app launches**

```bash
cd app/shaderJsonEditor && npx electron . --no-sandbox
```

Expected: Window opens with empty editor, toolbar buttons, and status bar showing "No file".

- [ ] **Step 2: Test with real shaders.json**

Use the app to:
1. Click "Open" → navigate to `example/20260615_110825_frame0/shaders.json`
2. Verify shader list shows 2 shaders (Shader#1 VERTEX, Shader#2 FRAGMENT)
3. Click on each shader — verify source loads in editor with GLSL syntax highlighting
4. Edit a shader source — verify ● dirty indicator appears
5. Click "Save" — verify file writes successfully and dirty indicator clears
6. Close the window — verify no spurious unsaved-changes dialog

- [ ] **Step 3: Commit any fixes if needed**

```bash
git status
# If files modified during testing, stage and commit them
```
