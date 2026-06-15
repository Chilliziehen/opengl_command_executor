/**
 * @file FrameCapture.h
 * @brief Aggregation container for all resources in a single captured frame
 */

#ifndef FRAME_CAPTURE_H
#define FRAME_CAPTURE_H

#include <cstdint>
#include <vector>
#include "drawResources/Manifest.h"
#include "drawResources/Buffer.h"
#include "drawResources/Texture.h"
#include "drawResources/Shader.h"
#include "drawResources/Program.h"
#include "drawResources/VertexArrayObject.h"
#include "drawResources/Framebuffer.h"
#include "drawResources/State.h"
#include "drawResources/Command.h"

/// Holds all resources loaded from a single capture directory
class FrameCapture {
public:
    // ---- resources ----
    CaptureManifest                       m_manifest;
    std::vector<CaptureBuffer>            m_buffers;
    std::vector<TextureWrapper>           m_textures;
    std::vector<CaptureShader>            m_shaders;
    std::vector<CaptureProgram>           m_programs;
    std::vector<CaptureVertexArrayObject> m_vertexArrays;
    std::vector<CaptureFramebuffer>       m_framebuffers;
    CaptureState                          m_state;
    std::vector<Command>                  m_commands;

    // ---- lookup ----
    const CaptureBuffer*            findBuffer(uint32_t id) const;
    const TextureWrapper*           findTexture(uint32_t id) const;
    const CaptureShader*            findShader(uint32_t id) const;
    const CaptureProgram*           findProgram(uint32_t id) const;
    const CaptureVertexArrayObject* findVertexArray(uint32_t id) const;
    const CaptureFramebuffer*       findFramebuffer(uint32_t id) const;

    // ---- capacity ----
    size_t bufferCount()       const { return m_buffers.size(); }
    size_t textureCount()      const { return m_textures.size(); }
    size_t shaderCount()       const { return m_shaders.size(); }
    size_t programCount()      const { return m_programs.size(); }
    size_t vertexArrayCount()  const { return m_vertexArrays.size(); }
    size_t framebufferCount()  const { return m_framebuffers.size(); }
    size_t commandCount()      const { return m_commands.size(); }

    /// Release all resources back to default state
    void clear();
};

#endif // FRAME_CAPTURE_H
