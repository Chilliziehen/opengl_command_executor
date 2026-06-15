/**
 * @file Buffer.h
 * @brief Capture buffer descriptor — maps to buffers/Buffer#N.json
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <cstdint>
#include <string>

/// Full buffer descriptor matching the capture schema buffers/Buffer#N.json
struct CaptureBuffer {
    /// Unique buffer identifier
    uint32_t m_bufferId = 0;

    /// Human-readable label, e.g. "Buffer#1"
    std::string m_label;

    /// GL buffer target enum (34962=GL_ARRAY_BUFFER, 34963=GL_ELEMENT_ARRAY_BUFFER, etc.)
    uint32_t m_target = 0;

    /// Type label: "VBO" / "EBO" / "UBO" / "Other"
    std::string m_bufferType;

    /// GL usage hint enum (35044=GL_STATIC_DRAW, 35048=GL_DYNAMIC_DRAW)
    uint32_t m_usageHint = 0;

    /// Allocated buffer size in bytes
    uint64_t m_byteLength = 0;

    /// Actually captured data size in bytes (≤ byteLength)
    uint64_t m_capturedByteLength = 0;

    /// Whether baseline bytes were captured at frame start
    bool m_hasBaselineBytes = false;

    /// Capture source: "CopyBufferSubData@4MBcap" / "DirectGetBufferSubData@EBO" / ""
    std::string m_sourceKind;

    /// Relative path to the buffer metadata JSON file
    std::string m_metadataPath;

    /// Relative path to the binary data file (empty if no data)
    std::string m_binaryDataPath;
};

#endif // BUFFER_H
