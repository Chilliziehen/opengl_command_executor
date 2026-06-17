# TuanjieRelic Capture 数据结构规约

> **面向读者：** Replayer 开发者——需要消费 capture 输出的 JSON/bin 文件来还原 GL 状态并回放渲染帧。

---

## 1. 概览

一次 capture 产生一个**帧目录**，包含 GL 命令流、资源快照和运行时状态。目录结构如下：

```
<captureName>/
├── manifest.json          — 索引 & 元数据（入口文件）
├── commands.json          — GL 命令流（按 eventId 排序）
├── state.json             — 帧起始时的完整 GL 上下文状态
├── shaders.json           — 着色器源码列表
├── programs.json           — 着色器程序列表
├── framebuffers.json       — 帧缓冲对象列表
├── vaos.json              — 顶点数组对象列表
├── debug_log.json         — 调试日志
├── runtime_logs.json      — 运行时日志
├── buffers/
│   ├── Buffer#N.json      — 缓冲区元数据
│   └── Buffer#N.bin       — 缓冲区二进制数据
├── textures/
│   ├── Texture#N.json     — 纹理元数据
│   └── Texture#N.bin      — 纹理像素数据（降采样）
└── uploads/               — 帧内上传数据（可选，可能为空）
    └── upload_N.bin
```

**加载顺序：** `manifest.json` → `state.json` → `shaders.json` → `programs.json` → `framebuffers.json` → `vaos.json` → `buffers/*.json` + `.bin` → `textures/*.json` + `.bin` → `commands.json`。

---

## 2. manifest.json — 索引与元数据

入口文件，声明 capture 格式版本、所有文件的路径和诊断信息。

```typescript
interface Manifest {
  format: "tjrelic.wx.capture";       // 格式标识符
  version: 2;                          // 格式版本号

  // ── 基本信息 ──────────────────────────────────────────
  captureModel: CaptureModel;          // 捕获模型（见下）
  generatedAt: string;                // ISO 8601 时间戳
  frameIndex: number;                 // 帧序号
  canvas: {                            // 画布信息
    width: number;                     //   物理像素宽
    height: number;                   //   物理像素高
    cssWidth: number;                 //   CSS 像素宽
    cssHeight: number;                //   CSS 像素高
    devicePixelRatio: number;         //   DPR
  };

  // ── 回放兼容性 ────────────────────────────────────────
  replayerCompatibility: {
    currentStandaloneReplayer: boolean;  // 是否可独立回放
    hasPreviewPayload: boolean;         // 是否有预览数据
    reason: string;                      // 不可回放时的原因
  };

  // ── 诊断信息 ──────────────────────────────────────────
  diagnostics: {
    profile: string;                    // 捕获 profile 名
    commandLimit: number;               // 命令上限
    commandCount: number;               // 实际命令数
    droppedCommandCount: number;        // 丢弃命令数
    uniformCommandCount: number;        // uniform 命令数
    commandLimitReached: boolean;       // 是否达到上限
    largeUploadSkipCount: number;       // 大上传跳过数
    resourceMetadataEnabled: boolean;
    resourceBaselineMode: string;       // "all"|"ubo"|"none"
    maxResourceBaselineBytes: number;
    resourceSnapshotEnabled: boolean;
    bufferUploadPayloadEnabled: boolean;
    textureUploadPayloadEnabled: boolean;
    maxUploadBytes: number;
    uniformValueMode: string;           // "all"|"scalars-and-matrices"|"none"
    maxUniformDataElements: number;
  };

  // ── 文件索引 ──────────────────────────────────────────
  files: {
    commands: "commands.json";
    state: "state.json";
    shaders: "shaders.json";
    programs: "programs.json";
    framebuffers: "framebuffers.json";
    vaos: "vaos.json";
    debugLog?: "debug_log.json";
    runtimeLogs?: "runtime_logs.json";
    buffers: BufferFileEntry[];
    textures: TextureFileEntry[];
    uploads: UploadFileEntry[];
  };
}

type CaptureModel =
  | "gl-command-resource"              // 全量资源快照
  | "gl-command-resource-partial"       // 部分资源基线（UBO 等）
  | "gl-command-resource-metadata"     // 仅资源元数据
  | string;                             // 其他 profile 名

interface BufferFileEntry {
  id: number;
  label: string;
  jsonPath: string;                     // "buffers/Buffer#N.json"
  binPath: string;                      // "buffers/Buffer#N.bin"
  byteLength: number;
}

interface TextureFileEntry {
  id: number;
  label: string;
  jsonPath: string;
  binPath: string;
  byteLength: number;
  width: number;
  height: number;
}

interface UploadFileEntry {
  path: string;
  byteLength: number;
}
```

