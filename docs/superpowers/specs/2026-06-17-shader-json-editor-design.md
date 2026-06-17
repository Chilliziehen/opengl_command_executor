# Shader JSON Editor — Design Spec

> **Goal:** A standalone Electron desktop app that opens `shaders.json`, provides GLSL editing with Monaco Editor syntax highlighting, and saves edits back to the same file.

> **Tech Stack:** Electron 33 + Monaco Editor + vanilla JS (no framework/bundler)

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Electron Main Process                          │
│  ┌──────────────┐  ┌────────────────────────┐   │
│  │ File I/O      │  │ Window Management      │   │
│  │ (read/write   │  │ (BrowserWindow)        │   │
│  │  JSON)        │  │                        │   │
│  └──────┬───────┘  └───────────┬────────────┘   │
│         │                      │                │
│         │    IPC Bridge        │                │
│         └──────────┬───────────┘                │
├────────────────────┼────────────────────────────┤
│  Renderer Process  │                            │
│  ┌─────────────────▼──────────────────────────┐ │
│  │  Monaco Editor (GLSL syntax highlighting)   │ │
│  │  Shader List (sidebar)                      │ │
│  │  Toolbar (Open / Save / Save As)            │ │
│  └────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

The renderer never touches the filesystem. All file I/O goes through IPC to the main process via a `preload.js` context bridge.

---

## UI Layout

```
┌──────────────────────────────────────────────────────┐
│  File  Edit  View                          _  □  ×  │
├────────────┬─────────────────────────────────────────┤
│  Toolbar   │ Open │ Save │ Save As │  (shader count) │
├────────────┴─────────────────────────────────────────┤
│ ┌─────────────────┐ ┌───────────────────────────────┐│
│ │ Shader List     │ │ Monaco Editor                 ││
│ │                 │ │  (GLSL highlighting)          ││
│ │ ┌─────────────┐ │ │                               ││
│ │ │ ◆ Shader#1  │ │ │ #version 300 es              ││
│ │ │   VERTEX    │ │ │ uniform mat4 u_MVP;          ││
│ │ │   id:1      │ │ │ in vec3 a_Position;          ││
│ │ └─────────────┘ │ │ in vec3 a_Color;             ││
│ │ ┌─────────────┐ │ │ out vec3 v_Color;            ││
│ │ │ ● Shader#2  │ │ │ void main() {                ││
│ │ │   FRAGMENT  │ │ │   ...                        ││
│ │ │   id:2      │ │ │ }                             ││
│ │ └─────────────┘ │ │                               ││
│ └─────────────────┘ └───────────────────────────────┘│
│ Status: shaders.json | 2 shaders | Line 5:12          │
└──────────────────────────────────────────────────────┘
```

- **Left panel** (~250px): scrollable shader list. Each item: label, type badge (VERTEX/FRAGMENT), id. ◆ = clean, ● = dirty.
- **Right panel**: Monaco Editor in GLSL language mode, with line numbers and minimap.
- **Toolbar**: Open, Save, Save As buttons + shader count badge.
- **Status bar**: current file path, shader count, cursor line:column.

---

## Data Flow

```
     [Open file dialog]
            │
            ▼
  ┌─────────────────────┐
  │ Main: read JSON     │──▶ parse & validate (array of shader objects)
  │ (filesystem)         │
  └──────┬──────────────┘
         │ IPC: { shaders: [...] }
         ▼
  ┌─────────────────────┐
  │ Renderer: store     │──▶ populate shader list sidebar
  │ shader array in      │──▶ select first shader → load into Monaco
  │ state               │
  └──────┬──────────────┘
         │ User edits in Monaco
         │ (onChange → mark shader dirty)
         ▼
  ┌─────────────────────┐
  │ Renderer: track     │──▶ dirty flag per shader (● indicator)
  │ edits per shader    │
  └──────┬──────────────┘
         │ User clicks Save
         │ IPC: { shaders: [...] }
         ▼
  ┌─────────────────────┐
  │ Main: serialize     │──▶ write JSON back to filesystem
  │ to JSON, write file │──▶ preserve order & all fields
  └─────────────────────┘
```

---

## Key Behaviors

- **Dirty tracking**: When Monaco source content differs from the original loaded source, the shader list item shows a ● indicator. Save clears all dirty flags.
- **Unsaved changes guard**: If dirty shaders exist and user triggers Open or closes the window, a native confirmation dialog appears ("You have unsaved changes. Discard them?").
- **Validation**: On save, the main process validates each shader object has `id` (number), `label` (string), `shaderType` (number), `source` (string) before writing.
- **Pretty-print**: JSON output uses 2-space indent, matching existing `shaders.json` style.
- **Format preservation**: All fields from the input JSON are preserved in output. Unknown extra fields pass through unchanged.

---

## Shader Type Display

| `shaderType` value | GL enum | Display badge |
|---|---|---|
| 35633 | `GL_VERTEX_SHADER` | `VERTEX` |
| 35632 | `GL_FRAGMENT_SHADER` | `FRAGMENT` |
| Other | Unknown | `SHADER(<num>)` |

---

## Save Format

Output matches input structure exactly:

```json
[
  {
    "id": 1,
    "label": "Shader#1",
    "shaderType": 35633,
    "source": "#version 300 es\nuniform mat4 u_MVP;\n..."
  },
  {
    "id": 2,
    "label": "Shader#2",
    "shaderType": 35632,
    "source": "#version 300 es\nprecision highp float;\n..."
  }
]
```

---

## Project Structure

```
app/shaderJsonEditor/
├── package.json              # Electron + Monaco deps, npm scripts
├── main.js                   # Electron main process (window, IPC, file I/O)
├── preload.js                # Context bridge (exposes safe IPC APIs to renderer)
├── renderer/
│   ├── index.html            # Shell HTML
│   ├── style.css             # Layout & theming (dark VS Code-like theme)
│   └── app.js                # Renderer logic: state, Monaco setup, IPC calls
└── .gitignore                # node_modules/, out/
```

**Dependencies:**
- `electron` — desktop shell, window management, file dialogs
- `monaco-editor` — GLSL syntax highlighting, minimap, bracket matching, undo/redo

**No bundler, no framework.** Vanilla JS in renderer. Monaco loads via AMD loader with `file://` protocol.

---

## Error Handling

- **File not found**: Dialog "File not found: <path>"
- **Invalid JSON**: Dialog "Failed to parse shaders.json: <parse error>" — file not opened
- **Invalid shader structure**: Dialog "Invalid shader at index N: missing field '<field>'" — save rejected
- **File write error**: Dialog "Failed to save: <system error>"
- **Dirty + close window**: Native confirm dialog via `beforeunload` + main process `close` event

---

## Out of Scope

- Shader compilation/validation (no GLSL compiler integration)
- Diff/compare between shaders
- Multi-file editing (only one `shaders.json` at a time)
- Integration with guiReplayer (standalone tool)
- Adding/removing shaders (edit existing only — can be added later)
