# TJRelic WX Capture — JSON Data Schema v2

> 基于 `tjrelic-wx-capture.js` 2.0.0 版本，对应 capture `format: "tjrelic.wx.capture"`, `version: 2`。

## 目录结构

```
<capture_name>/
  manifest.json            — 索引文件，描述所有子文件路径和全局元数据
  commands.json            — GL 命令流（JSON Array）
  state.json               — 帧首 GL 状态快照
  shaders.json             — Shader 源码
  programs.json            — Program 描述
  framebuffers.json        — FBO 描述符（可能为 []）
  vaos.json                — VAO 基线描述符（单个 JSON Array 文件）
  debug_log.json           — 调试日志（可忽略）
  runtime_logs.json        — 运行时诊断日志（可忽略）
  buffers/
    Buffer#N.json          — Buffer 描述符
    Buffer#N.bin           — Buffer 二进制数据（可选）
  textures/
    Texture#N.json         — Texture 描述符
    Texture#N.bin          — Texture 二进制像素数据（可选）
  uploads/
    upload_N.json          — 帧内 Upload 描述符
    upload_N.bin           — 帧内 Upload 二进制数据（可选）
```

---

## 1. manifest.json

### 结构

```json
{
  "format": "tjrelic.wx.capture",
  "version": 2,
  "captureModel": "gl-command-resource-partial",
  "generatedAt": "2026-06-12T06:58:13.134Z",
  "frameIndex": 0,
  "canvas": { "width": 430, "height": 932, "cssWidth": 1, "cssHeight": 1, "devicePixelRatio": 1 },
  "replayerCompatibility": {
    "currentStandaloneReplayer": true,
    "hasPreviewPayload": true,
    "reason": "partial resource bytes baseline only; large resources omitted"
  },
  "diagnostics": {
    "profile": "gl-command-sample-uniform-scalars-matrices-buffer-texture-payload",
    "commandLimit": 2048,
    "commandCount": 10,
    "uniformCommandCount": 2,
    "commandLimitReached": false,
    "resourceBaselineMode": "ubo",
    "maxResourceBaselineBytes": 4194304,
    "bufferUploadPayloadEnabled": true,
    "textureUploadPayloadEnabled": true,
    "uniformValueMode": "scalars-and-matrices"
  },
  "projectRoot": "",
  "sandboxRoot": "http://usr/tuanjierelic-captures",
  "files": {
    "commands": "commands.json",
    "state": "state.json",
    "shaders": "shaders.json",
    "programs": "programs.json",
    "framebuffers": "framebuffers.json",
    "buffers": [
      { "id": 1, "label": "Buffer#1", "path": "buffers/Buffer#1.json", "dataPath": "buffers/Buffer#1.bin" }
    ],
    "textures": [
      { "id": 9, "label": "Texture#9", "path": "textures/Texture#9.json", "dataPath": "textures/Texture#9.bin" }
    ],
    "uploads": [
      { "path": "uploads/upload_1.json", "dataPath": "uploads/upload_1.bin" }
    ],
    "debugLog": "debug_log.json",
    "runtimeLogs": "runtime_logs.json",
    "vaos": "vaos.json"
  }
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `format` | string | 固定值 `"tjrelic.wx.capture"` |
| `version` | integer | 当前为 2 |
| `captureModel` | string | `"gl-command-resource-partial"` |
| `frameIndex` | integer | 采集帧序号 |
| `canvas.width` | integer | WebGL canvas 宽（物理像素） |
| `canvas.height` | integer | WebGL canvas 高（物理像素） |
| `files.buffers[].id` | integer | Buffer 全局唯一 ID |
| `files.buffers[].label` | string | 人类可读标签 |
| `files.buffers[].path` | string | 描述符 json 文件相对路径 |
| `files.buffers[].dataPath` | string | 二进制数据相对路径，`""`表示无数据 |
| `files.textures[].id` | integer | Texture 全局唯一 ID（999999 = 屏幕截图） |
| `files.textures[].dataPath` | string | `""`表示只有元数据无像素数据 |
| `files.vaos` | string | VAO 描述文件相对路径 |

---

## 2. Buffer 描述符 (buffers/Buffer#N.json)

```json
{
  "id": 1,
  "label": "Buffer#1",
  "target": 34962,
  "bufferType": "VBO",
  "usage": 35044,
  "byteLength": 192,
  "capturedByteLength": 192,
  "hasBaselineBytes": true,
  "sourceKind": "CopyBufferSubData@4MBcap",
  "dataPath": "Buffer#1.bin"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | Buffer ID |
| `target` | integer | GL enum: 34962=ARRAY_BUFFER, 34963=ELEMENT_ARRAY_BUFFER, 35345=UNIFORM_BUFFER, 36662=COPY_READ_BUFFER, 36663=COPY_WRITE_BUFFER |
| `bufferType` | string | `"VBO"` / `"EBO"` / `"UBO"` / `"Other"` |
| `usage` | integer | GL enum: 35044=STATIC_DRAW, 35048=DYNAMIC_DRAW, 0=未知 |
| `byteLength` | integer | 分配大小（字节） |
| `capturedByteLength` | integer | 实际捕获大小（≤byteLength） |
| `hasBaselineBytes` | boolean | 是否在帧首基线采集到了数据 |
| `sourceKind` | string | 采集来源：`"CopyBufferSubData@4MBcap"` / `"DirectGetBufferSubData@EBO"` / `""` |
| `dataPath` | string | 同 manifest，冗余字段 |

**`.bin` 文件格式**：原始字节流，长度 = `capturedByteLength`。内容为 `glGetBufferSubData` 回读的 buffer 数据。

---

## 3. Texture 描述符 (textures/Texture#N.json)

```json
{
  "id": 9,
  "label": "Texture#9",
  "target": 34067,
  "width": 128,
  "height": 128,
  "depth": 0,
  "internalFormat": 32856,
  "format": 6408,
  "pixelType": 5121,
  "sourceKind": "",
  "compressed": false,
  "levels": 1,
  "parameters": {},
  "deleted": false,
  "mipsGenerated": false,
  "captureMethod": "FBOReadPixelsCubeMap",
  "byteLength": 24576,
  "capturedWidth": 32,
  "capturedHeight": 32,
  "capturedByteLength": 24576,
  "hasBaselineBytes": true,
  "dataPath": "Texture#9.bin"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | Texture ID |
| `target` | integer | GL enum: 3553=TEXTURE_2D, 34067=TEXTURE_CUBE_MAP |
| `width` | integer | 原始宽度（texels） |
| `height` | integer | 原始高度 |
| `internalFormat` | integer | GL enum, 例如 0x8058=RGBA8, 0x88F0=DEPTH24_STENCIL8 |
| `format` | integer | GL enum, 例如 0x1908=GL_RGBA, 0x1902=GL_DEPTH_COMPONENT |
| `pixelType` | integer | GL enum, 例如 0x1401=UNSIGNED_BYTE, 0x1405=UNSIGNED_INT |
| `parameters` | object | glTexParameteri 设置，key 为 GLenum 字符串, value 为 GLint |
| `captureMethod` | string | `"FBOReadPixels"` / `"DepthBlit"` / `"FBOReadPixelsCubeMap"` / `"DepthCubeBlit"` / `"depth-cubemap-hardcoded-fallback"` / `""` |
| `capturedWidth` | integer | 降采样后的捕获宽度（1/4 of width） |
| `capturedHeight` | integer | 降采样后的捕获高度 |
| `byteLength` | integer | 像素数据总大小 |
| `capturedByteLength` | integer | 实际捕获数据大小 |
| `hasBaselineBytes` | boolean | 是否有像素数据 |

### `.bin` 文件格式

- **TEXTURE_2D, captureMethod=FBOReadPixels/DepthBlit**：原始 RGBA 像素，`capturedWidth × capturedHeight × 4` 字节。DepthBlit 时 RGBA 编码为 depth→R8G8B8 (fract-subtract)。
- **TEXTURE_CUBE_MAP, captureMethod=FBOReadPixelsCubeMap**：6 面依次拼接，每面 `capturedWidth × capturedHeight × 4` 字节 RGBA。
- **TEXTURE_CUBE_MAP, captureMethod=DepthCubeBlit**：同上，但 RGBA 为 depth 编码。

### internalFormat 常用值

| 值 | 宏 | 说明 |
|----|-----|------|
| 0x8058 (32856) | GL_RGBA8 | 标准颜色纹理 |
| 0x88F0 (35056) | GL_DEPTH24_STENCIL8 | 深度+模板 |
| 0x81A6 (33158) | GL_DEPTH_COMPONENT24 | 24位深度 |
| 0x81A5 (33157) | GL_DEPTH_COMPONENT16 | 16位深度 |
| 0x8CAC (36012) | GL_DEPTH_COMPONENT32F | 32位浮点深度 |
| 0x8C3A (35898) | 微信私有格式 | 非标准 |

---

## 4. Shader 描述 (shaders.json)

```json
[
  {
    "id": 1,
    "label": "Shader#1",
    "shaderType": 35633,
    "source": "#version 300 es\nuniform mat4 u_MVP;\nin vec3 a_Position;\n..."
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | Shader ID |
| `shaderType` | integer | 35633=VERTEX_SHADER, 35632=FRAGMENT_SHADER |
| `source` | string | 完整 GLSL ES 3.0 源码 |

**注意**：WebGL shader 使用 `#version 300 es`、`precision` 声明、`in`/`out` varying。回放端需要翻译到桌面 GLSL `#version 330 core`。

---

## 5. Program 描述 (programs.json)

```json
[
  {
    "id": 1,
    "label": "Program#1",
    "attachedShaderIds": [1, 2],
    "linked": true
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | Program ID |
| `attachedShaderIds` | array[integer] | `[vertexShaderId, fragmentShaderId]` |
| `linked` | boolean | 是否链接成功 |

---

## 6. VAO 描述 (vaos.json)

```json
[
  {
    "id": 1,
    "label": "VAO#1",
    "attribs": {
      "0": {
        "enabled": true,
        "size": 3,
        "type": 5126,
        "normalized": false,
        "stride": 24,
        "offset": 0,
        "bufferId": 1,
        "name": "a_Position"
      },
      "1": {
        "enabled": true,
        "size": 3,
        "type": 5126,
        "normalized": false,
        "stride": 24,
        "offset": 12,
        "bufferId": 1,
        "name": "a_Color"
      }
    },
    "elementArrayBufferId": 2,
    "deleted": false
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | VAO ID（0 = 默认 VAO） |
| `attribs` | object | key 为属性索引字符串，value 为属性描述 |
| `attribs.N.enabled` | boolean | 是否启用 |
| `attribs.N.size` | integer | 分量数（1-4） |
| `attribs.N.type` | integer | GL enum（5126=GL_FLOAT） |
| `attribs.N.normalized` | boolean | 是否归一化 |
| `attribs.N.stride` | integer | 顶点步长（字节） |
| `attribs.N.offset` | integer | 顶点属性偏移（字节） |
| `attribs.N.bufferId` | integer | 绑定的 VBO ID |
| `attribs.N.name` | string | 属性名称（由 `glGetActiveAttrib` 获取，可能为空） |
| `elementArrayBufferId` | integer | 绑定的 EBO ID（0 = 无 EBO） |
| `deleted` | boolean | 是否已删除 |

---

## 7. State 状态快照 (state.json)

```json
{
  "currentProgramId": 1,
  "currentFramebufferId": 0,
  "currentActiveTexture": 0,
  "currentArrayBufferId": 1,
  "currentElementArrayBufferId": 2,
  "currentVertexArrayId": 1,
  "currentTextures": { "0": 2 },
  "currentSamplers": {},
  "attribs": {
    "0": {
      "enabled": true, "size": 3, "type": 5126, "normalized": false,
      "stride": 24, "offset": 0, "bufferId": 1, "name": "a_Position"
    }
  },
  "viewport": [0, 0, 430, 932],
  "capabilities": { "2929": true },
  "clearColor": [0.5, 0.82, 0.5, 1.0],
  "clearDepth": 1.0,
  "clearStencil": 0,
  "colorMask": [true, true, true, true],
  "depthMask": true,
  "depthFunc": 513,
  "blendFunc": { "srcRGB": 1, "dstRGB": 0, "srcAlpha": 1, "dstAlpha": 0 },
  "blendEquation": { "rgb": 32774, "alpha": 32774 },
  "cullFaceMode": 1029,
  "frontFace": 2305,
  "scissorBox": [0, 0, 0, 0],
  "pixelStore": {}
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `currentProgramId` | integer | 当前激活的 Program ID |
| `currentFramebufferId` | integer | 当前绑定的 FBO ID（0=默认帧缓冲） |
| `currentTextures` | object | key=纹理单元(0..N), value=Texture ID |
| `currentSamplers` | object | Program ID → { uniformName → textureUnit } |
| `attribs` | object | 全局顶点属性状态（同 VAO attribs 结构） |
| `viewport` | [x, y, w, h] | 视口矩形 |
| `capabilities` | object | key=GLenum字符串，value=boolean. 例如 `"2929": true`=GL_DEPTH_TEST |
| `clearColor` | [r, g, b, a] | 清除颜色（0.0-1.0） |
| `clearDepth` | float | 清除深度 |
| `clearStencil` | integer | 清除模板值 |
| `colorMask` | [r, g, b, a] | 颜色写入掩码 |
| `depthMask` | boolean | 深度写入开关 |
| `depthFunc` | integer | GL enum, 513=GL_LESS |
| `blendFunc` | object | 混合函数，src/dst × RGB/Alpha |
| `blendEquation` | object | 混合方程，rgb/alpha 分量 |
| `cullFaceMode` | integer | 1029=GL_BACK |
| `frontFace` | integer | 2305=GL_CW |
| `stencilFunc` | object | `{ func, ref, mask }` 或 `{ front: { func, ref, mask }, back: {...} }` ⚠️ 未捕获 |
| `stencilOp` | object | `{ sfail, dpfail, dppass }` 或 Separate 变体 ⚠️ 未捕获 |
| `stencilMask` | integer | 模板写掩码 ⚠️ 未捕获 |

---

## 8. 命令流 (commands.json)

### Command 基类

每个 command 是一个 JSON Object，最小字段：

```json
{
  "eventId": 1,
  "name": "<command_name>",
  "args": [...]
}
```

### 完整命令列表

#### viewport

```json
{ "eventId": 1, "name": "viewport", "args": [0, 0, 430, 932] }
```
args = [x, y, width, height]

#### enable / disable

```json
{ "eventId": 2, "name": "enable", "args": [2929] }
```
args = [capability_enum]. 2929=GL_DEPTH_TEST.

#### clearColor

```json
{ "eventId": 3, "name": "clearColor", "args": [0.5, 0.82, 0.5, 1.0] }
```
args = [r, g, b, a]

#### clear

```json
{ "eventId": 4, "name": "clear", "args": [16640], "mask": 16640, "framebufferId": 0 }
```
args = [bitmask]. Extra: `mask`, `framebufferId`.

#### useProgram

```json
{ "eventId": 5, "name": "useProgram", "args": [], "programId": 1 }
```
Extra: `programId`.

#### uniform4fv

```json
{
  "eventId": 6,
  "name": "uniform4fv",
  "args": [{ "type": "Float32Array", "data": [1.0, 0.0, 0.0, 0.0] }],
  "programId": 1,
  "uniformName": "u_MVP[0]",
  "valueOmitted": false,
  "valueOmittedReason": "",
  "_snapshot": true
}
```
Args[0] = `{ type: "Float32Array", data: [float...] }`. Extra: `programId`, `uniformName`, `valueOmitted`, `_snapshot`.

#### uniformMatrix4fv

```json
{
  "eventId": 8,
  "name": "uniformMatrix4fv",
  "args": [
    false,
    { "type": "Float32Array", "data": [m00, m01, ..., m33] }
  ],
  "programId": 1,
  "uniformName": "u_MVP",
  "valueOmitted": false
}
```
Args[0] = transpose boolean. Args[1] = Float32Array. Extra 同 uniform4fv.

#### uniform1i (sampler binding)

```json
{
  "eventId": 166,
  "name": "uniform1i",
  "args": [0],
  "programId": 2,
  "uniformName": "tex",
  "valueOmitted": false
}
```
args = [texture_unit]. Extra 同 uniform4fv.

#### bindVertexArray

```json
{ "eventId": 7, "name": "bindVertexArray", "args": [], "vertexArrayId": 1 }
```
Extra: `vertexArrayId`.

#### drawElements

```json
{ "eventId": 9, "name": "drawElements", "args": [4, 36, 5123, 0], "framebufferId": 0 }
```
args = [mode, count, type, offset]. mode: 4=GL_TRIANGLES. type: 5123=GL_UNSIGNED_SHORT. Extra: `framebufferId`.

#### drawArrays

```json
{ "eventId": 148, "name": "drawArrays", "args": [4, 0, 5040], "framebufferId": 3 }
```
args = [mode, first, count]. Extra: `framebufferId`.

#### bindFramebuffer

```json
{ "eventId": 10, "name": "bindFramebuffer", "args": [36160], "target": 36160, "framebufferId": 0 }
```
args = [target]. 36160=GL_FRAMEBUFFER. Extra: `target`, `framebufferId`.

#### bindBuffer

```json
{ "eventId": 22, "name": "bindBuffer", "args": [34962, 61], "bufferId": 61, "target": 34962 }
```
args = [target, bufferId]. Extra: `bufferId`, `target`.

#### bindBufferBase

```json
{ "eventId": 20, "name": "bindBufferBase", "args": [35345, 0, 69], "bufferId": 69, "target": 35345, "index": 0 }
```
args = [target, index, bufferId]. target=35345=GL_UNIFORM_BUFFER. Extra: `bufferId`, `target`, `index`.

#### bindBufferRange

```json
{ "eventId": 21, "name": "bindBufferRange", "args": [35345, 0, 60, 0, 720], "bufferId": 60, "target": 35345, "index": 0 }
```
args = [target, index, bufferId, offset, size]. Extra: `bufferId`, `target`, `index`.

#### bindTexture

```json
{ "eventId": 60, "name": "bindTexture", "args": [34067], "target": 34067, "textureId": 9, "unit": 0 }
```
args = [target]. Extra: `target`, `textureId`, `unit`.

#### activeTexture

```json
{ "eventId": 61, "name": "activeTexture", "args": [33984], "unit": 0 }
```
args = [texture_enum]. 33984=GL_TEXTURE0. Extra: `unit`.

#### vertexAttribPointer

```json
{
  "eventId": 23, "name": "vertexAttribPointer",
  "args": [0, 3, 5126, false, 56, 0],
  "bufferId": 61, "attribName": "in_POSITION0"
}
```
args = [index, size, type, normalized, stride, offset]. Extra: `bufferId`, `attribName`.

#### enableVertexAttribArray / disableVertexAttribArray

```json
{ "eventId": 24, "name": "enableVertexAttribArray", "args": [1] }
```
args = [index].

#### createVertexArray / deleteVertexArray / bindVertexArray

```json
{ "eventId": 1, "name": "createVertexArray", "args": [], "vertexArrayId": 1 }
{ "eventId": 2, "name": "deleteVertexArray", "args": [], "vertexArrayId": 1 }
```
Extra: `vertexArrayId`.

#### vertexAttribDivisor

```json
{ "eventId": 1, "name": "vertexAttribDivisor", "args": [1, 1] }
```
args = [index, divisor].

#### createBuffer / deleteBuffer

```json
{ "eventId": 1, "name": "createBuffer", "args": [], "bufferId": 1 }
{ "eventId": 2, "name": "deleteBuffer", "args": [], "bufferId": 1 }
```
Extra: `bufferId`.

#### bufferData / bufferSubData

```json
{
  "eventId": 13, "name": "bufferSubData",
  "args": [36663, 0, { "type": "UploadRef", "uploadPath": "uploads/upload_1.bin", "byteLength": 268435456 }],
  "bufferId": 67, "target": 36663
}
```
args[0] = target, args[1] = offset, args[2] = 数据或 UploadRef. Extra: `bufferId`, `target`.

UploadRef 是帧内 upload 的大数据替代：

```json
{ "type": "UploadRef", "uploadPath": "uploads/upload_N.bin", "byteLength": 268435456 }
```

#### texImage2D / texStorage2D / texSubImage2D / compressedTexImage2D

```json
{
  "eventId": 1, "name": "texImage2D",
  "args": [3553, 0, 32856, 128, 128, 0, 6408, 5121, { "type": "UploadRef", ... }]
}
```
args 同 GL 函数参数，最后一个可能是 UploadRef。

#### generateMipmap / copyTexImage2D

```json
{ "eventId": 1, "name": "generateMipmap", "args": [3553], "textureId": 5 }
```
Extra: `textureId`, `unit`.

#### shaderSource / attachShader / linkProgram / compileShader

```json
{ "eventId": 1, "name": "shaderSource", "args": [1, "#version 300 es..."], "shaderId": 1 }
{ "eventId": 2, "name": "attachShader", "args": [1, 1], "programId": 1, "shaderId": 1 }
{ "eventId": 3, "name": "linkProgram", "args": [1], "programId": 1 }
```
Extra: `shaderId`, `programId`.

#### createTexture / deleteTexture

```json
{ "eventId": 1, "name": "createTexture", "args": [], "textureId": 1 }
```
Extra: `textureId`.

#### createFramebuffer / deleteFramebuffer / bindFramebuffer

```json
{ "eventId": 1, "name": "createFramebuffer", "args": [], "framebufferId": 1 }
```
Extra: `framebufferId`.

#### framebufferTexture2D

```json
{
  "eventId": 1, "name": "framebufferTexture2D",
  "args": [36160, 36064, 3553, 2, 0],
  "framebufferId": 1, "attachment": 36064, "textureId": 2
}
```
Extra: `framebufferId`, `attachment`, `textureId`.

#### scissor / blendFunc / blendEquation / cullFace / frontFace / depthFunc / depthMask / colorMask / lineWidth

基本 GL 状态设置，args 直接对应 GL 函数参数。

#### stencilFunc / stencilFuncSeparate / stencilOp / stencilOpSeparate / stencilMask / stencilMaskSeparate

```json
{ "eventId": 1, "name": "stencilFunc", "args": [519, 0, 255] }
{ "eventId": 1, "name": "stencilFuncSeparate", "args": [1028, 519, 0, 255] }
{ "eventId": 1, "name": "stencilOp", "args": [7680, 7680, 7681] }
{ "eventId": 1, "name": "stencilMask", "args": [255] }
```
⚠️ **当前未捕获**——Unity 使用 stencil 做阴影渲染和 light culling。缺少会导致回放中 stencil 状态不一致。

#### blendColor

```json
{ "eventId": 1, "name": "blendColor", "args": [1.0, 1.0, 1.0, 1.0] }
```
args = [r, g, b, a]. 用于 `GL_BLEND_COLOR` 混合模式。⚠️ **当前未捕获**。

#### drawBuffers (MRT)

```json
{ "eventId": 1, "name": "drawBuffers", "args": [36064] }
```
args = [color_attachment, ...]. WebGL2 多渲染目标。⚠️ **当前未捕获**。

#### blitFramebuffer

```json
{ "eventId": 1, "name": "blitFramebuffer", "args": [0, 0, 512, 512, 0, 0, 512, 512, 16640, 9728] }
```
args = [srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter]. ⚠️ **当前未捕获**——Unity WebGL 用它做纹理复制和 MSAA resolve。

#### readPixels

```json
{ "eventId": 1, "name": "readPixels", "args": [0, 0, 512, 512, 6408, 5121] }
```
args = [x, y, width, height, format, type]. ⚠️ **当前未捕获**。

#### framebufferRenderbuffer / renderbufferStorage / createRenderbuffer

```json
{ "eventId": 1, "name": "createRenderbuffer", "args": [], "renderbufferId": 1 }
{ "eventId": 1, "name": "renderbufferStorage", "args": [36161, 33189, 512, 512], "renderbufferId": 1 }
{ "eventId": 1, "name": "framebufferRenderbuffer", "args": [36160, 36096, 36161, 1], "framebufferId": 1, "attachment": 36096, "renderbufferId": 1 }
```
RBO args = [target, internalFormat, width, height]. ⚠️ **当前未捕获**——Unity 常用 RBO 做深度/模板附件。

#### vertexAttribIPointer

```json
{ "eventId": 1, "name": "vertexAttribIPointer", "args": [0, 4, 5124, 0, 0], "bufferId": 1 }
```
WebGL2 整数顶点属性指针。args = [index, size, type, stride, offset]. ⚠️ **当前未捕获**。

#### deleteShader / deleteProgram / deleteFramebuffer

```json
{ "eventId": 1, "name": "deleteShader", "args": [], "shaderId": 1 }
{ "eventId": 1, "name": "deleteProgram", "args": [], "programId": 1 }
```
⚠️ **当前未捕获**——已捕获 deleteBuffer/deleteTexture/deleteVertexArray，但 Shader/Program/Framebuffer 的 delete 遗漏。

---

## 9. Upload 描述符 (uploads/upload_N.json)

```json
{
  "type": "Upload",
  "path": "uploads/upload_N.bin",
  "byteLength": 268435456
}
```

`.bin` 文件为帧内 `glBufferSubData` / `glTexImage2D` 的大数据替代——当数据过大不适合内联 JSON 时，写入此文件并通过 `UploadRef` 在命令中引用。

---

## 10. Framebuffer 描述符 (framebuffers.json)

```json
[
  {
    "id": 2,
    "label": "Framebuffer#2",
    "attachments": [
      { "attachment": 36096, "textureId": 4, "level": 0, "textarget": 3553 },
      { "attachment": 36128, "textureId": 0, "level": 0, "textarget": 3553 }
    ]
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | integer | FBO ID |
| `attachments[].attachment` | integer | 0x8CE0=COLOR_ATTACHMENT0, 0x8D00=DEPTH_ATTACHMENT, 0x821A=DEPTH_STENCIL_ATTACHMENT |
| `attachments[].textureId` | integer | Texture ID（0=renderbuffer） |
| `attachments[].level` | integer | mip level |
| `attachments[].textarget` | integer | 3553=TEXTURE_2D |

**可能为空数组 `[]`**——简单场景（如 demo）没有自定义 FBO。

---

## 附录：GL 常量速查

| 十进制 | 十六进制 | 宏 |
|--------|---------|-----|
| 4 | 0x0004 | GL_TRIANGLES |
| 5123 | 0x1403 | GL_UNSIGNED_SHORT |
| 5126 | 0x1406 | GL_FLOAT |
| 2929 | 0x0B71 | GL_DEPTH_TEST |
| 513 | 0x0201 | GL_LESS |
| 1029 | 0x0405 | GL_BACK |
| 2305 | 0x0901 | GL_CW |
| 3553 | 0x0DE1 | GL_TEXTURE_2D |
| 34067 | 0x8513 | GL_TEXTURE_CUBE_MAP |
| 34962 | 0x8892 | GL_ARRAY_BUFFER |
| 34963 | 0x8893 | GL_ELEMENT_ARRAY_BUFFER |
| 35345 | 0x8A11 | GL_UNIFORM_BUFFER |
| 35632 | 0x8B30 | GL_FRAGMENT_SHADER |
| 35633 | 0x8B31 | GL_VERTEX_SHADER |
| 36160 | 0x8D40 | GL_FRAMEBUFFER |
| 36662 | 0x8F36 | GL_COPY_READ_BUFFER |
| 36663 | 0x8F37 | GL_COPY_WRITE_BUFFER |
| 36064 | 0x8CE0 | GL_COLOR_ATTACHMENT0 |
| 36096 | 0x8D00 | GL_DEPTH_ATTACHMENT |
| 33306 | 0x821A | GL_DEPTH_STENCIL_ATTACHMENT |
| 32856 | 0x8058 | GL_RGBA8 |
| 35056 | 0x88F0 | GL_DEPTH24_STENCIL8 |
| 33158 | 0x81A6 | GL_DEPTH_COMPONENT24 |
| 6408 | 0x1908 | GL_RGBA |

---

## 11. 捕获覆盖缺口（Hook Coverage Gaps）

以下 GL 函数**尚未被捕获脚本 hook**，回放端遇到这些命令时需做 fallback 处理或忽略。

### 高影响（影响 Unity 渲染正确性）

| 函数 | 说明 |
|------|------|
| `stencilFunc` / `stencilFuncSeparate` | Unity 阴影、光裁剪依赖模板测试 |
| `stencilOp` / `stencilOpSeparate` | 模板操作（替换/增减/翻转） |
| `stencilMask` / `stencilMaskSeparate` | 模板写掩码 |
| `blendColor` | `GL_BLEND_COLOR` 模式的常量颜色 |
| `framebufferRenderbuffer` / `renderbufferStorage` / `createRenderbuffer` | Unity 常用 RBO 做深度/模板附件 |
| `blitFramebuffer` | Unity WebGL 用它做纹理复制和 MSAA resolve |

### 中影响

| 函数 | 说明 |
|------|------|
| `drawBuffers` | WebGL2 MRT（多渲染目标） |
| `readPixels` | Unity 某些 GPGPU 计算或截屏功能 |
| `vertexAttribIPointer` | WebGL2 整数顶点属性 |

### 低影响

| 函数 | 说明 |
|------|------|
| `deleteShader` / `deleteProgram` / `deleteFramebuffer` | 资源删除未被追踪（Buffer/Texture/VAO 已覆盖） |
| `texSubImage3D` / `compressedTexImage3D` / `compressedTexSubImage3D` | 3D/数组纹理的部分更新 |

### 已知数据格式问题

| 问题 | 说明 |
|------|------|
| `uploadPlaceholder` 使用 `type: 'UploadSkipped'` | 回放端 `ReplaySession.cpp` 只识别 `type: 'UploadRef'`，Skipped 被静默丢弃 |
| `bufferData` hook 强制设 `record.bytes = null` | 禁用 buffer 资源捕获，即使 baseline mode 需要数据也不会保存 |
| 5121 | 0x1401 | GL_UNSIGNED_BYTE |