**Replayer 消费方式：** 读取 `manifest.files` 获取所有文件路径，按需加载。`replayerCompatibility.currentStandaloneReplayer` 为 `false` 时不可独立回放。

---

## 3. state.json — 帧起始 GL 上下文状态

帧捕获**开始时**的完整 GL 状态快照。这是命令流执行**之前**的状态，Replayer 需要先还原此状态，然后逐步执行 commands.json 中的命令。

### 3.1 完整字段

```typescript
interface GLState {
  // ── ShaderState ─────────────────────────────────────────
  currentProgramId: number;             // 当前 useProgram 的 program ID（0=无）

  // ── FramebufferState ───────────────────────────────────
  currentFramebufferId: number;         // GL_DRAW_FRAMEBUFFER_BINDING（0=默认 FBO）
  readFramebufferId: number;            // GL_READ_FRAMEBUFFER_BINDING（0=默认 FBO）
  drawBuffers: number[];                // glDrawBuffers 的 GLenum 数组
  readBuffer: number;                   // glReadBuffer 的 GLenum（默认 0x0405=GL_BACK）

  // ── ResourceBindingState ───────────────────────────────
  currentActiveTexture: number;         // 当前活跃纹理单元（0-based）
  currentTextures: Record<number, number>;  // 纹理单元 → Texture ID
  currentSamplers: Record<number, Record<string, number>>;  // programId → {uniformName → textureUnit}
  uniformBufferBindings: Record<number, {  // binding point → UBO 绑定
    bufferId: number;                   //   Buffer ID
    offset: number;                     //   GLintptr（bindBufferBase 时为 0）
    size: number;                       //   GLsizeiptr（bindBufferBase 时为 0，表示整个 buffer）
  }>;
  samplerObjects: Record<number, number>;  // 纹理单元 → Sampler ID

  // ── VertexInputState ──────────────────────────────────
  currentVertexArrayId: number;          // 当前 VAO（0=默认 VAO）
  currentArrayBufferId: number;         // GL_ARRAY_BUFFER_BINDING
  currentElementArrayBufferId: number;  // GL_ELEMENT_ARRAY_BUFFER_BINDING
  attribs: Record<number, {            // 顶点属性索引 → 属性状态
    enabled: boolean;
    size: number;                       // 1-4 或 GL_BGRA
    type: number;                       // GLenum（5126=FLOAT 等）
    normalized: boolean;
    stride: number;
    offset: number;
    bufferId: number;                   // 绑定的 VBO Buffer ID
    name: string;                       // 属性名（从 getActiveAttrib 获取）
  }>;
  vertexBufferBindings: Record<number, {  // WebGL2 顶点绑定点 → 状态
    bufferId: number;
    offset: number;
    stride: number;
  }>;
  vertexBindingDivisors: Record<number, number>;  // 绑定点 → divisor

  // ── RasterizerState ───────────────────────────────────
  viewport: [number, number, number, number];  // [x, y, width, height]
  cullFaceMode: number;                 // GLenum（1029=GL_BACK）
  frontFace: number;                    // GLenum（2305=GL_CCW）
  scissorBox: [number, number, number, number];
  scissorTestEnabled: boolean;
  polygonOffsetFillEnabled: boolean;
  polygonOffsetFactor: number;
  polygonOffsetUnits: number;
  lineWidth: number;
  multisampleEnabled: boolean;

  // ── DepthStencilState ──────────────────────────────────
  depthFunc: number;                    // GLenum（513=GL_LESS）
  depthMask: boolean;
  depthRangeNear: number;
  depthRangeFar: number;
  stencilTestEnabled: boolean;
  stencilFront: StencilFaceState;
  stencilBack: StencilFaceState;
  clearDepth: number;
  clearStencil: number;

  // ── BlendState ────────────────────────────────────────
  blendEnabled: boolean;
  blendFunc: {
    srcRGB: number; dstRGB: number;
    srcAlpha: number; dstAlpha: number;
  };
  blendEquation: {
    rgb: number; alpha: number;
  };
  blendColor: [number, number, number, number];
  colorMask: [boolean, boolean, boolean, boolean];

  // ── PixelStorageState ──────────────────────────────────
  pixelStore: Record<string, number>;   // 旧格式：pname→value（兼容）
  pixelPack: PixelStoreParams;
  pixelUnpack: PixelStoreParams;
  pixelPackBuffer: number;             // GL_PIXEL_PACK_BUFFER_BINDING
  pixelUnpackBuffer: number;           // GL_PIXEL_UNPACK_BUFFER_BINDING

  // ── ClearState ─────────────────────────────────────────
  clearColor: [number, number, number, number];

  // ── Capabilities ──────────────────────────────────────
  capabilities: Record<string, boolean>;  // GLenum 名 → 是否启用
}

interface StencilFaceState {
  func: number;           // 比较函数 GLenum（0x0207=GL_ALWAYS）
  ref: number;            // 参考值
  compareMask: number;   // 比较掩码
  writeMask: number;     // 写入掩码
  sfail: number;         // 模板测试失败操作
  dpfail: number;        // 模板过/深度失败操作
  dppass: number;        // 模板过/深度过操作
}

interface PixelStoreParams {
  swapBytes: boolean;
  lsbFirst: boolean;
  rowLength: number;
  imageHeight: number;
  skipRows: number;
  skipPixels: number;
  skipImages: number;
  alignment: number;     // 默认 4
}
```

