/**
 * @file Program.h
 * @brief Capture program descriptor — maps to entries in programs.json
 */

#ifndef PROGRAM_H
#define PROGRAM_H

#include <cstdint>
#include <string>
#include <vector>

/// Program descriptor matching an entry in the programs.json array
struct CaptureProgram {
    /// Unique program identifier
    uint32_t m_programId = 0;

    /// Human-readable label, e.g. "Program#1"
    std::string m_label;

    /// Ordered list of attached shader IDs: [vertexShaderId, fragmentShaderId]
    std::vector<uint32_t> m_attachedShaderIds;

    /// Whether the program linked successfully
    bool m_linked = false;
};

#endif // PROGRAM_H
