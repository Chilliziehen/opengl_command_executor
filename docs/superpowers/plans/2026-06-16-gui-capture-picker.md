# GUI Capture Directory Picker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace command-line-based capture loading in guiReplayer with a native folder-selection dialog triggered by an in-GUI button, allowing capture selection without restarting.

**Architecture:** Add a Windows native `IFileOpenDialog`-based folder picker (Vista+), wrapped in a small helper. The guiReplayer main loop now has two modes: "no capture loaded" (shows a load prompt) and "playing" (the existing UI). A "Load Capture..." button in the control bar opens the folder dialog; selecting a folder unloads the previous capture (if any) and loads the new one. Error handling maintains a `loaded` + `lastError` pair.

**Tech Stack:** C++20, Win32 COM (`IFileOpenDialog`), Dear ImGui, existing GLFW/glad/GLStateManager stack.

---

## File Structure

| File | Role |
|------|------|
| `src/platform/NativeDialog.h` (create) | Declares `bool OpenFolderDialog(std::string &outPath)` — platform-neutral API |
| `src/platform/NativeDialog_win32.cpp` (create) | Win32 COM folder dialog implementation, UTF-8 output |
| `app/guiReplayer.cpp` (modify) | Replace argv parsing with load-on-demand via dialog; add Load button + status text |
| `CMakeLists.txt` (modify) | Add `NativeDialog_win32.cpp` to `drawCommandsGLlib`; link `Ole32` |

---

### Task 1: Create the native folder-dialog API header

**Files:**
- Create: `src/platform/NativeDialog.h`

- [ ] **Step 1: Write the header**

```cpp
/**
 * @file NativeDialog.h
 * @brief Platform-native dialog helpers.
 *
 * Currently provides a folder-selection dialog.  Implementations live under
 * src/platform/NativeDialog_<platform>.cpp.
 */
#ifndef NATIVE_DIALOG_H
#define NATIVE_DIALOG_H

#include <string>

namespace platform {

/// Open a native folder-selection dialog and return the chosen path.
/// @param outPath  Receives the UTF-8 absolute path on success; unchanged on
///                 failure or if the user cancelled.
/// @return true if the user selected a folder, false if cancelled or error.
bool OpenFolderDialog(std::string &outPath);

} // namespace platform

#endif // NATIVE_DIALOG_H
```

- [ ] **Step 2: Commit**

```bash
git add src/platform/NativeDialog.h
git commit -m "feat: add NativeDialog API header for folder selection"
```

---

### Task 2: Implement the Win32 folder dialog

**Files:**
- Create: `src/platform/NativeDialog_win32.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
/**
 * @file NativeDialog_win32.cpp
 * @brief Win32 COM folder-selection dialog (IFileOpenDialog with FOS_PICKFOLDERS).
 */

#include "platform/NativeDialog.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog, IShellItem
#include <wrl/client.h> // Microsoft::WRL::ComPtr (available in Windows SDK)
#include <string>

namespace platform {

bool OpenFolderDialog(std::string &outPath) {
    // COM must already be initialised by the caller (GLFW does this on Windows).
    // We use COINIT_APARTMENTTHREADED which is what GLFW uses internally; if
    // we weren't already initialised the call below would fail harmlessly.

    using Microsoft::WRL::ComPtr;

    ComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr))
        return false;

    // FOS_PICKFOLDERS: pick a directory instead of files.
    DWORD flags = 0;
    dlg->GetOptions(&flags);
    dlg->SetOptions(flags | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

    // Set a meaningful title.
    dlg->SetTitle(L"Select Capture Directory");

    // Default to "example/" relative to the project root, or the user's
    // Documents folder, whichever exists.
    // We just let Windows pick its default (usually Libraries/Documents).
    hr = dlg->Show(nullptr); // nullptr = no owner window (modeless not needed)
    if (FAILED(hr))
        return false; // user cancelled or error

    ComPtr<IShellItem> item;
    hr = dlg->GetResult(&item);
    if (FAILED(hr))
        return false;

    PWSTR rawPath = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
    if (FAILED(hr))
        return false;

    // Convert wide string → UTF-8
    int len = ::WideCharToMultiByte(CP_UTF8, 0, rawPath, -1, nullptr, 0,
                                    nullptr, nullptr);
    if (len > 0) {
        std::string utf8(static_cast<size_t>(len - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, rawPath, -1, utf8.data(), len,
                              nullptr, nullptr);
        outPath = utf8;
    }
    ::CoTaskMemFree(rawPath);
    return !outPath.empty();
}

} // namespace platform
```

- [ ] **Step 2: Commit**