### 3.2 状态还原顺序

Replayer 恢复状态时应按以下顺序执行：

1. `glViewport` ← `viewport`
2. `glBindFramebuffer(GL_FRAMEBUFFER, ...)` ← `currentFramebufferId`
3. `glUseProgram(...)` ← `currentProgramId`
4. `glBindVertexArray(...)` ← `currentVertexArrayId`
5. `glBindBuffer(GL_ARRAY_BUFFER, ...)` ← `currentArrayBufferId`
6. `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...)` ← `currentElementArrayBufferId`
7. 设置顶点属性 ← `attribs`
8. 绑定纹理 ← `currentTextures` + `currentActiveTexture`
9. 绑定 UBO ← `uniformBufferBindings`（`glBindBufferRange` / `glBindBufferBase`）
10. 设置混合 ← `blendEnabled` / `blendFunc` / `blendEquation` / `blendColor` / `colorMask`
11. 设置深度/模板 ← `depthFunc` / `depthMask` / `stencilTestEnabled` / `stencilFront` / `stencilBack`
12. 设置光栅化 ← `cullFaceMode` / `frontFace` / `scissorTestEnabled` / `polygonOffset*`
13. 设置像素存储 ← `pixelPack` / `pixelUnpack`
14. 设置清屏色 ← `clearColor` / `clearDepth` / `clearStencil`
15. 启用/禁用 capabilities ← `capabilities`

### 3.3 GLenum 常用值速查

