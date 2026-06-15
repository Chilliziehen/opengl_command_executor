#include "FrameCapture.h"
#include <algorithm>

const CaptureBuffer* FrameCapture::findBuffer(uint32_t id) const {
    auto it = std::find_if(m_buffers.begin(), m_buffers.end(),
        [id](const CaptureBuffer& b) { return b.m_bufferId == id; });
    return it != m_buffers.end() ? &(*it) : nullptr;
}

const TextureWrapper* FrameCapture::findTexture(uint32_t id) const {
    auto it = std::find_if(m_textures.begin(), m_textures.end(),
        [id](const TextureWrapper& t) { return t.m_textureId == id; });
    return it != m_textures.end() ? &(*it) : nullptr;
}

const CaptureShader* FrameCapture::findShader(uint32_t id) const {
    auto it = std::find_if(m_shaders.begin(), m_shaders.end(),
        [id](const CaptureShader& s) { return s.m_shaderId == id; });
    return it != m_shaders.end() ? &(*it) : nullptr;
}

const CaptureProgram* FrameCapture::findProgram(uint32_t id) const {
    auto it = std::find_if(m_programs.begin(), m_programs.end(),
        [id](const CaptureProgram& p) { return p.m_programId == id; });
    return it != m_programs.end() ? &(*it) : nullptr;
}

const CaptureVertexArrayObject* FrameCapture::findVertexArray(uint32_t id) const {
    auto it = std::find_if(m_vertexArrays.begin(), m_vertexArrays.end(),
        [id](const CaptureVertexArrayObject& v) { return v.m_vertexArrayId == id; });
    return it != m_vertexArrays.end() ? &(*it) : nullptr;
}

const CaptureFramebuffer* FrameCapture::findFramebuffer(uint32_t id) const {
    auto it = std::find_if(m_framebuffers.begin(), m_framebuffers.end(),
        [id](const CaptureFramebuffer& f) { return f.m_framebufferId == id; });
    return it != m_framebuffers.end() ? &(*it) : nullptr;
}

void FrameCapture::clear() {
    m_manifest = CaptureManifest{};
    m_buffers.clear();
    m_textures.clear();
    m_shaders.clear();
    m_programs.clear();
    m_vertexArrays.clear();
    m_framebuffers.clear();
    m_state = CaptureState{};
    m_commands.clear();
}
