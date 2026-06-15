/**
 * @file Texture.h
 * @author chenwenjun.liu@unity.cn
 * @brief Capture texture descriptor — maps to textures/Texture#N.json
 * @version 0.2
 */
#ifndef TEXTURE_H
#define TEXTURE_H

#include <cstdint>
#include <string>
#include <unordered_map>

/// Full texture descriptor matching the capture schema textures/Texture#N.json
struct TextureWrapper {
    // ---- identity ----
    uint32_t    m_textureId       = 0;
    std::string m_label;

    // ---- paths ----
    std::string m_textureMetaPath;
    std::string m_textureBinaryPath;

    // ---- descriptor ----
    uint32_t m_target         = 0;   // GLenum: 3553=TEXTURE_2D, 34067=TEXTURE_CUBE_MAP
    uint32_t m_width          = 0;
    uint32_t m_height         = 0;
    uint32_t m_depth          = 0;
    uint32_t m_internalFormat = 0;   // GLenum: 0x8058=RGBA8, 0x88F0=DEPTH24_STENCIL8
    uint32_t m_format         = 0;   // GLenum: 0x1908=RGBA
    uint32_t m_pixelType      = 0;   // GLenum: 0x1401=UNSIGNED_BYTE
    bool     m_compressed     = false;
    uint32_t m_mipmapLevels   = 1;
    bool     m_mipmapsGenerated = false;
    bool     m_deleted         = false;

    // ---- parameters ----
    std::unordered_map<std::string, int32_t> m_parameters;

    // ---- capture metadata ----
    std::string m_sourceKind;       // may be empty
    std::string m_captureMethod;    // "FBOReadPixels" / "DepthBlit" / "FBOReadPixelsCubeMap" / ...
    uint64_t    m_byteLength         = 0;
    uint32_t    m_capturedWidth      = 0;
    uint32_t    m_capturedHeight     = 0;
    uint64_t    m_capturedByteLength = 0;
    bool        m_hasBaselineBytes   = false;
};

#endif