| 语义 | 值 | GL 常量 |
|---|---|---|
| GL_FLOAT | 5126 | `0x1406` |
| GL_UNSIGNED_SHORT | 5123 | `0x1403` |
| GL_UNSIGNED_BYTE | 5121 | `0x1401` |
| GL_LESS | 513 | `0x0201` |
| GL_ALWAYS | 519 | `0x0207` |
| GL_KEEP | 7680 | `0x1E00` |
| GL_BACK | 1029 | `0x0405` |
| GL_CCW | 2305 | `0x0901` |
| GL_FUNC_ADD | 32774 | `0x8006` |
| GL_ONE | 1 | `0x0001` |
| GL_ZERO | 0 | `0x0000` |
| GL_SRC_ALPHA | 770 | `0x0302` |
| GL_ONE_MINUS_SRC_ALPHA | 771 | `0x0303` |
| GL_COLOR_ATTACHMENT0 | 36064 | `0x8CE0` |
| GL_DEPTH_ATTACHMENT | 36096 | `0x8D00` |
| GL_DEPTH_STENCIL_ATTACHMENT | 33306 | `0x821A` |
| GL_FRAMEBUFFER | 36160 | `0x8D40` |
| GL_DRAW_FRAMEBUFFER | 36009 | `0x8CA9` |
| GL_READ_FRAMEBUFFER | 36008 | `0x8CA8` |
| GL_UNIFORM_BUFFER | 35345 | `0x8A11` |
| GL_ARRAY_BUFFER | 34962 | `0x8892` |
| GL_ELEMENT_ARRAY_BUFFER | 34963 | `0x8893` |

---

## 4. commands.json — GL 命令流

命令流是一个数组，每个元素代表一个 GL 调用。Replayer 按顺序执行即可还原帧渲染。

```typescript
interface Command {
  eventId: number;           // 命令序号（从 1 开始，连续）
  name: string;              // GL 函数名（如 "drawElements", "uniform4fv"）
  args: SerializedValue[];   // 参数列表（已序列化）

  // ── 可选的附加上下文字段 ──────────────────────────────
  framebufferId?: number;   // 命令执行时的 FBO ID（draw 调用）
  programId?: number;       // 命令执行时的 program ID（uniform 调用）
  bufferId?: number;        // 命令执行时的 buffer ID（buffer 相关调用）
  textureId?: number;       // 命令执行时的 texture ID（texture 相关调用）
  target?: number;          // GLenum target（bindBuffer, bindTexture 等）
  index?: number;           // 索引绑定点（bindBufferRange, bindBufferBase）
  uniformName?: string;     // uniform 名称（uniform 调用）
  uniformValue?: any;        // uniform 值快照（取决于 CAPTURE_UNIFORM_VALUE_MODE）
}
```

### 4.1 命令分类

| 类别 | 命令名 | 说明 |
|---|---|---|
| **Draw** | `drawArrays`, `drawElements`, `drawArraysInstanced`, `drawElementsInstanced` | 实际绘制调用 |
| **State** | `viewport`, `depthMask`, `depthFunc`, `colorMask`, `blendFunc`, `blendEquation`, `cullFace`, `frontFace`, `clearColor`, `clearDepth`, `clearStencil`, `scissor`, `stencilFunc`, `stencilOp`, `stencilMask`, `blendColor`, `polygonOffset` | 状态设置 |
| **Resource bind** | `bindFramebuffer`, `useProgram`, `bindBuffer`, `bindTexture`, `activeTexture`, `bindVertexArray`, `bindBufferRange`, `bindBufferBase`, `uniformBlockBinding`, `bindSampler`, `bindVertexBuffer`, `vertexBindingDivisor`, `drawBuffers`, `readBuffer` | 资源绑定 |
| **Resource create** | `createBuffer`, `createTexture`, `createShader`, `createProgram`, `createFramebuffer`, `createVertexArray` | 对象创建 |
| **Resource data** | `bufferData`, `bufferSubData`, `texImage2D`, `texSubImage2D`, `texStorage2D`, `compressedTexImage2D`, `copyBufferSubData` | 数据上传 |
| **Uniform** | `uniform1f`..`uniform4f`, `uniform1i`..`uniform4i`, `uniform1fv`..`uniform4fv`, `uniform1iv`..`uniform4iv`, `uniformMatrix3fv`, `uniformMatrix4fv` | Uniform 设置 |
| **Vertex attrib** | `vertexAttribPointer`, `enableVertexAttribArray`, `disableVertexAttribArray`, `vertexAttribDivisor` | 顶点属性配置 |
| **Capability** | `enable`, `disable` | GL capability 开关 |
| **Misc** | `clear`, `pixelStorei`, `texParameteri`, `generateMipmap`, `attachShader`, `linkProgram`, `shaderSource` | 其他 |

