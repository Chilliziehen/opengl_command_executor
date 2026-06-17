/**
 * @file Program.h
 * @brief Capture program descriptor — maps to entries in programs.json
 */

#ifndef PROGRAM_H
#define PROGRAM_H

#include <cstdint>
#include <string>
#include <unordered_map>
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

    /// uniform block index → UBO binding point (v2 captures). When present, the
    /// replayer applies glUniformBlockBinding with these instead of guessing.
    /// Empty = not captured (fall back to index==binding heuristic).
    std::unordered_map<uint32_t, uint32_t> m_uniformBlockBindings;
};

#endif // PROGRAM_H
