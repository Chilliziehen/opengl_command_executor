/**
 * @file testLoadCapture.cpp
 * @brief Smoke test: loads the example capture and prints a summary
 */
#include <iostream>
#include <string>
#include "framecapture/CaptureLoader.h"

int main(int argc, char* argv[]) {
    std::string capturePath = (argc > 1)
        ? argv[1]
        : std::string(PROJECT_ROOT_DIR) + "/example/20260611_161020_frame6";

    std::cout << "Loading capture from: " << capturePath << std::endl;

    FrameCapture capture;
    std::string  error;
    if (!CaptureLoader::loadFromDirectory(capturePath, capture, error)) {
        std::cerr << "FAILED: " << error << std::endl;
        return 1;
    }

    const auto& m = capture.m_manifest;
    std::cout << "\n========== Manifest =========="  << std::endl;
    std::cout << "  format       : " << m.m_format       << std::endl;
    std::cout << "  version      : " << m.m_version      << std::endl;
    std::cout << "  model        : " << m.m_captureModel << std::endl;
    std::cout << "  generatedAt  : " << m.m_generatedAt  << std::endl;
    std::cout << "  frameIndex   : " << m.m_frameIndex   << std::endl;
    std::cout << "  canvas       : " << m.m_canvas.width << "x" << m.m_canvas.height
              << " (css " << m.m_canvas.cssWidth << "x" << m.m_canvas.cssHeight
              << ", dpr " << m.m_canvas.devicePixelRatio << ")" << std::endl;

    std::cout << "\n========== Resources ==========" << std::endl;
    std::cout << "  buffers      : " << capture.bufferCount()       << std::endl;
    std::cout << "  textures     : " << capture.textureCount()      << std::endl;
    std::cout << "  shaders      : " << capture.shaderCount()       << std::endl;
    std::cout << "  programs     : " << capture.programCount()      << std::endl;
    std::cout << "  vertexArrays : " << capture.vertexArrayCount()  << std::endl;
    std::cout << "  framebuffers : " << capture.framebufferCount()  << std::endl;
    std::cout << "  commands     : " << capture.commandCount()      << std::endl;

    std::cout << "\n========== State Snapshot ==========" << std::endl;
    const auto& s = capture.m_state;
    std::cout << "  currentProgram       : " << s.m_currentProgramId       << std::endl;
    std::cout << "  currentFramebuffer   : " << s.m_currentFramebufferId   << std::endl;
    std::cout << "  currentVAO           : " << s.m_currentVertexArrayId   << std::endl;
    std::cout << "  currentArrayBuffer   : " << s.m_currentArrayBufferId   << std::endl;
    std::cout << "  currentElementBuffer : " << s.m_currentElementArrayBufferId << std::endl;
    std::cout << "  depthMask            : " << s.m_depthMask             << std::endl;
    std::cout << "  depthFunc            : " << s.m_depthFunction         << std::endl;
    std::cout << "  cullFace             : " << s.m_cullFaceMode          << std::endl;

    std::cout << "\n========== First 5 Commands ==========" << std::endl;
    size_t showCount = std::min(capture.commandCount(), size_t(5));
    for (size_t i = 0; i < showCount; ++i) {
        const auto& cmd = capture.m_commands[i];
        std::cout << "  [" << cmd->getEventId() << "] "
                  << cmd->getCommandName() << std::endl;
    }

    std::cout << "\n========== Texture Previews ==========" << std::endl;
    for (const auto& t : capture.m_textures) {
        std::cout << "  id=" << t.m_textureId
                  << " " << t.m_label
                  << " " << t.m_width << "x" << t.m_height
                  << " captured=" << t.m_capturedWidth << "x" << t.m_capturedHeight
                  << " method=" << t.m_captureMethod
                  << " bytes=" << t.m_byteLength << std::endl;
    }

    std::cout << "\nDONE." << std::endl;
    return 0;
}