### 4.2 args 中的序列化值类型

`args` 数组中的每个元素是 `SerializedValue`：

```typescript
type SerializedValue =
  | number                // 标量值
  | string                // 字符串值
  | boolean               // 布尔值
  | null                  // null
  | { type: "Buffer" | "Texture" | "Shader" | "Program" | "Framebuffer" | "Sampler"; id: number }  // GL 对象引用
  | { type: "Uint8Array" | "Float32Array" | ...; data: number[] }  // TypedArray 数据
  | { type: "ArrayBuffer"; data: number[] }                        // ArrayBuffer 数据
  | { type: "UploadRef"; uploadPath: string; byteLength: number }   // 上传数据引用
  | { type: "UploadSkipped"; byteLength: number; skippedReason: string }  // 上传跳过占位
  | SerializedValue[];     // 数组
```

### 4.3 上传数据引用

当 `CAPTURE_FRAME_UPLOADS_ENABLED=true` 时，`bufferData`/`texImage2D` 等命令的 args 中可能出现 `UploadRef`，指向 `uploads/upload_N.bin` 文件。Replayer 需要读取对应 bin 文件来还原数据。

当 `CAPTURE_FRAME_UPLOADS_ENABLED=false` 时，args 中出现 `UploadSkipped` 占位符，数据不可用。

---

## 5. shaders.json

```typescript
interface ShaderRecord {
  id: number;               // Shader ID（1-based）
  label: string;            // "Shader#N"
  shaderType: number;       // 35633=VERTEX_SHADER, 35632=FRAGMENT_SHADER
  source: string;           // 完整 GLSL ES 3.00 源码
}
```

**Replayer 消费方式：** 遍历列表，对每个 shader 调用 `gl.createShader` → `gl.shaderSource` → `gl.compileShader`。

---

## 6. programs.json

```typescript
interface ProgramRecord {
  id: number;                         // Program ID（1-based）
  label: string;                      // "Program#N"
  attachedShaderIds: number[];        // 关联的 Shader ID 列表（通常 2 个：VS + FS）
  linked: boolean;                    // 是否 link 成功
  uniformBlockBindings?: Record<string, number>;  // uniformBlockIndex → bindingPoint
}
```

**Replayer 消费方式：**
1. 先确保所有 attachedShaderIds 对应的 shader 已编译
2. `gl.createProgram` → `gl.attachShader` → `gl.linkProgram`
3. 如有 `uniformBlockBindings`，调用 `gl.uniformBlockBinding(program, blockIndex, bindingPoint)`

**交叉引用：** `attachedShaderIds` → `shaders.json` 中的 `id`；`uniformBlockBindings` 与 `state.json` 中的 `uniformBufferBindings` 对应。

---

## 7. framebuffers.json

