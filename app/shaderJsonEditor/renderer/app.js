// --- State ---
let shaders = [];            // Array of { id, label, shaderType, source }
let selectedIndex = -1;      // Currently selected shader index
let dirtyFlags = new Set();  // Indices of modified shaders
let currentFilePath = null;  // Path to currently open file (for status bar)
let editor = null;           // Monaco editor instance

// --- Constants ---
var SHADER_TYPE_MAP = {
  35633: { label: 'VERTEX', cssClass: 'vertex' },
  35632: { label: 'FRAGMENT', cssClass: 'fragment' },
};

function getShaderTypeInfo(shaderType) {
  return SHADER_TYPE_MAP[shaderType] || { label: 'SHADER(' + shaderType + ')', cssClass: 'unknown' };
}

// --- Monaco Setup ---
require.config({
  paths: { vs: '../node_modules/monaco-editor/min/vs' },
});

require(['vs/editor/editor.main'], function () {
  // Register GLSL language
  monaco.languages.register({ id: 'glsl' });

  // Define GLSL tokens — keywords, types, built-in functions
  monaco.languages.setMonarchTokensProvider('glsl', {
    defaultToken: '',
    tokenPostfix: '.glsl',

    keywords: [
      'attribute', 'const', 'uniform', 'varying', 'break', 'continue',
      'do', 'for', 'while', 'if', 'else', 'in', 'out', 'inout',
      'void', 'bool', 'true', 'false', 'lowp', 'mediump', 'highp',
      'precision', 'invariant', 'discard', 'return', 'struct', 'layout',
    ],

    typeKeywords: [
      'float', 'int', 'vec2', 'vec3', 'vec4', 'ivec2', 'ivec3', 'ivec4',
      'bvec2', 'bvec3', 'bvec4', 'mat2', 'mat3', 'mat4', 'mat2x2', 'mat2x3', 'mat2x4',
      'mat3x2', 'mat3x3', 'mat3x4', 'mat4x2', 'mat4x3', 'mat4x4',
      'sampler2D', 'sampler3D', 'samplerCube', 'sampler2DShadow', 'sampler2DArray',
      'sampler2DArrayShadow', 'isampler2D', 'isampler3D', 'isamplerCube',
      'isampler2DArray', 'usampler2D', 'usampler3D', 'usamplerCube', 'usampler2DArray',
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

  // Define GLSL dark theme
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
  editor.onDidChangeCursorPosition(function (e) {
    document.getElementById('status-cursor').textContent =
      'Line ' + e.position.lineNumber + ':' + e.position.column;
  });

  // Track content changes for dirty flag
  editor.onDidChangeModelContent(function () {
    if (selectedIndex >= 0 && selectedIndex < shaders.length) {
      var currentSource = editor.getValue();
      if (currentSource !== shaders[selectedIndex].source) {
        dirtyFlags.add(selectedIndex);
      } else {
        dirtyFlags.delete(selectedIndex);
      }
      renderShaderList();
    }
  });

  // Wire up toolbar buttons (after Monaco is ready)
  document.getElementById('btn-open').addEventListener('click', handleOpen);
  document.getElementById('btn-save').addEventListener('click', handleSave);
  document.getElementById('btn-save-as').addEventListener('click', handleSaveAs);
});

// --- Shader List Rendering ---
function renderShaderList() {
  var listEl = document.getElementById('shader-list');
  listEl.innerHTML = '';

  shaders.forEach(function (shader, index) {
    var typeInfo = getShaderTypeInfo(shader.shaderType);
    var isDirty = dirtyFlags.has(index);
    var isSelected = index === selectedIndex;

    var item = document.createElement('div');
    item.className = 'shader-item' + (isSelected ? ' selected' : '');
    item.innerHTML =
      '<span class="dirty-indicator">' + (isDirty ? '●' : '◆') + '</span>' +
      '<div class="shader-info">' +
        '<div class="shader-label">' + escapeHtml(shader.label) + '</div>' +
        '<div class="shader-meta">' +
          '<span class="shader-type ' + typeInfo.cssClass + '">' + typeInfo.label + '</span>' +
          '<span>id:' + shader.id + '</span>' +
        '</div>' +
      '</div>';

    item.addEventListener('click', (function (idx) {
      return function () { selectShader(idx); };
    })(index));
    listEl.appendChild(item);
  });
}

function escapeHtml(text) {
  var div = document.createElement('div');
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
async function checkUnsavedAndProceed() {
  if (dirtyFlags.size > 0) {
    if (!confirm('You have ' + dirtyFlags.size + ' unsaved shader(s). Discard changes?')) {
      return false;
    }
  }
  return true;
}

async function handleOpen() {
  if (!(await checkUnsavedAndProceed())) return;

  var result = await window.shaderAPI.openFile();
  if (result.canceled) return;

  if (!result.success) {
    alert('Error: ' + result.error);
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

  var result = await window.shaderAPI.saveFile(shaders);

  if (result.canceled) return;

  if (!result.success) {
    alert('Error: ' + result.error);
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

  var result = await window.shaderAPI.saveFileAs(shaders);

  if (result.canceled) return;

  if (!result.success) {
    alert('Error: ' + result.error);
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
    shaders.length + ' shader' + (shaders.length !== 1 ? 's' : '');
}

// --- Unsaved Changes Guard (window close) ---
window.addEventListener('beforeunload', function (e) {
  if (dirtyFlags.size > 0) {
    e.preventDefault();
    e.returnValue = '';
  }
});
