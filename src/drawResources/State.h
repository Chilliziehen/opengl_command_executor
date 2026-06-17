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

/// Per-face stencil state (state.json stencilFront / stencilBack)
struct StencilFaceCapture {
    uint32_t m_func        = 0x0207; // GL_ALWAYS
    int32_t  m_ref         = 0;
    uint32_t m_compareMask = 0xFFFFFFFFu;
    uint32_t m_writeMask   = 0xFFFFFFFFu;
    uint32_t m_sfail       = 0x1E00; // GL_KEEP
    uint32_t m_dpfail      = 0x1E00;
    uint32_t m_dppass      = 0x1E00;
};

/// A UBO binding-point entry (state.json uniformBufferBindings[i])
struct UboBindingCapture {
    uint32_t m_bufferId = 0;
    int64_t  m_offset   = 0;
    int64_t  m_size     = 0; // 0 = bindBufferBase (whole buffer)
};

/// glPixelStorei pack/unpack parameter group (state.json pixelPack / pixelUnpack)
struct PixelStoreCapture {
    bool    m_swapBytes   = false;
    bool    m_lsbFirst    = false;
    int32_t m_rowLength   = 0;
    int32_t m_imageHeight = 0;
    int32_t m_skipRows    = 0;
    int32_t m_skipPixels  = 0;
    int32_t m_skipImages  = 0;
    int32_t m_alignment   = 4;
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

    /// Pixel store parameters (key = GLenum string, value = int) — legacy "pixelStore"
    std::unordered_map<std::string, int32_t> m_pixelStoreParameters;

    // ──────────────── v2 additions ────────────────

    /// GL_READ_FRAMEBUFFER_BINDING (0 = default)
    uint32_t m_readFramebufferId = 0;
    /// glDrawBuffers GLenum list
    std::vector<uint32_t> m_drawBuffers;
    /// glReadBuffer GLenum (default GL_BACK)
    uint32_t m_readBuffer = 0x0405;

    /// binding point (string key) → UBO binding
    std::unordered_map<std::string, UboBindingCapture> m_uniformBufferBindings;
    /// texture unit (string key) → sampler object ID
    std::unordered_map<std::string, uint32_t> m_samplerObjects;

    /// WebGL2 separate-format vertex bindings (binding point → {buffer,offset,stride})
    std::unordered_map<std::string, VertexAttribute> m_vertexBufferBindings; // reuse fields loosely
    std::unordered_map<std::string, uint32_t> m_vertexBindingDivisors;

    /// Blend
    bool m_blendEnabled = false;
    std::vector<float> m_blendColor; // [r,g,b,a]

    /// Stencil
    bool m_stencilTestEnabled = false;
    StencilFaceCapture m_stencilFront;
    StencilFaceCapture m_stencilBack;

    /// Depth range
    float m_depthRangeNear = 0.0f;
    float m_depthRangeFar  = 1.0f;

    /// Rasterizer extras
    bool  m_scissorTestEnabled = false;
    bool  m_polygonOffsetFillEnabled = false;
    float m_polygonOffsetFactor = 0.0f;
    float m_polygonOffsetUnits  = 0.0f;
    float m_lineWidth = 1.0f;
    bool  m_multisampleEnabled = true;

    /// Pixel storage (v2 structured) + PBO bindings
    PixelStoreCapture m_pixelPack;
    PixelStoreCapture m_pixelUnpack;
    bool m_hasPixelPackUnpack = false; // whether the structured fields were present
    uint32_t m_pixelPackBuffer = 0;
    uint32_t m_pixelUnpackBuffer = 0;
};

#endif // STATE_H
