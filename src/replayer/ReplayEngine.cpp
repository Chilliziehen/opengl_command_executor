#include "replayer/ReplayEngine.h"

#include <glad/gl.h>

#include "framecapture/CaptureLoader.h"
#include "resourceManagement/ResourceAllocator.h"
#include "resourceManagement/ResourceManager.h"

namespace {
const std::string kEmpty;

const char *glErrorName(GLenum err) {
    switch (err) {
    case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
    default:                               return "GL_UNKNOWN";
    }
}
} // namespace

bool ReplayEngine::load(const std::string& captureDirectory, std::string& outError) {
    if (!CaptureLoader::loadFromDirectory(captureDirectory, m_capture, outError))
        return false;
    m_captureDirectory = captureDirectory;
    Command::setCaptureDirectory(captureDirectory);
    m_loaded = true;
    m_initialized = false;
    m_cursor = 0;
    return true;
}

void ReplayEngine::unload() {
    if (m_initialized) {
        ResourceAllocator::deleteAllResources(m_capture);
        m_initialized = false;
    }
    m_capture.clear();
    m_loaded = false;
    m_cursor = 0;
    m_captureDirectory.clear();
    m_lastGlErrors.clear();
}

bool ReplayEngine::reset(std::string& outError) {
    if (!m_loaded) {
        outError = "no capture loaded";
        return false;
    }
    // Release any GL objects from a previous run, then rebuild the frame-start
    // state from scratch.
    ResourceAllocator::deleteAllResources(m_capture);
    if (!ResourceAllocator::allocateAllResources(m_capture, outError))
        return false;
    Command::setCaptureDirectory(m_captureDirectory);
    if (!ResourceManager::uploadAllResources(m_capture, m_captureDirectory, outError))
        return false;
    m_cursor = 0;
    m_initialized = true;
    m_lastGlErrors.clear();
    while (glGetError() != GL_NO_ERROR) {} // drain init errors
    return true;
}

void ReplayEngine::executeOne(size_t index) {
    auto& cmd = m_capture.m_commands[index];
    if (cmd) cmd->execute();
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError()) {
        m_lastGlErrors += "cmd #" + std::to_string(cmd ? cmd->getEventId() : 0) +
                          " " + (cmd ? cmd->getCommandName() : "?") + " -> " +
                          glErrorName(e) + "\n";
    }
}

bool ReplayEngine::stepForward() {
    if (!m_initialized || m_cursor >= commandCount())
        return false;
    m_lastGlErrors.clear();
    executeOne(m_cursor);
    ++m_cursor;
    return true;
}

bool ReplayEngine::stepBackward(std::string& outError) {
    if (m_cursor == 0)
        return false;
    return seekTo(m_cursor - 1, outError);
}

bool ReplayEngine::seekTo(size_t index, std::string& outError) {
    if (index > commandCount())
        index = commandCount();
    // If we need to go backward (or aren't initialized), rebuild from scratch.
    if (!m_initialized || index < m_cursor) {
        if (!reset(outError))
            return false;
    }
    m_lastGlErrors.clear();
    while (m_cursor < index) {
        executeOne(m_cursor);
        ++m_cursor;
    }
    return true;
}

const std::string& ReplayEngine::commandName(size_t index) const {
    if (index >= m_capture.m_commands.size() || !m_capture.m_commands[index])
        return kEmpty;
    return m_capture.m_commands[index]->getCommandName();
}

uint32_t ReplayEngine::commandEventId(size_t index) const {
    if (index >= m_capture.m_commands.size() || !m_capture.m_commands[index])
        return 0;
    return m_capture.m_commands[index]->getEventId();
}

unsigned int ReplayEngine::currentDrawFramebuffer() const {
    GLint fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    return static_cast<unsigned int>(fbo);
}