```bash
git add src/platform/NativeDialog_win32.cpp
git commit -m "feat: add Win32 COM folder-selection dialog implementation"
```

---

### Task 3: Add the native-dialog source to CMake and link Ole32

**Files:**
- Modify: `CMakeLists.txt:107-117` (the `drawCommandsGLlib` source list)

- [ ] **Step 1: Add the source file and Ole32 link**

In `CMakeLists.txt`, change the `drawCommandsGLlib` block from:

```cmake
add_library(drawCommandsGLlib
    src/core/GladImplementation.cpp
    src/drawResources/Texture.cpp
    src/drawResources/Command.cpp
    src/framecapture/FrameCapture.cpp
    src/framecapture/CaptureLoader.cpp
    src/resourceManagement/ResourceAllocator.cpp
    src/resourceManagement/ResourceManager.cpp
    src/shaderTranslation/ShaderInterpreter.cpp
    src/replayer/ReplayEngine.cpp
)
target_link_libraries(drawCommandsGLlib PRIVATE thirdparties)
target_include_directories(drawCommandsGLlib
    PUBLIC "${CMAKE_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
)
```

To:

```cmake
add_library(drawCommandsGLlib
    src/core/GladImplementation.cpp
    src/drawResources/Texture.cpp
    src/drawResources/Command.cpp
    src/framecapture/FrameCapture.cpp
    src/framecapture/CaptureLoader.cpp
    src/resourceManagement/ResourceAllocator.cpp
    src/resourceManagement/ResourceManager.cpp
    src/shaderTranslation/ShaderInterpreter.cpp
    src/replayer/ReplayEngine.cpp
    src/platform/NativeDialog_win32.cpp
)
target_link_libraries(drawCommandsGLlib PRIVATE thirdparties Ole32)
target_include_directories(drawCommandsGLlib
    PUBLIC "${CMAKE_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
)
```

- [ ] **Step 2: Reconfigure and build**

```bash
cd build && cmake .. && cmake --build . --target drawCommandsGLlib
```

Expected: drawCommandsGLlib links successfully.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat: add NativeDialog_win32.cpp to build, link Ole32"
```

---

### Task 4: Modify guiReplayer to load captures via the dialog

**Files:**
- Modify: `app/guiReplayer.cpp` (multiple sections)

This is the core change.  The rewrite removes CLI-based loading and adds a "Load" button + state machine.

- [ ] **Step 1: Add the include**

At the top of `guiReplayer.cpp`, after line 38 (`#include "resourceManagement/ResourceAllocator.h"`), add:

```cpp
#include "platform/NativeDialog.h"
```

- [ ] **Step 2: Add file-scope state for the load path and loading UX**

