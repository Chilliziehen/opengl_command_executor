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
 * Controls: Left/Right = step, Home/End = first/last, Space = play/pause, Esc = exit.
 *
 * Usage: guiReplayer [capture_directory]
 *
 * (Implements the RenderDoc event-browser/stepping concept independently — no
 *  RenderDoc code is used; it is GPL and unsuitable to vendor here.)
 */
#include <glad/gl.h>
#define GLAD_GL_IMPLEMENTATION
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

#include "replayer/ReplayEngine.h"
#include "resourceManagement/ResourceAllocator.h"

namespace {

ReplayEngine g_engine;

// Persistent preview render target (the currently-selected command's output is
// blitted here once, then displayed every frame — so we replay only on change).
GLuint g_previewFbo = 0;
GLuint g_previewTex = 0;
int    g_previewW = 0, g_previewH = 0;

GLFWwindow *g_window = nullptr;

void createPreviewTarget(int w, int h) {
    g_previewW = w;
    g_previewH = h;
    if (g_previewTex == 0) glGenTextures(1, &g_previewTex);
    glBindTexture(GL_TEXTURE_2D, g_previewTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (g_previewFbo == 0) glGenFramebuffers(1, &g_previewFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_previewFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_previewTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Blit the framebuffer the engine last drew to into the preview texture.
void updatePreview() {
    GLuint srcFbo = g_engine.currentDrawFramebuffer();

    // Source size: window framebuffer for the default FB, else the captured
    // canvas (off-screen color targets are canvas-sized).
    int srcW, srcH;
    if (srcFbo == 0) {
        glfwGetFramebufferSize(g_window, &srcW, &srcH);
    } else {
        srcW = static_cast<int>(g_engine.capture().m_manifest.m_canvas.width);
        srcH = static_cast<int>(g_engine.capture().m_manifest.m_canvas.height);
        if (srcW <= 0 || srcH <= 0) { srcW = g_previewW; srcH = g_previewH; }
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_previewFbo);
    glViewport(0, 0, g_previewW, g_previewH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    // Depth-only render targets have no color attachment 0; the blit is then a
    // no-op (the clear color marks "no preview").
    glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, g_previewW, g_previewH,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    while (glGetError() != GL_NO_ERROR) {} // ignore blit errors (e.g. no color)
}

// Run the engine to `target` commands executed, then refresh the preview.
void seek(size_t target) {
    std::string err;
    g_engine.seekTo(target, err);
    if (!err.empty())
        std::cerr << "seek error: " << err << std::endl;
    updatePreview();
}

} // namespace

int main(int argc, char *argv[]) {
    std::string capturePath =
        (argc > 1) ? argv[1]
                   : std::string(PROJECT_ROOT_DIR) + "/example/20260611_161020_frame6";

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(1600, 900, "Capture Replayer (per-command)", nullptr, nullptr);
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
    ImGui::GetIO().IniFilename = nullptr; // don't litter an imgui.ini
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ---- load capture ----
    std::string err;
    bool loaded = g_engine.load(capturePath, err);
    if (!loaded) {
        std::cerr << "Load FAILED: " << err << std::endl;
    } else {
        int cw = static_cast<int>(g_engine.capture().m_manifest.m_canvas.width);
        int ch = static_cast<int>(g_engine.capture().m_manifest.m_canvas.height);
        if (cw <= 0 || ch <= 0) { cw = 1280; ch = 720; }
        createPreviewTarget(cw, ch);
        seek(g_engine.commandCount()); // start showing the full frame
    }

    bool playing = false;
    double lastStepTime = 0.0;
    const double stepInterval = 0.05; // seconds per command while playing

    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // ---- keyboard shortcuts (when ImGui isn't capturing the keyboard) ----
        ImGuiIO &io = ImGui::GetIO();
        if (loaded && !io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                glfwSetWindowShouldClose(g_window, true);
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { g_engine.stepForward(); updatePreview(); }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  { std::string e; g_engine.stepBackward(e); updatePreview(); }
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) seek(0);
            if (ImGui::IsKeyPressed(ImGuiKey_End))  seek(g_engine.commandCount());
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) playing = !playing;
        }

        // ---- auto-advance while playing ----
        if (loaded && playing) {
            double now = glfwGetTime();
            if (now - lastStepTime >= stepInterval) {
                lastStepTime = now;
                if (!g_engine.stepForward()) playing = false; // stop at end
                else updatePreview();
            }
        }

        // ---- Controls ----
        ImGui::Begin("Replay Controls");
        if (!loaded) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Load failed:");
            ImGui::TextWrapped("%s", err.c_str());
        } else {
            size_t cur = g_engine.cursor();
            size_t cnt = g_engine.commandCount();
            ImGui::Text("Capture: %s", g_engine.captureDirectory().c_str());
            ImGui::Text("Command %zu / %zu", cur, cnt);

            if (ImGui::Button("|<")) seek(0);
            ImGui::SameLine();
            if (ImGui::Button("<")) { std::string e; g_engine.stepBackward(e); updatePreview(); }
            ImGui::SameLine();
            if (ImGui::Button(">")) { g_engine.stepForward(); updatePreview(); }
            ImGui::SameLine();
            if (ImGui::Button(">|")) seek(cnt);
            ImGui::SameLine();
            ImGui::Checkbox("Play", &playing);

            int sliderVal = static_cast<int>(cur);
            if (ImGui::SliderInt("##seek", &sliderVal, 0, static_cast<int>(cnt)))
                seek(static_cast<size_t>(sliderVal));

            if (cur > 0) {
                ImGui::Separator();
                ImGui::Text("Last: #%u %s",
                            g_engine.commandEventId(cur - 1),
                            g_engine.commandName(cur - 1).c_str());
            }
        }
        ImGui::End();

        // ---- Command browser ----
        if (loaded) {
            ImGui::Begin("Commands");
            size_t cur = g_engine.cursor();
            ImGui::BeginChild("cmdlist");
            for (size_t i = 0; i < g_engine.commandCount(); ++i) {
                bool isCurrent = (i + 1 == cur);
                char label[160];
                std::snprintf(label, sizeof(label), "%4zu  #%u  %s", i,
                              g_engine.commandEventId(i),
                              g_engine.commandName(i).c_str());
                if (ImGui::Selectable(label, isCurrent))
                    seek(i + 1); // execute through command i
                if (isCurrent && ImGui::IsWindowAppearing())
                    ImGui::SetScrollHereY();
            }
            ImGui::EndChild();
            ImGui::End();
        }

        // ---- Render-target preview ----
        if (loaded) {
            ImGui::Begin("Render Target");
            GLuint fbo = g_engine.currentDrawFramebuffer();
            ImGui::Text("Bound draw FBO: %u   (%dx%d)", fbo, g_previewW, g_previewH);
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float aspect = g_previewH > 0 ? (float)g_previewW / (float)g_previewH : 1.0f;
            float w = avail.x;
            float h = w / aspect;
            if (h > avail.y) { h = avail.y; w = h * aspect; }
            // Flip V: GL texture origin is bottom-left, ImGui is top-left.
            ImGui::Image((ImTextureID)(intptr_t)g_previewTex, ImVec2(w, h),
                         ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
        }

        // ---- GL error log ----
        if (loaded) {
            ImGui::Begin("GL Errors (last step)");
            const std::string &errors = g_engine.lastGlErrors();
            if (errors.empty())
                ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "No GL errors.");
            else
                ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "%s", errors.c_str());
            ImGui::End();
        }

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
        glfwSwapBuffers(g_window);
    }

    if (loaded)
        ResourceAllocator::deleteAllResources(g_engine.capture());
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}
