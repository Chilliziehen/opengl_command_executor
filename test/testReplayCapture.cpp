/**
 * @file testReplayCapture.cpp
 * @brief Full replay test: loads a capture, allocates GPU resources, executes
 * all commands
 *
 * Usage: testReplayCapture [capture_directory]
 *   Default: PROJECT_ROOT_DIR/example/20260611_161020_frame6
 *
 * Controls:
 *   ESC  — exit
 *   R    — re-execute all commands
 *   Space — single-step (execute one command, press again for next)
 */

#include <glad/gl.h>
#define GLAD_GL_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "framecapture/CaptureLoader.h"
#include "mappers/UniversalDMapper.h"
#include "resourceManagement/ResourceAllocator.h"
#include "resourceManagement/ResourceManager.h"
#include "shaderTranslation/ShaderInterpreter.h"

// ---- GLFW callbacks ----
static void errorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

static void keyCallback(GLFWwindow *window, int key, int /*scancode*/,
                        int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// ---- helpers ----
static void printGlInfo() {
  std::cout << "GL Vendor   : " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "GL Renderer : " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "GL Version  : " << glGetString(GL_VERSION) << std::endl;
}

static void printDivider(const char *title) {
  std::cout << "\n========== " << title << " ==========" << std::endl;
}

// ---- GL error debug helpers ----
static const char *glErrorName(GLenum err) {
  switch (err) {
  case GL_NO_ERROR:                      return "GL_NO_ERROR";
  case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
  case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
  case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
  case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
  case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
  default:                               return "GL_UNKNOWN";
  }
}

// Drain the GL error queue, printing every pending error tagged with `label`.
// Returns true if any error was found.
static bool checkGlErrors(const std::string &label) {
  bool any = false;
  for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
    std::cerr << "  [GL ERROR] " << label << " -> 0x" << std::hex << err
              << std::dec << " (" << glErrorName(err) << ")" << std::endl;
    any = true;
  }
  return any;
}

