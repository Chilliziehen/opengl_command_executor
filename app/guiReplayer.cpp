/**
 * @file guiReplayer.cpp
 * @brief RenderDoc-style per-command ("逐 cmd") GUI replayer.
 *
 * A Dear ImGui front-end over ReplayEngine. Lets you scrub a captured WebGL2
 * frame one command at a time:
 *   - Event/command browser (click to jump to any command)
 *   - Step forward/back, jump to start/end, play, slider
 *   - Render-target preview: shows the framebuffer the current command drew to
 *   - Per-step GL error log
 *
 * Controls: Left/Right = step, Home/End = first/last, Space = play/pause, Esc =
 * exit.
 */
#include <glad/gl.h>
#define GLAD_GL_IMPLEMENTATION
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "GLStateManager/GLStateManager.h"
#include "drawResources/Command.h"
#include "platform/NativeDialog.h"
#include "replayer/ReplayEngine.h"
#include "resourceManagement/ResourceAllocator.h"

namespace {

ReplayEngine g_engine;
GLFWwindow *g_window = nullptr;
int g_canvasW = 0, g_canvasH = 0;

// The capture's "default framebuffer" (FBO id 0) is redirected here — an
// app-owned offscreen target — so the replay NEVER draws into the GUI window.
GLuint g_defaultFbo = 0, g_defaultColorTex = 0, g_defaultDepthRbo = 0;

// Persistent preview render target: the current command's output is blitted
// here once (on change) and displayed every frame.
GLuint g_previewFbo = 0, g_previewTex = 0;
int g_previewW = 0, g_previewH = 0;

// The replay's GL state, captured after each step. Restored before the next
// step so the preview/ImGui work in between can't corrupt the replay.
GLStateSnapshot g_engineState;
bool g_haveEngineState = false;

// ---- load-on-demand state ----
bool g_loaded = false;
std::string g_capturePath;
std::string g_lastLoadError;

void createColorDepthFbo(GLuint &fbo, GLuint &colorTex, GLuint &depthRbo, int w,
                         int h) {
  if (colorTex == 0)
    glGenTextures(1, &colorTex);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (depthRbo == 0)
    glGenRenderbuffers(1, &depthRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
  if (fbo == 0)
    glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, depthRbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void createPreviewTarget(int w, int h) {
  g_previewW = w;
  g_previewH = h;
  if (g_previewTex == 0)
    glGenTextures(1, &g_previewTex);
  glBindTexture(GL_TEXTURE_2D, g_previewTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (g_previewFbo == 0)
    glGenFramebuffers(1, &g_previewFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, g_previewFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         g_previewTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Blit the framebuffer the current command drew to into the preview texture.
// Depth-only FBOs (e.g. shadow maps) have no colour buffer — the blit is
// skipped to avoid GL_INVALID_OPERATION and potential GPU-side stalls.
void updatePreview() {
  GLuint srcFbo = g_engine.currentDrawFramebuffer();

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_previewFbo);
  glViewport(0, 0, g_previewW, g_previewH);
  glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
  GLint colourAttachType = GL_NONE;
  if (srcFbo != 0) {
    glGetFramebufferAttachmentParameteriv(
        GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colourAttachType);
  } else {
    colourAttachType = GL_RENDERBUFFER; // default FB always has a back buffer
  }
  if (colourAttachType != GL_NONE) {
    glBlitFramebuffer(0, 0, g_canvasW, g_canvasH, 0, 0, g_previewW, g_previewH,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  while (glGetError() != GL_NO_ERROR) {}
}

// Forward declarations.
void doSeek(size_t target);
void doStepForward();
void doStepBackward();

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

  createColorDepthFbo(g_defaultFbo, g_defaultColorTex, g_defaultDepthRbo,
                      g_canvasW, g_canvasH);
  Command::setDefaultFramebuffer(g_defaultFbo);
  createPreviewTarget(g_canvasW, g_canvasH);
  doSeek(g_engine.commandCount()); // start showing the full frame
  return true;
}

// ---- engine-state isolation via glStateManager ----
void captureEngineState() {
  g_engineState = GLStateManager::CaptureCurrentState();
  g_haveEngineState = true;
}
void restoreEngineState() {
  if (g_haveEngineState)
    GLStateManager::RestoreState(g_engineState);
}

void doSeek(size_t target) {
  restoreEngineState();
  std::string err;
  g_engine.seekTo(target, err);
  if (!err.empty())
    std::cerr << "seek error: " << err << std::endl;
  captureEngineState();
  updatePreview();
}
void doStepForward() {
  restoreEngineState();
  g_engine.stepForward();
  captureEngineState();
  updatePreview();
}
void doStepBackward() {
  restoreEngineState();
  std::string err;
  g_engine.stepBackward(err);
  captureEngineState();
  updatePreview();
}

} // namespace

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

    if (ImGui::Button("Load Capture...")) {
      std::string chosen;
      if (platform::OpenFolderDialog(chosen))
        loadCapture(chosen);
    }
    ImGui::SameLine();

    if (!g_loaded) {
      if (g_lastLoadError.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0.5f, 1),
                           "No capture loaded.  Click \"Load Capture...\" to "
                           "select a capture directory.");
      } else {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Load failed: %s",
                           g_lastLoadError.c_str());
      }
    } else {
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

    // -- Left top: Event Browser --
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

    // ---- render to window ----
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
