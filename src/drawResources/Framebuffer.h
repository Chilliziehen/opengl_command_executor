/**
 * @file Framebuffer.h
 * @brief Capture framebuffer descriptor — maps to entries in framebuffers.json
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <cstdint>
#include <string>
#include <vector>

/// Single attachment within a framebuffer
struct FramebufferAttachment {
    /// GL attachment point enum (36064=GL_COLOR_ATTACHMENT0, 36096=GL_DEPTH_ATTACHMENT, etc.)
    uint32_t m_attachmentPoint = 0;

    /// Texture ID bound to this attachment (0 = renderbuffer)
    uint32_t m_textureId = 0;

    /// Mipmap level of the attached texture
    uint32_t m_mipmapLevel = 0;

    /// Texture target of the attachment (3553=GL_TEXTURE_2D)
    uint32_t m_textureTarget = 0;
};

/// Framebuffer descriptor matching an entry in the framebuffers.json array
struct CaptureFramebuffer {
    /// Unique framebuffer identifier
    uint32_t m_framebufferId = 0;

    /// Human-readable label, e.g. "Framebuffer#1"
    std::string m_label;

    /// Ordered list of color + depth/stencil attachments
    std::vector<FramebufferAttachment> m_attachments;
};

#endif // FRAMEBUFFER_H
