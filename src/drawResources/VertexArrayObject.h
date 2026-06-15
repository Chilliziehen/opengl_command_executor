/**
 * @file VertexArrayObject.h
 * @brief Capture VAO descriptor — maps to entries in vaos.json
 */

#ifndef VERTEX_ARRAY_OBJECT_H
#define VERTEX_ARRAY_OBJECT_H

#include <cstdint>
#include <string>
#include <unordered_map>

/// Single vertex attribute descriptor within a VAO
struct VertexAttribute {
    /// Whether this attribute array is enabled
    bool m_enabled = false;

    /// Number of components (1-4)
    uint32_t m_componentCount = 0;

    /// GL component type enum (5126=GL_FLOAT, etc.)
    uint32_t m_componentType = 0;

    /// Whether fixed-point values are normalized
    bool m_normalized = false;

    /// Vertex stride in bytes
    uint32_t m_stride = 0;

    /// Attribute offset in bytes from the start of the vertex
    uint32_t m_offset = 0;

    /// ID of the VBO this attribute sources data from
    uint32_t m_bufferId = 0;

    /// Attribute name from glGetActiveAttrib (may be empty)
    std::string m_attributeName;
};

/// VAO descriptor matching an entry in the vaos.json array
struct CaptureVertexArrayObject {
    /// Unique VAO identifier (0 = default VAO)
    uint32_t m_vertexArrayId = 0;

    /// Human-readable label, e.g. "VAO#1" or "VAO#0 (default)"
    std::string m_label;

    /// Map from attribute index (string key) to attribute descriptor
    std::unordered_map<std::string, VertexAttribute> m_vertexAttributes;

    /// ID of the bound element array buffer (0 = no EBO)
    uint32_t m_elementArrayBufferId = 0;

    /// Whether the VAO has been deleted
    bool m_deleted = false;
};

#endif // VERTEX_ARRAY_OBJECT_H
