# 逐命令重放器（GUI）

基于 `drawCommandsGLlib`、参照 RenderDoc 的 event-browser / 单步思路实现的图形化逐命令（逐 cmd）重放器。可像 RenderDoc 一样把一帧 capture **逐条命令**地前进/后退/跳转查看，并预览每条命令写入的渲染目标。

> 说明：未使用任何 RenderDoc 源码（其为 GPL，不适合并入本仓库），仅参照其功能设计独立实现。GUI 采用 Dear ImGui（GLFW + OpenGL3 后端），而非 `windows.h` 原生控件——对这类工具更合适，也避免大量 Win32 样板。

## 功能
- **命令浏览器（Commands）**：滚动列表，显示每条命令的序号 / eventId / 名称；当前命令高亮；点击任意命令跳转到"执行到该命令为止"的状态。
- **控制面板（Replay Controls）**：`|<` 回到开头、`<` 后退一步、`>` 前进一步、`>|` 跳到结尾、`Play` 自动播放、进度滑块。
- **渲染目标预览（Render Target）**：显示当前绑定的 draw FBO 的颜色内容（逐 pass 可见，比如先看阴影/场景 FBO 再看最终合成），并标注当前 FBO 句柄。
- **GL 错误日志（GL Errors）**：显示最近一步执行产生的逐命令 GL 错误（无错误时显示绿色 No GL errors）。

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