```typescript
interface FramebufferRecord {
  id: number;                // Framebuffer ID（1-based，0=默认 FBO 不在此列表）
  label: string;             // "Framebuffer#N"
  attachments: {
    attachment: number;       // GLenum（GL_COLOR_ATTACHMENT0=0x8CE0, GL_DEPTH_ATTACHMENT=0x8D00 等）
    textureId: number;        // 绑定的 Texture ID（0=无绑定/已解绑）
    level: number;            // mipmap level（通常为 0）
    textarget: number;        // GL_TEXTURE_2D=0x0DE1 等
  }[];
}
```

**Replayer 消费方式：** `gl.createFramebuffer` → `gl.bindFramebuffer` → 对每个 attachment 调用 `gl.framebufferTexture2D`。

**注意：** `textureId: 0` 表示该 attachment 无绑定，跳过即可。

**交叉引用：** `textureId` → `textures/Texture#N.json` 中的 `id`。

---

## 8. vaos.json

```typescript
interface VAOSRecord {
  id: number;                // VAO ID（0=默认 VAO，由 capture 合成）
  label: string;             // "VAO#N" 或 "VAO#0 (default)"
  attribs: Record<number, {  // 属性索引 → 属性状态
    enabled: boolean;
    size: number;
    type: number;
    normalized: boolean;
    stride: number;
    offset: number;
    bufferId: number;        // VBO Buffer ID
  }>;
  elementArrayBufferId: number;  // EBO Buffer ID（0=无）
  deleted: boolean;
}
```

**Replayer 消费方式：**
1. `gl.createVertexArray`（id=0 时跳过，使用默认 VAO）
2. `gl.bindVertexArray(vao)`
3. 对每个 attrib 调用 `gl.bindBuffer(GL_ARRAY_BUFFER, bufferId)` → `gl.vertexAttribPointer` → `gl.enableVertexAttribArray`
4. `gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBufferId)`

**交叉引用：** `attribs[].bufferId` 和 `elementArrayBufferId` → `buffers/Buffer#N.json`。

---

## 9. buffers/ — 缓冲区数据

每个 buffer 由两个文件组成：

### Buffer#N.json — 元数据

```typescript
interface BufferRecord {
  id: number;                    // Buffer ID
  label: string;                 // "Buffer#N"
  target: number;                // GLenum target（GL_ARRAY_BUFFER, GL_UNIFORM_BUFFER 等）
  bufferType: "VBO" | "EBO" | "UBO" | "Other" | "";
  usage: number;                 // GLenum usage（GL_STATIC_DRAW=0x88E4 等）
  byteLength: number;            // GPU 上的逻辑大小
  capturedByteLength: number;     // 实际捕获的字节数
  hasBaselineBytes: boolean;     // 是否有 .bin 数据
  sourceKind: string;             // 捕获方式描述
  dataPath: string;              // "Buffer#N.bin"（相对路径）
}
```

### Buffer#N.bin — 二进制数据

原始 buffer 内容的 `Uint8Array` 二进制文件。`capturedByteLength` 可能小于 `byteLength`（受 `CAPTURE_MAX_RESOURCE_BASELINE_BYTES` 限制）。

**bufferType 语义：**
- `VBO`：顶点缓冲区（`GL_ARRAY_BUFFER`）
- `EBO`：索引缓冲区（`GL_ELEMENT_ARRAY_BUFFER`）
- `UBO`：Uniform 缓冲区（`GL_UNIFORM_BUFFER`，通过 `bindBufferRange`/`bindBufferBase` 绑定）
- `Other`：其他类型

**Replayer 消费方式：**
1. `gl.createBuffer` → `gl.bindBuffer(target, buffer)`
2. 读取 `Buffer#N.bin` 为 `ArrayBuffer`
3. `gl.bufferData(target, data, usage)`

**UBO 特殊处理：** UBO 需要通过 `gl.bindBufferRange(GL_UNIFORM_BUFFER, index, buffer, offset, size)` 绑定到索引绑定点。绑定点信息来自 `state.json` 中的 `uniformBufferBindings`。

---

## 10. textures/ — 纹理数据

### Texture#N.json — 元数据