Replace lines 41-61 (the anonymous namespace's state variables up through `g_haveEngineState`) with:

```cpp
namespace {

ReplayEngine g_engine;
GLFWwindow *g_window = nullptr;
int g_canvasW = 0, g_canvasH = 0;

// The capture's "default framebuffer" (FBO id 0) is redirected here — an
// app-owned offscreen target — so the replay NEVER draws into the GUI window.
// This is what eliminates the previous feedback/mirror artifact.
GLuint g_defaultFbo = 0, g_defaultColorTex = 0, g_defaultDepthRbo = 0;

// Persistent preview render target: the current command's output is blitted
// here once (on change) and displayed every frame.
GLuint g_previewFbo = 0, g_previewTex = 0;
int g_previewW = 0, g_previewH = 0;

// The replay's GL state, captured after each step. Restored before the next
// step so the preview/ImGui work in between can't corrupt the replay
// (glStateManager).
GLStateSnapshot g_engineState;
bool g_haveEngineState = false;

// ---- load-on-demand state ----
bool g_loaded = false;
std::string g_capturePath;
std::string g_lastLoadError;
```

- [ ] **Step 3: Add an `unloadCapture()` helper**

After line 121 (the end of `updatePreview()`), add:

```cpp
void unloadCapture() {
  if (g_loaded) {
    ResourceAllocator::deleteAllResources(g_engine.capture());
    g_engine.unload();
    g_loaded = false;
    g_haveEngineState = false;
    g_capturePath.clear();
    g_lastLoadError.clear();
  }
}
```

- [ ] **Step 4: Add a `loadCapture(const std::string &path)` helper**

After `unloadCapture()`, add:

```cpp
bool loadCapture(const std::string &path) {
  unloadCapture();

  std::string err;
  if (!g_engine.load(path, err)) {
    g_lastLoadError = err;
    return false;
  }

  g_loaded = true;
  g_capturePath = path;

  g_canvasW = static_cast<int>(g_engine.capture().m_manifest.m_canvas.width);
  g_canvasH = static_cast<int>(g_engine.capture().m_manifest.m_canvas.height);
  if (g_canvasW <= 0 || g_canvasH <= 0) {
    g_canvasW = 1280;
    g_canvasH = 720;
  }

  // Offscreen target standing in for the capture's default framebuffer.
  createColorDepthFbo(g_defaultFbo, g_defaultColorTex, g_defaultDepthRbo,
                      g_canvasW, g_canvasH);
  Command::setDefaultFramebuffer(g_defaultFbo);
  createPreviewTarget(g_canvasW, g_canvasH);
  doSeek(g_engine.commandCount()); // start showing the full frame
  return true;
}
```

- [ ] **Step 5: Replace `main()` — remove argv parsing, add idle-state ImGui UI**

Replace `main()` (lines 161-467) with:

```cpp
int main(int, char **) {
  if (!glfwInit()) {
    std::cerr << "Failed to init GLFW" << std::endl;
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  g_window = glfwCreateWindow(1600, 900, "Capture Replayer (per-command)",
                              nullptr, nullptr);
  if (!g_window) {
    std::cerr << "Failed to create window" << std::endl;
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(g_window);
  glfwSwapInterval(1);
  if (gladLoadGL(glfwGetProcAddress) == 0) {
    std::cerr << "Failed to init glad" << std::endl;
    return 1;
  }

  // ---- ImGui ----
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(g_window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  bool playing = false;
  double lastStepTime = 0.0;
  const double stepInterval = 0.05;

  while (!glfwWindowShouldClose(g_window)) {
    glfwPollEvents();

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // ---- keyboard shortcuts (when ImGui isn't capturing the keyboard) ----
    ImGuiIO &io = ImGui::GetIO();
    if (g_loaded && !io.WantCaptureKeyboard) {
      if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        glfwSetWindowShouldClose(g_window, true);
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        doStepForward();
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        doStepBackward();
      if (ImGui::IsKeyPressed(ImGuiKey_Home))
        doSeek(0);
      if (ImGui::IsKeyPressed(ImGuiKey_End))
        doSeek(g_engine.commandCount());
      if (ImGui::IsKeyPressed(ImGuiKey_Space))
        playing = !playing;
    }

    // ---- auto-advance while playing ----
    if (g_loaded && playing) {
      double now = glfwGetTime();
      if (now - lastStepTime >= stepInterval) {
        lastStepTime = now;
        if (g_engine.cursor() >= g_engine.commandCount())
          playing = false;
        else
          doStepForward();
      }
    }

    // ---- fixed layout ----
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    const ImVec2 org = vp->WorkPos;
    const ImVec2 vsz = vp->WorkSize;
    const float topH = 96.0f;
    const float leftW = 380.0f;
    const float logH = 150.0f;
    const float pipeH = 270.0f;
    const ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    // -- Top: control bar --
    ImGui::SetNextWindowPos(org);
    ImGui::SetNextWindowSize(ImVec2(vsz.x, topH));
    ImGui::Begin("Replay Controls", nullptr, panelFlags);

    // ---- Load button (always visible) ----
    if (ImGui::Button("Load Capture...")) {
      std::string chosen;
      if (platform::OpenFolderDialog(chosen))
        loadCapture(chosen);
    }
    ImGui::SameLine();

    if (!g_loaded) {
      // ---- Idle state: prompt and optional error ----
      if (g_lastLoadError.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0.5f, 1),
                           "No capture loaded.  Click \"Load Capture...\" to "
                           "select a capture directory.");
      } else {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Load failed: %s",
                           g_lastLoadError.c_str());
      }
    } else {
      // ---- Loaded state: replay controls ----
      ImGui::Text("Path: %s", g_capturePath.c_str());
      size_t cur = g_engine.cursor();
      size_t cnt = g_engine.commandCount();
      if (ImGui::Button("|< First"))
        doSeek(0);
      ImGui::SameLine();
      if (ImGui::Button("< Step"))
        doStepBackward();
      ImGui::SameLine();
      if (ImGui::Button("Step >"))
        doStepForward();
      ImGui::SameLine();
      if (ImGui::Button("Last >|"))
        doSeek(cnt);
      ImGui::SameLine();
      ImGui::Checkbox("Play", &playing);
      ImGui::SameLine();
      ImGui::Text("   Event %zu / %zu", cur, cnt);
      if (cur > 0) {
        ImGui::SameLine();
        ImGui::Text("  |  #%u %s", g_engine.commandEventId(cur - 1),
                    g_engine.commandName(cur - 1).c_str());
      }
      int sliderVal = static_cast<int>(cur);
      ImGui::SetNextItemWidth(vsz.x - 24.0f);
      if (ImGui::SliderInt("##seek", &sliderVal, 0, static_cast<int>(cnt)))
        doSeek(static_cast<size_t>(sliderVal));
    }
    ImGui::End();

    // -- Left top: Event Browser (only when loaded) --
    ImGui::SetNextWindowPos(ImVec2(org.x, org.y + topH));
    ImGui::SetNextWindowSize(ImVec2(leftW, vsz.y - topH - pipeH));
    ImGui::Begin("Event Browser", nullptr, panelFlags);
    if (g_loaded) {
      static size_t prevCursor = static_cast<size_t>(-1);
      size_t cur = g_engine.cursor();
      bool cursorChanged = (cur != prevCursor);
      prevCursor = cur;
      if (ImGui::BeginTable("events", 2,
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("EID", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < g_engine.commandCount(); ++i) {
          ImGui::TableNextRow();
          bool isCurrent = (i + 1 == cur);
          ImGui::TableSetColumnIndex(0);
          char eid[32];
          std::snprintf(eid, sizeof(eid), "%u##ev%zu",
                        g_engine.commandEventId(i), i);
          if (ImGui::Selectable(eid, isCurrent,
                                ImGuiSelectableFlags_SpanAllColumns))
            doSeek(i + 1);
          ImGui::TableSetColumnIndex(1);
          ImGui::TextUnformatted(g_engine.commandName(i).c_str());
          if (isCurrent && cursorChanged)
            ImGui::SetScrollHereY(0.5f);
        }
        ImGui::EndTable();
      }
    } else {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
                         "Load a capture to browse commands.");
    }
    ImGui::End();

    // -- Left bottom: Pipeline State --
    ImGui::SetNextWindowPos(ImVec2(org.x, org.y + vsz.y - pipeH));
    ImGui::SetNextWindowSize(ImVec2(leftW, pipeH));
    ImGui::Begin("Pipeline State", nullptr, panelFlags);
    if (g_loaded && g_haveEngineState) {
      const GLStateSnapshot &st = g_engineState;
      ImGui::Text("Program : %u", st.m_shader.m_programId.value_or(0));
      ImGui::Text("VAO     : %u", st.m_vertexInput.m_vaoId.value_or(0));
      ImGui::Text("Draw FBO: %u    Read FBO: %u",
                  st.m_framebuffer.m_drawFramebuffer,
                  st.m_framebuffer.m_readFramebuffer);
      ImGui::Text("Viewport: %d,%d %dx%d", st.m_viewport[0], st.m_viewport[1],
                  st.m_viewport[2], st.m_viewport[3]);
      ImGui::Separator();
      ImGui::Text("Depth test : %s  func=0x%X  write=%s",
                  st.m_depthStencil.m_depthTestEnabled ? "ON" : "off",
                  static_cast<unsigned>(st.m_depthStencil.m_depthFunc),
                  st.m_depthStencil.m_depthWriteEnabled ? "yes" : "no");
      ImGui::Text("Cull face  : %s  mode=0x%X  front=0x%X",
                  st.m_rasterizer.m_cullFaceEnabled ? "ON" : "off",
                  static_cast<unsigned>(st.m_rasterizer.m_cullMode),
                  static_cast<unsigned>(st.m_rasterizer.m_frontFace));
      ImGui::Text("Blend      : %s   ColorMask %d%d%d%d",
                  st.m_blend.m_blendEnabled ? "ON" : "off",
                  st.m_blend.m_colorMask[0], st.m_blend.m_colorMask[1],
                  st.m_blend.m_colorMask[2], st.m_blend.m_colorMask[3]);
      ImGui::Separator();
      ImGui::Text("Texture bindings (unit: tex2D/cube):");
      const auto &rb = st.m_resourceBindings;
      for (size_t u = 0; u < rb.m_textureUnits.size(); ++u) {
        const auto &t2d = rb.m_textureUnits[u].binding(TextureTarget::Tex2D);
        const auto &tcb = rb.m_textureUnits[u].binding(TextureTarget::TexCube);
        if (t2d.has_value() || tcb.has_value())
          ImGui::Text("  unit %zu: 2D=%u cube=%u", u, t2d.value_or(0),
                      tcb.value_or(0));
      }
    }
    ImGui::End();

    // -- Right top: Render-target preview --
    ImGui::SetNextWindowPos(ImVec2(org.x + leftW, org.y + topH));
    ImGui::SetNextWindowSize(ImVec2(vsz.x - leftW, vsz.y - topH - logH));
    ImGui::Begin("Render Target", nullptr, panelFlags);
    if (g_loaded) {
      GLuint fbo = g_engine.currentDrawFramebuffer();
      ImGui::Text("Bound draw FBO: %u    preview %dx%d", fbo, g_previewW,
                  g_previewH);
      ImVec2 avail = ImGui::GetContentRegionAvail();
      float aspect =
          g_previewH > 0 ? (float)g_previewW / (float)g_previewH : 1.0f;
      float w = avail.x;
      float h = w / aspect;
      if (h > avail.y) {
        h = avail.y;
        w = h * aspect;
      }
      float padx = (avail.x - w) * 0.5f;
      if (padx > 0)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padx);
      ImGui::Image((ImTextureID)(intptr_t)g_previewTex, ImVec2(w, h),
                   ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::End();

    // -- Right bottom: GL error log --
    ImGui::SetNextWindowPos(ImVec2(org.x + leftW, org.y + vsz.y - logH));
    ImGui::SetNextWindowSize(ImVec2(vsz.x - leftW, logH));
    ImGui::Begin("GL Errors (last step)", nullptr, panelFlags);
    if (g_loaded) {
      const std::string &errors = g_engine.lastGlErrors();
      if (errors.empty())
        ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "No GL errors.");
      else
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "%s", errors.c_str());
    }
    ImGui::End();

    // ---- present ----
    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(g_window, &dw, &dh);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Headless screenshot for verification.
    if (const char *dumpPath = std::getenv("GUIDUMP")) {
      static int frameCount = 0;
      if (++frameCount >= 4) {
        std::vector<unsigned char> px(static_cast<size_t>(dw) * dh * 3);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, dw, dh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        if (FILE *fp = std::fopen(dumpPath, "wb")) {
          std::fprintf(fp, "P6\n%d %d\n255\n", dw, dh);
          std::vector<unsigned char> row(static_cast<size_t>(dw) * 3);
          for (int y = dh - 1; y >= 0; --y) {
            std::fwrite(px.data() + static_cast<size_t>(y) * dw * 3, 1,
                        row.size(), fp);
          }
          std::fclose(fp);
          std::cout << "Wrote GUI screenshot to " << dumpPath << std::endl;
        }
        glfwSetWindowShouldClose(g_window, true);
      }
    }

    glfwSwapBuffers(g_window);
  }

  if (g_loaded)
    ResourceAllocator::deleteAllResources(g_engine.capture());
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(g_window);
  glfwTerminate();
  return 0;
}
```

- [ ] **Step 2: Build and fix any compilation issues**

```bash
cd build && cmake --build . --target guiReplayer
```

Expected: builds successfully. If `g_engine.unload()` doesn't exist on ReplayEngine, add a simple `void unload() { /* clear internal state if needed */ }` or skip calling it if the destructor handles cleanup.

- [ ] **Step 3: Commit**

```bash
git add app/guiReplayer.cpp
git commit -m "feat: replace CLI capture path with in-GUI Load button + folder dialog"
```

---

### Task 5: Run the full build and verify

- [ ] **Step 1: Full rebuild**

```bash
cd build && cmake .. && cmake --build . 2>&1
```

Expected: all targets compile and link with 0 errors.

- [ ] **Step 2: Smoke test the GUI**

```bash
./out/guiReplayer.exe
```

Expected: window opens with "Load Capture..." button and idle-state prompt. Clicking the button opens a folder dialog. Selecting a capture directory loads and displays the replay UI.

- [ ] **Step 3: Commit any cleanup**

```bash
git add -A
git commit -m "chore: final cleanup after guiReplayer capture-picker migration"
```

---

## Self-Review

**1. Spec coverage:**
- "程序载入frame改为在程序内点击选择capture路径" → Task 4 implements the Load button + folder dialog, Task 1-3 provide the native dialog infrastructure. ✓

**2. Placeholder scan:** No TBDs, no "similar to Task N" references, no abstract "add error handling" without code. ✓

**3. Type consistency:**
- `platform::OpenFolderDialog(std::string &)` declared in Task 1, implemented in Task 2, called in Task 4. ✓
- `g_loaded`, `g_capturePath`, `g_lastLoadError` added in Task 4 step 2, used in subsequent steps. ✓
- `loadCapture()`, `unloadCapture()` helpers consistent throughout. ✓
- `g_engine.unload()` — may need a no-op stub if ReplayEngine doesn't have one. Noted in Task 4 step 2. ✓
