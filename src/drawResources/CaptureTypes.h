/**
 * @file CaptureTypes.h
 * @brief Common types and GL enum constants for capture data structures
 */

#ifndef CAPTURE_TYPES_H
#define CAPTURE_TYPES_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/// GL enum constants used across capture data
namespace CaptureGlEnum {

// --- Texture targets ---
constexpr uint32_t kTexture2d       = 0x0DE1; // 3553
constexpr uint32_t kTextureCubeMap  = 0x8513; // 34067

// --- Buffer targets ---
constexpr uint32_t kArrayBuffer         = 0x8892; // 34962
constexpr uint32_t kElementArrayBuffer  = 0x8893; // 34963
constexpr uint32_t kUniformBuffer       = 0x8A11; // 35345
constexpr uint32_t kCopyReadBuffer      = 0x8F36; // 36662
constexpr uint32_t kCopyWriteBuffer     = 0x8F37; // 36663

// --- Buffer usage ---
constexpr uint32_t kStaticDraw  = 0x88E4; // 35044
constexpr uint32_t kDynamicDraw = 0x88E8; // 35048

// --- Shader types ---
constexpr uint32_t kFragmentShader = 0x8B30; // 35632
constexpr uint32_t kVertexShader   = 0x8B31; // 35633

// --- Pixel formats ---
constexpr uint32_t kRgba              = 0x1908; // 6408
constexpr uint32_t kRgba8             = 0x8058; // 32856
constexpr uint32_t kDepthComponent16  = 0x81A5; // 33157
constexpr uint32_t kDepthComponent24  = 0x81A6; // 33158
constexpr uint32_t kDepthComponent32f = 0x8CAC; // 36012
constexpr uint32_t kDepth24Stencil8   = 0x88F0; // 35056

// --- Pixel types ---
constexpr uint32_t kUnsignedByte = 0x1401; // 5121
constexpr uint32_t kUnsignedInt  = 0x1405; // 5125
constexpr uint32_t kFloat        = 0x1406; // 5126

// --- Framebuffer ---
constexpr uint32_t kFramebuffer          = 0x8D40; // 36160
constexpr uint32_t kColorAttachment0     = 0x8CE0; // 36064
constexpr uint32_t kDepthAttachment      = 0x8D00; // 36096
constexpr uint32_t kDepthStencilAttachment = 0x821A; // 33306

// --- Capabilities ---
constexpr uint32_t kDepthTest = 0x0B71; // 2929

// --- Draw ---
constexpr uint32_t kTriangles      = 0x0004; // 4
constexpr uint32_t kUnsignedShort  = 0x1403; // 5123

// --- Depth / Stencil ---
constexpr uint32_t kLess     = 0x0201; // 513
constexpr uint32_t kBack     = 0x0405; // 1029
constexpr uint32_t kCw       = 0x0901; // 2305
constexpr uint32_t kCcw      = 0x0902; // 2306

// --- Blend ---
constexpr uint32_t kFuncAdd              = 0x8006; // 32774
constexpr uint32_t kOne                  = 0x0001; // 1
constexpr uint32_t kZero                 = 0x0000; // 0
constexpr uint32_t kSrcAlpha             = 0x0302; // 770
constexpr uint32_t kOneMinusSrcAlpha     = 0x0303; // 771
constexpr uint32_t kDstAlpha             = 0x0304; // 772
constexpr uint32_t kOneMinusDstAlpha     = 0x0305; // 773

// --- Texture units ---
constexpr uint32_t kTexture0 = 0x84C0; // 33984

// --- Read framebuffer ---
constexpr uint32_t kReadFramebuffer = 0x8CA8; // 36009

// --- Clear ---
constexpr uint32_t kColorBufferBit   = 0x4000; // 16384
constexpr uint32_t kDepthBufferBit   = 0x0100; // 256
constexpr uint32_t kStencilBufferBit = 0x0400; // 1024
// Combined depth+color:
// 0x4100 = 16640 = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT

} // namespace CaptureGlEnum

/// Buffer type label
enum class BufferType : uint8_t {
    VertexBuffer,
    ElementBuffer,
    UniformBuffer,
    Other
};

/// Canonical capture format identifier
constexpr const char* kCaptureFormat = "tjrelic.wx.capture";
constexpr uint32_t    kCaptureVersion = 2;

/// File index entry for a single resource in manifest
struct ResourceFileEntry {
    uint32_t    resourceId = 0;
    std::string label;
    std::string metadataPath;
    std::string binaryDataPath;
};

/// Canvas descriptor from manifest
struct CanvasDescriptor {
    uint32_t width             = 0;
    uint32_t height            = 0;
    uint32_t cssWidth          = 0;
    uint32_t cssHeight         = 0;
    float    devicePixelRatio  = 1.0f;
};

/// Diagnostics info from manifest
struct DiagnosticsInfo {
    std::string profile;
    uint32_t    commandLimit            = 0;
    uint32_t    commandCount            = 0;
    uint32_t    uniformCommandCount     = 0;
    bool        commandLimitReached     = false;
    std::string resourceBaselineMode;
    uint32_t    maxResourceBaselineBytes = 0;
    bool        bufferUploadPayloadEnabled   = false;
    bool        textureUploadPayloadEnabled  = false;
    std::string uniformValueMode;
};

#endif // CAPTURE_TYPES_H