// ---- main ----
int main(int argc, char *argv[]) {
  // ---- parse args ----
  std::string capturePath =
      (argc > 1) ? argv[1]
                 : std::string(PROJECT_ROOT_DIR) + "/example/20260611_161020_frame6";

  std::cout << "Capture path: " << capturePath << std::endl;

  // ---- init GLFW ----
  glfwSetErrorCallback(errorCallback);
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return 1;
  }

  // ---- load capture (pure CPU; no GL context needed yet) ----
  // Loaded before window creation so the window/viewport can be sized from the
  // captured canvas dimensions.
  FrameCapture capture;
  std::string error;
  if (!CaptureLoader::loadFromDirectory(capturePath, capture, error)) {
    std::cerr << "Load FAILED: " << error << std::endl;
    glfwTerminate();
    return 1;
  }

  // ---- size the window from the captured canvas ----
  int windowWidth  = static_cast<int>(capture.m_manifest.m_canvas.width);
  int windowHeight = static_cast<int>(capture.m_manifest.m_canvas.height);
  if (windowWidth <= 0 || windowHeight <= 0) {
    windowWidth  = 1280;
    windowHeight = 720;
    std::cerr << "  Canvas size missing/invalid; falling back to "
              << windowWidth << "x" << windowHeight << std::endl;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight,
                                        "Capture Replay", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return 1;
  }

  glfwSetKeyCallback(window, keyCallback);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // ---- load GL via glad ----
  int gladVersion = gladLoadGL(glfwGetProcAddress);
  if (gladVersion == 0) {
    std::cerr << "Failed to initialize glad (GL loader)" << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  std::cout << "glad loaded GL " << GLAD_VERSION_MAJOR(gladVersion) << "."
            << GLAD_VERSION_MINOR(gladVersion) << std::endl;
  printGlInfo();

  printDivider("Capture Loaded");
  std::cout << "  format       : " << capture.m_manifest.m_format << std::endl;
  std::cout << "  frameIndex   : " << capture.m_manifest.m_frameIndex
            << std::endl;
  std::cout << "  canvas       : " << capture.m_manifest.m_canvas.width << "x"
            << capture.m_manifest.m_canvas.height << std::endl;
  std::cout << "  buffers      : " << capture.bufferCount() << std::endl;
  std::cout << "  textures     : " << capture.textureCount() << std::endl;
  std::cout << "  shaders      : " << capture.shaderCount() << std::endl;
  std::cout << "  programs     : " << capture.programCount() << std::endl;
  std::cout << "  vertexArrays : " << capture.vertexArrayCount() << std::endl;
  std::cout << "  framebuffers : " << capture.framebufferCount() << std::endl;
  std::cout << "  commands     : " << capture.commandCount() << std::endl;
  // ---- initialize GL to the capture's frame-start state ----
  // Allocate every GPU object first, then run the full upload + state-restore
  // pipeline (buffers -> textures -> shaders -> programs -> state -> FBOs ->
  // VAOs). uploadAllResources is the single, correctly-ordered entry point;
  // it also restores VAO/FBO/vertex-attribute state, which the previous
  // hand-written sequence skipped.
  checkGlErrors("after glad/context init");

  printDivider("Allocating GPU Resources");
  if (!ResourceAllocator::allocateAllResources(capture, error)) {
    std::cerr << "Allocation FAILED: " << error << std::endl;
  } else {
    std::cout << "  All "
              << (capture.bufferCount() + capture.textureCount() +
                  capture.shaderCount() + capture.programCount() +
                  capture.vertexArrayCount() + capture.framebufferCount())
              << " GPU objects created." << std::endl;
  }
  checkGlErrors("allocateAllResources");

  printDivider("Uploading Resources & Restoring State");
  if (!ResourceManager::uploadAllResources(capture, capturePath, error)) {
    std::cerr << "  Initialization FAILED: " << error << std::endl;
    std::cout << "  Continuing despite initialization errors..." << std::endl;
  } else {
    std::cout << "  Resources uploaded and frame-start state restored."
              << std::endl;
  }
  checkGlErrors("uploadAllResources");

  // ---- execute commands ----
  printDivider("Executing Commands");
  Command::setCaptureDirectory(capturePath);
  // Drain any leftover errors so per-command checks below are attributable.
  checkGlErrors("before command replay (leftover)");
  // Debug: stop replay after this eventId, then blit the current draw FBO to the
  // screen so intermediate render passes can be inspected. -1 = run all.
  long stopAt = -1;
  if (const char *s = std::getenv("REPLAY_STOP_AT"))
    stopAt = std::atol(s);
  size_t executedCount = 0;
  for (auto &cmd : capture.m_commands) {
    if (cmd) {
      cmd->execute();
      ++executedCount;
      // Tag any error with the command that produced it.
      checkGlErrors("cmd #" + std::to_string(cmd->getEventId()) + " " +
                    cmd->getCommandName());
      if (stopAt >= 0 && static_cast<long>(cmd->getEventId()) >= stopAt) {
        GLint drawFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        std::cout << "  [STOP] after eventId " << cmd->getEventId()
                  << "; blitting draw FBO " << drawFbo << " to screen" << std::endl;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFbo);
        // Read the HDR color attachment as float to see dim (sub-1/255) content
        // that an 8-bit blit would round to black.
        {
          std::vector<float> hdr(static_cast<size_t>(windowWidth) * windowHeight * 4);
          glReadPixels(0, 0, windowWidth, windowHeight, GL_RGBA, GL_FLOAT, hdr.data());
          float mx = 0.0f; size_t nz = 0;
          for (size_t i = 0; i < hdr.size(); i += 4) {
            float m = hdr[i] > hdr[i+1] ? hdr[i] : hdr[i+1];
            if (hdr[i+2] > m) m = hdr[i+2];
            if (m > mx) mx = m;
            if (m > 0.0f) ++nz;
          }
          std::cout << "  [STOP] HDR max=" << mx << " nonzero=" << nz << std::endl;
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth,
                          windowHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        break;
      }
    }
  }
  std::cout << "  Executed " << executedCount << " commands." << std::endl;

  // ---- final GL error sweep ----
  if (!checkGlErrors("after execution (final sweep)"))
    std::cout << "  No GL errors." << std::endl;

  // Debug: inspect a depth texture (e.g. shadow map). REPLAY_DEBUG_TEX=<captureId>
  if (const char *texArg = std::getenv("REPLAY_DEBUG_TEX")) {
    uint32_t texId = static_cast<uint32_t>(std::atol(texArg));
    if (hasMappedHandle(ResourceKind::Texture, texId)) {
      GLuint h = getMappedHandle(ResourceKind::Texture, texId);
      glBindTexture(GL_TEXTURE_2D, h);
      GLint w = 0, ht = 0;
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ht);
      std::vector<float> px(static_cast<size_t>(w) * ht);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, px.data());
      float mn = 1e9f, mx = -1e9f;
      double sum = 0;
      for (float v : px) { if (v < mn) mn = v; if (v > mx) mx = v; sum += v; }
      std::cout << "  [TEX " << texId << "] " << w << "x" << ht << " depth min="
                << mn << " max=" << mx << " mean=" << (sum / px.size()) << std::endl;
    }
  }

  // ---- debug: confirm the draw produced output ----
  // Read back the (not-yet-swapped) back buffer and count pixels that differ
  // from the captured clear color. Zero non-background pixels => nothing drew.
  // Uses the ACTUAL framebuffer size (the window may be clamped to the screen).
  {
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    unsigned char clearR = 0, clearG = 0, clearB = 0;
    if (capture.m_state.m_clearColor.size() >= 3) {
      clearR = static_cast<unsigned char>(capture.m_state.m_clearColor[0] * 255.0f + 0.5f);
      clearG = static_cast<unsigned char>(capture.m_state.m_clearColor[1] * 255.0f + 0.5f);
      clearB = static_cast<unsigned char>(capture.m_state.m_clearColor[2] * 255.0f + 0.5f);
    }
    std::vector<unsigned char> pixels(
        static_cast<size_t>(fbWidth) * fbHeight * 4);
    glReadPixels(0, 0, fbWidth, fbHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    size_t drawn = 0;
    for (size_t i = 0; i < pixels.size(); i += 4) {
      int dr = std::abs(static_cast<int>(pixels[i + 0]) - clearR);
      int dg = std::abs(static_cast<int>(pixels[i + 1]) - clearG);
      int db = std::abs(static_cast<int>(pixels[i + 2]) - clearB);
      if (dr > 8 || dg > 8 || db > 8)
        ++drawn;
    }
    std::cout << "  Framebuffer size: " << fbWidth << "x" << fbHeight
              << std::endl;
    std::cout << "  Clear color RGB = (" << (int)clearR << "," << (int)clearG
              << "," << (int)clearB << ")" << std::endl;
    std::cout << "  Non-background pixels: " << drawn << " / "
              << (static_cast<size_t>(fbWidth) * fbHeight) << std::endl;

    // Optional screenshot dump (set REPLAY_DUMP=<path.ppm>) for visual debug.
    // Written as a binary PPM (P6), flipped to image (top-left) orientation.
    if (const char *dumpPath = std::getenv("REPLAY_DUMP")) {
      if (FILE *fp = std::fopen(dumpPath, "wb")) {
        std::fprintf(fp, "P6\n%d %d\n255\n", fbWidth, fbHeight);
        std::vector<unsigned char> row(static_cast<size_t>(fbWidth) * 3);
        for (int y = fbHeight - 1; y >= 0; --y) {
          const unsigned char *src = pixels.data() +
              static_cast<size_t>(y) * fbWidth * 4;
          for (int x = 0; x < fbWidth; ++x) {
            row[x * 3 + 0] = src[x * 4 + 0];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 2];
          }
          std::fwrite(row.data(), 1, row.size(), fp);
        }
        std::fclose(fp);
        std::cout << "  Wrote screenshot to " << dumpPath << std::endl;
      }
    }
  }

  printDivider("Controls");
  std::cout << "  ESC      — exit" << std::endl;
  std::cout << "  R        — re-execute all commands" << std::endl;
  std::cout << "  SPACE    — single-step next command" << std::endl;

  // ---- main loop ----
  // Re-run the captured frame every iteration. The commands set their own
  // viewport / clear color / clear, so we do NOT override the viewport with the
  // window size here — doing so previously fought the captured viewport.
  while (!glfwWindowShouldClose(window)) {
    for (auto &cmd : capture.m_commands) {
      if (cmd)
        cmd->execute();
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // ---- cleanup ----
  std::cout << "\nShutting down." << std::endl;
  // Delete GL objects + clear id→handle mappings while the context is still
  // current, then release CPU-side capture data.
  ResourceAllocator::deleteAllResources(capture);
  capture.clear();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
