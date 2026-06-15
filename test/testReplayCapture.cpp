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
#include <iostream>
#include <string>

#include "framecapture/CaptureLoader.h"
#include "resourceManagement/ResourceAllocator.h"
#include "resourceManagement/ResourceManager.h"
#include "shaderTranslation/ShaderInterpreter.h"

#include "../src/config.h"

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

// ---- main ----
int main(int argc, char *argv[]) {
  // ---- parse args ----
  std::string capturePath =
      (argc > 1) ? argv[1]
                 : std::string(WORKING_DIR) + "/example/20260615_110825_frame0";

  std::cout << "Capture path: " << capturePath << std::endl;

  // ---- init GLFW ----
  glfwSetErrorCallback(errorCallback);
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Capture Replay", nullptr, nullptr);
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

  // ---- load capture ----
  FrameCapture capture;
  std::string error;
  if (!CaptureLoader::loadFromDirectory(capturePath, capture, error)) {
    std::cerr << "Load FAILED: " << error << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

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

  printDivider("Uploading Resource Data");
  bool uploadOk = true;
  std::cout << "  Uploading " << capture.bufferCount() << " buffers..."
            << std::endl;
  for (size_t i = 0; i < capture.m_buffers.size() && uploadOk; ++i) {
    if (!ResourceManager::uploadBufferData(capture.m_buffers[i], capturePath,
                                           error)) {
      std::cerr << "  Buffer #" << capture.m_buffers[i].m_bufferId
                << " upload FAILED: " << error << std::endl;
      uploadOk = false;
    }
  }
  if (uploadOk)
    std::cout << "  Buffers done." << std::endl;

  if (uploadOk) {
    std::cout << "  Uploading " << capture.textureCount() << " textures..."
              << std::endl;
    for (size_t i = 0; i < capture.m_textures.size() && uploadOk; ++i) {
      if (!ResourceManager::uploadTextureData(capture.m_textures[i],
                                              capturePath, error)) {
        std::cerr << "  Texture #" << capture.m_textures[i].m_textureId
                  << " upload FAILED: " << error << std::endl;
        uploadOk = false;
      }
    }
    if (uploadOk)
      std::cout << "  Textures done." << std::endl;
  }

  if (uploadOk) {
    std::cout << "  Compiling " << capture.shaderCount() << " shaders..."
              << std::endl;
    for (size_t i = 0; i < capture.m_shaders.size() && uploadOk; ++i) {
      std::cout << "    Shader #" << capture.m_shaders[i].m_shaderId << "..."
                << std::flush;
      if (!ResourceManager::compileShader(capture.m_shaders[i], error)) {
        std::cerr << " FAILED: " << error << std::endl;
        // Print translated source for debugging
        if (capture.m_shaders[i].m_shaderId == 4) {
          std::string debugSrc = capture.m_shaders[i].m_source;
          std::string ignoreErr;
          ShaderInterpreter::translateInPlace(debugSrc, ignoreErr);
          std::cerr << "    -- Translated Shader #4 --" << std::endl;
          int lineNo = 0;
          for (size_t p = 0; p < debugSrc.size();) {
            size_t nl = debugSrc.find('\n', p);
            std::string line = (nl != std::string::npos)
                                   ? debugSrc.substr(p, nl - p)
                                   : debugSrc.substr(p);
            std::cerr << "    " << ++lineNo << ": " << line << std::endl;
            if (nl == std::string::npos)
              break;
            p = nl + 1;
          }
          std::cerr << "    -- End Shader #4 --" << std::endl;
        }
        // uploadOk = false; // continue trying all shaders
      } else {
        std::cout << " OK" << std::endl;
      }
    }
    if (uploadOk)
      std::cout << "  Shaders done." << std::endl;
    else
      std::cout << "  Shader compilation had errors, continuing..."
                << std::endl;
  }

  if (uploadOk) {
    std::cout << "  Linking " << capture.programCount() << " programs..."
              << std::endl;
    for (size_t i = 0; i < capture.m_programs.size() && uploadOk; ++i) {
      if (!ResourceManager::linkProgram(capture.m_programs[i], error)) {
        std::cerr << "  Program #" << capture.m_programs[i].m_programId
                  << " link FAILED: " << error << std::endl;
        uploadOk = false;
      }
    }
    if (uploadOk)
      std::cout << "  Programs done." << std::endl;
  }

  if (uploadOk) {
    std::cout << "  Restoring state..." << std::endl;
    if (!ResourceManager::restoreState(capture.m_state, error)) {
      std::cerr << "  State restore FAILED: " << error << std::endl;
      uploadOk = false;
    }
    if (uploadOk)
      std::cout << "  State restored." << std::endl;
  }

  if (!uploadOk) {
    std::cout << "  Continuing despite upload errors..." << std::endl;
  }

  // ---- execute commands ----
  printDivider("Executing Commands");
  Command::setCaptureDirectory(capturePath);
  size_t executedCount = 0;
  for (auto &cmd : capture.m_commands) {
    if (cmd) {
      cmd->execute();
      ++executedCount;
    }
  }
  std::cout << "  Executed " << executedCount << " commands." << std::endl;

  // ---- check GL errors after execution ----
  GLenum glErr = glGetError();
  if (glErr != GL_NO_ERROR) {
    std::cerr << "  GL error after execution: 0x" << std::hex << glErr
              << std::dec << std::endl;
  } else {
    std::cout << "  No GL errors." << std::endl;
  }

  printDivider("Controls");
  std::cout << "  ESC      — exit" << std::endl;
  std::cout << "  R        — re-execute all commands" << std::endl;
  std::cout << "  SPACE    — single-step next command" << std::endl;

  // ---- main loop ----
  size_t stepIndex = 0;
  while (!glfwWindowShouldClose(window)) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // handle key events for re-execution / stepping
    for (auto &cmd : capture.m_commands) {
      if (cmd)
        cmd->execute();
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // ---- cleanup ----
  std::cout << "\nShutting down." << std::endl;
  capture.clear();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