```typescript
interface TextureRecord {
  id: number;                    // Texture ID（Screen 纹理用 999999）
  label: string;                 // "Texture#N" 或 "Screen"
  target: number;                // GLenum（GL_TEXTURE_2D=0x0DE1, GL_TEXTURE_CUBE_MAP=0x8513）
  width: number;                 // 纹理原始宽度
  height: number;                // 纹理原始高度
  depth: number;                 // 3D 纹理深度（通常为 0）
  internalFormat: number;        // GLenum 内部格式
  format: number;                // GLenum 格式
  pixelType: number;             // GLenum 像素类型
  sourceKind: string;            // 捕获方式/来源描述
  compressed: boolean;           // 是否压缩格式
  levels: number;                 // mipmap 层数
  parameters: Record<string, number>;  // texParameteri 参数
  deleted: boolean;
  mipsGenerated: boolean;
  captureMethod: "FBOReadPixels" | "DepthBlit" | "FBOReadPixelsCubeMap" | "DepthCubeBlit" | "";
  byteLength: number;             // 原始估算大小
  capturedWidth: number;          // 实际捕获宽度（降采样后）
  capturedHeight: number;         // 实际捕获高度（降采样后）
  capturedByteLength: number;     // 实际捕获字节数
  hasBaselineBytes: boolean;
  dataPath: string;              // "Texture#N.bin"
}
```

### Texture#N.bin — 像素数据

**降采样规则：** 颜色纹理和深度纹理在宽/高 > 256 时按 1/4 分辨率捕获（`capturedWidth = floor(width/4)`）。

**像素格式对应 captureMethod：**

| captureMethod | 格式 | bin 文件内容 |
|---|---|---|
| `FBOReadPixels` | RGBA | `capturedWidth × capturedHeight × 4` 字节 RGBA8 像素 |
| `DepthBlit` | DEPTH | `capturedWidth × capturedHeight × 4` 字节 RGBA8（depth 编码到 RGB 24-bit，A=1.0） |
| `FBOReadPixelsCubeMap` | RGBA CubeMap | `6 × capturedWidth × capturedHeight × 4` 字节（6 个 face 顺序拼接：+X, -X, +Y, -Y, +Z, -Z） |
| `DepthCubeBlit` | DEPTH CubeMap | `6 × capturedWidth × capturedHeight × 4` 字节（同上，6 face 拼接） |
| `""` (空) | 任意 | 无像素数据（可能 bin 为空或极小） |

**Replayer 消费方式：**
1. `gl.createTexture` → `gl.bindTexture(target, texture)`
2. 设置 parameters → `gl.texParameteri`
3. 根据 `target` 和格式调用 `gl.texImage2D` / `gl.texStorage2D`
4. 对 CubeMap：逐 face 调用 `gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, ...)`

**Screen 纹理（id=999999）：** 全帧截图，格式为 RGBA8，尺寸与 canvas 相同，不可用于 GL 回放，仅供 UI 预览。

---

## 11. ID 交叉引用图

```
manifest.json
  └── files.buffers[].id ──────┐
  └── files.textures[].id ─────┤
                               │
state.json                     │
  ├── currentProgramId ───────────→ programs.json[].id
  ├── currentFramebufferId ───────→ framebuffers.json[].id (0=默认)
  ├── readFramebufferId ──────────→ framebuffers.json[].id
  ├── currentTextures{unit→id} ───→ textures/Texture#N.json
  ├── currentArrayBufferId ────────→ buffers/Buffer#N.json
  ├── currentElementArrayBufferId─→ buffers/Buffer#N.json
  ├── uniformBufferBindings       │
  │     └── [].bufferId ───────────→ buffers/Buffer#N.json
  ├── attribs[].bufferId ─────────→ buffers/Buffer#N.json
  └── vertexBufferBindings        │
        └── [].bufferId ───────────→ buffers/Buffer#N.json

programs.json
  ├── attachedShaderIds[] ────────→ shaders.json[].id
  └── uniformBlockBindings ───────→ state.uniformBufferBindings (binding point 对应)

framebuffers.json
  └── attachments[].textureId ────→ textures/Texture#N.json (0=无)

vaos.json
  ├── attribs[].bufferId ─────────→ buffers/Buffer#N.json
  └── elementArrayBufferId ───────→ buffers/Buffer#N.json

commands.json
  ├── programId ──────────────────→ programs.json[].id
  ├── framebufferId ─────────────→ framebuffers.json[].id
  ├── bufferId ──────────────────→ buffers/Buffer#N.json
  └── textureId ─────────────────→ textures/Texture#N.json
```

