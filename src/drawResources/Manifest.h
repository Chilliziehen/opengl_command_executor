/**
 * @file Manifest.h
 * @brief Capture manifest — maps to manifest.json
 */

#ifndef MANIFEST_H
#define MANIFEST_H

#include <cstdint>
#include <string>
#include <vector>
#include "CaptureTypes.h"

/// Replayer compatibility information
struct ReplayerCompatibility {
    bool        m_currentStandaloneReplayer = false;
    bool        m_hasPreviewPayload         = false;
    std::string m_reason;
};

/// File index embedded in manifest — lists all resource files in the capture
struct ManifestFileIndex {
    /// Relative path to commands.json
    std::string m_commandsPath;

    /// Relative path to state.json
    std::string m_statePath;

    /// Relative path to shaders.json
    std::string m_shadersPath;

    /// Relative path to programs.json
    std::string m_programsPath;

    /// Relative path to framebuffers.json
    std::string m_framebuffersPath;

    /// Relative path to vaos.json
    std::string m_vertexArraysPath;

    /// Relative path to debug_log.json
    std::string m_debugLogPath;

    /// Relative path to runtime_logs.json
    std::string m_runtimeLogsPath;

    /// Buffer resource entries (metadata + binary paths)
    std::vector<ResourceFileEntry> m_buffers;

    /// Texture resource entries (metadata + binary paths)
    std::vector<ResourceFileEntry> m_textures;

    /// Upload entries (binary path + byte length); may be empty
    std::vector<ResourceFileEntry> m_uploads;
};

/// Full capture manifest matching the schema manifest.json
struct CaptureManifest {
    /// Format identifier, always "tjrelic.wx.capture"
    std::string m_format;

    /// Schema version (currently 2)
    uint32_t m_version = 0;

    /// Capture model identifier
    std::string m_captureModel;

    /// ISO 8601 timestamp of capture generation
    std::string m_generatedAt;

    /// Captured frame index
    uint32_t m_frameIndex = 0;

    /// Canvas descriptor
    CanvasDescriptor m_canvas;

    /// Replayer compatibility flags
    ReplayerCompatibility m_replayerCompatibility;

    /// Diagnostics from the capture session
    DiagnosticsInfo m_diagnostics;

    /// Project root directory (may be empty)
    std::string m_projectRoot;

    /// Sandbox base URL
    std::string m_sandboxRoot;

    /// File index — maps resource type to file paths
    ManifestFileIndex m_files;
};

#endif // MANIFEST_H
