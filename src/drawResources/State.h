/**
 * @file State.h
 * @brief GL state snapshot — maps to state.json
 */

#ifndef STATE_H
#define STATE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "VertexArrayObject.h"

/// Blend function configuration (separate RGB / Alpha)
struct BlendFunction {
    uint32_t m_sourceRgb       = 0;
    uint32_t m_destinationRgb  = 0;
    uint32_t m_sourceAlpha     = 0;
    uint32_t m_destinationAlpha = 0;
};

/// Blend equation configuration (separate RGB / Alpha)
/// (named *State to avoid clashing with glStateManager's `enum class
/// BlendEquation` when both libraries are used in one translation unit.)
struct BlendEquationState {
    uint32_t m_rgbMode   = 0;
    uint32_t m_alphaMode = 0;
};

/// Full GL state snapshot matching the capture schema state.json
struct CaptureState {
    /// Currently active program ID
    uint32_t m_currentProgramId = 0;

    /// Currently bound framebuffer ID (0 = default framebuffer)
    uint32_t m_currentFramebufferId = 0;

    /// Currently active texture unit index
    uint32_t m_currentActiveTextureUnit = 0;

    /// Currently bound array buffer ID
    uint32_t m_currentArrayBufferId = 0;

    /// Currently bound element array buffer ID
    uint32_t m_currentElementArrayBufferId = 0;

    /// Currently bound VAO ID
    uint32_t m_currentVertexArrayId = 0;

    /// Map from texture unit index (string key) to bound texture ID
    std::unordered_map<std::string, uint32_t> m_currentTextureBindings;

    /// Map from program ID to { uniformName → textureUnit } (sampler bindings)
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> m_currentSamplerBindings;

    /// Vertex attribute state: key = attribute index string, value = descriptor
    std::unordered_map<std::string, VertexAttribute> m_vertexAttributes;

    /// Viewport rectangle: [x, y, width, height]
    std::vector<int32_t> m_viewport;

    /// Enabled capabilities: key = GLenum string, value = true
    std::unordered_map<std::string, bool> m_capabilities;

    /// Clear color: [r, g, b, a] in 0.0–1.0
    std::vector<float> m_clearColor;

    /// Clear depth value
    float m_clearDepth = 1.0f;

    /// Clear stencil value
    int32_t m_clearStencil = 0;

    /// Color write mask: [r, g, b, a]
    std::vector<bool> m_colorMask;

    /// Depth write mask
    bool m_depthMask = true;

    /// Depth test function enum (513=GL_LESS)
    uint32_t m_depthFunction = 0;

    /// Blend function (separate RGB / Alpha)
    BlendFunction m_blendFunction;

    /// Blend equation (separate RGB / Alpha)
    BlendEquationState m_blendEquation;

    /// Face culling mode enum (1029=GL_BACK)
    uint32_t m_cullFaceMode = 0;

    /// Front face orientation enum (2305=GL_CW)
    uint32_t m_frontFaceOrientation = 0;

    /// Scissor box: [x, y, width, height]
    std::vector<int32_t> m_scissorBox;

    /// Pixel store parameters (key = GLenum string, value = int)
    std::unordered_map<std::string, int32_t> m_pixelStoreParameters;
};

#endif // STATE_H