**规则：**
- ID 从 1 开始（0 表示"无"或"默认"）
- 所有 ID 在同类型资源内唯一
- `state.json` 中的 ID 是命令流执行**之前**的绑定状态

---

## 12. 回放策略

### 12.1 完整回放（Standalone Replayer）

当 `replayerCompatibility.currentStandaloneReplayer = true` 时：

1. **初始化 GL 上下文**
2. **创建并编译着色器** ← `shaders.json`
3. **创建并链接程序** ← `programs.json` + `uniformBlockBindings`
4. **创建并上传缓冲区数据** ← `buffers/*.json` + `.bin`
5. **创建并上传纹理数据** ← `textures/*.json` + `.bin`
6. **创建帧缓冲对象** ← `framebuffers.json`
7. **创建 VAO** ← `vaos.json`
8. **还原帧起始状态** ← `state.json`（按 3.2 的顺序）
9. **执行命令流** ← `commands.json`（按 eventId 顺序）

### 12.2 预览回放（Inspector / Preview）

当只需要预览渲染结果时：
1. 读取 `state.json` 获取最终状态
2. 读取 `textures/Screen.json` + `Screen.bin` 显示截图
3. 读取 `manifest.json` 的 diagnostics 获取命令统计

### 12.3 资源缺失时的降级

| 缺失资源 | 降级策略 |
|---|---|
| Buffer .bin 文件不存在或为空 | 用零填充的 `ArrayBuffer` 代替（`byteLength` 已知） |
| Texture .bin 不存在 | 创建 1×1 黑色纹理占位 |
| `capturedByteLength < byteLength` | .bin 数据仅为部分内容，其余补零 |
| Shader 编译失败 | 跳过该 program 的绑定，使用 fallback shader |
| `uniformBlockBindings` 无数据 | 不调用 `gl.uniformBlockBinding`，使用默认绑定 |
| `UploadSkipped` 占位符 | 无法还原该命令的完整数据，跳过或用零填充 |

---

## 13. 版本兼容性

| 版本 | 变更 |
|---|---|
| **v2** | 新增 `uniformBufferBindings`, `samplerObjects`, `readFramebufferId`, `drawBuffers`, `readBuffer`, `stencilTestEnabled`, `stencilFront/Back`, `depthRangeNear/Far`, `blendEnabled`, `blendColor`, `polygonOffset*`, `lineWidth`, `multisampleEnabled`, `scissorTestEnabled`, `pixelPack/Unpack`, `pixelPackBuffer/UnpackBuffer`, `vertexBufferBindings`, `vertexBindingDivisors`；新增 stencil/blend/polygonOffset/sampler 命令 |
| **v1** | 原始格式，仅有 `currentProgramId`, `currentFramebufferId`, `currentTextures`, `attribs` 等基础字段 |

Replayer 应通过 `manifest.version` 判断版本，缺失字段使用默认值：
- 布尔型：`false`（`multisampleEnabled` 默认 `true`）
- 数字型：`0`
- 数组/对象：`{}` / `[]`
- `depthRangeNear/Far`：`0.0` / `1.0`
- `readBuffer`：`0x0405`（GL_BACK）
