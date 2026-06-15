/**
 * @file Shader.h
 * @brief Capture shader descriptor — maps to entries in shaders.json
 */

#ifndef SHADER_H
#define SHADER_H

#include <cstdint>
#include <string>

/// Shader descriptor matching an entry in the shaders.json array
struct CaptureShader {
    /// Unique shader identifier
    uint32_t m_shaderId = 0;

    /// Human-readable label, e.g. "Shader#1"
    std::string m_label;

    /// GL shader type enum: 35633=GL_VERTEX_SHADER, 35632=GL_FRAGMENT_SHADER
    uint32_t m_shaderType = 0;

    /// Complete GLSL ES 3.0 source code
    std::string m_source;
};

#endif // SHADER_H
