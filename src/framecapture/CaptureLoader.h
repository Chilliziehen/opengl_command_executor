/**
 * @file CaptureLoader.h
 * @brief Loads a capture directory into a FrameCapture object
 */

#ifndef CAPTURE_LOADER_H
#define CAPTURE_LOADER_H

#include <string>
#include "FrameCapture.h"

/// Reads a capture directory (containing manifest.json + resource files)
/// and populates a FrameCapture with all parsed resources.
class CaptureLoader {
public:
    /// Load the capture at `captureDirectoryPath` into `outCapture`.
    /// Returns true on success, false with details written to `outErrorMessage`.
    static bool loadFromDirectory(const std::string& captureDirectoryPath,
                                  FrameCapture&        outCapture,
                                  std::string&         outErrorMessage);

private:
    CaptureLoader() = delete;

    // ---- resource parsers (called from loadFromDirectory) ----
    static bool parseManifest(const std::string& directoryPath,
                              CaptureManifest&   outManifest,
                              std::string&       outError);
    static bool parseBuffers(const std::string&     directoryPath,
                             const CaptureManifest& manifest,
                             std::vector<CaptureBuffer>& outBuffers,
                             std::string&                outError);
    static bool parseTextures(const std::string&        directoryPath,
                              const CaptureManifest&    manifest,
                              std::vector<TextureWrapper>& outTextures,
                              std::string&                outError);
    static bool parseShaders(const std::string&     directoryPath,
                             const CaptureManifest& manifest,
                             std::vector<CaptureShader>& outShaders,
                             std::string&                outError);
    static bool parsePrograms(const std::string&       directoryPath,
                              const CaptureManifest&   manifest,
                              std::vector<CaptureProgram>& outPrograms,
                              std::string&                outError);
    static bool parseVertexArrays(const std::string&     directoryPath,
                                  const CaptureManifest& manifest,
                                  std::vector<CaptureVertexArrayObject>& outVertexArrays,
                                  std::string&                         outError);
    static bool parseFramebuffers(const std::string&     directoryPath,
                                  const CaptureManifest& manifest,
                                  std::vector<CaptureFramebuffer>& outFramebuffers,
                                  std::string&                   outError);
    static bool parseState(const std::string&     directoryPath,
                           const CaptureManifest& manifest,
                           CaptureState&          outState,
                           std::string&           outError);
    static bool parseCommands(const std::string&     directoryPath,
                              const CaptureManifest& manifest,
                              std::vector<Command>&  outCommands,
                              std::string&           outError);
};

#endif // CAPTURE_LOADER_H
