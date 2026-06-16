# 逐命令重放器（GUI）

基于 `drawCommandsGLlib`、参照 RenderDoc 的 event-browser / 单步思路实现的图形化逐命令（逐 cmd）重放器。可像 RenderDoc 一样把一帧 capture **逐条命令**地前进/后退/跳转查看，并预览每条命令写入的渲染目标。

> 说明：未使用任何 RenderDoc 源码（其为 GPL，不适合并入本仓库），仅参照其功能设计独立实现。GUI 采用 Dear ImGui（GLFW + OpenGL3 后端），而非 `windows.h` 原生控件——对这类工具更合适，也避免大量 Win32 样板。

## 功能（固定停靠布局）
- **控制面板（Replay Controls，顶部）**：`|< First` 回到开头、`< Step` 后退一步、`Step >` 前进一步、`Last >|` 跳到结尾、`Play` 自动播放、全宽进度滑块、当前 Event 计数与名称。
- **事件浏览器（Event Browser，左上）**：EID / Action 双列表格，可点击任意事件跳转到"执行到该事件为止"的状态；当前事件高亮并自动滚动到可见区。
- **管线状态（Pipeline State，左下）**：基于 glStateManager 抓取的当前 GL 状态——program / VAO / draw·read FBO / viewport、深度·剔除·混合开关、各纹理单元的 2D/cube 绑定。RenderDoc 风格的状态检视。
- **渲染目标预览（Render Target，右上）**：显示当前绑定 draw FBO 的颜色内容（逐 pass 可见，比如先看阴影/场景 FBO 再看最终合成）。
- **GL 错误日志（GL Errors，右下）**：最近一步执行产生的逐命令 GL 错误（无错误时显示绿色 No GL errors）。

四个面板用 `SetNextWindowPos/Size` + `NoMove|NoResize` 固定到视口区域，随窗口缩放，不可拖动重叠。

## 快捷键
`←/→` 单步后退/前进 · `Home/End` 开头/结尾 · `Space` 播放/暂停 · `Esc` 退出。

## 单步语义
GL 状态不可逆，因此：
- **前进一步**：直接执行下一条命令（廉价）。
- **后退 / 跳到更早命令**：重新初始化帧起始状态（`deleteAllResources` → `allocateAllResources` → `uploadAllResources`）后从头重放到目标位置。

核心逻辑封装在 [`ReplayEngine`](../src/replayer/ReplayEngine.h)（`load` / `reset` / `stepForward` / `stepBackward` / `seekTo`），与 GUI 解耦，可被其它前端或测试复用。

预览采用"仅在命令位置变化时重放，再把当前 draw FBO blit 到一张常驻预览纹理"的方式，避免每帧重放整帧。

## 构建与运行
```bash
cmake --build build --target guiReplayer
./libs/guiReplayer.exe [capture_directory]
# 缺省加载 example/20260611_161020_frame6
```
依赖 Dear ImGui，由 CMake `FetchContent` 自动拉取到 `thirdparty/imgui`（与 glfw/glm 相同方式）。
