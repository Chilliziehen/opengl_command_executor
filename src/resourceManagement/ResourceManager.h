/**
 * @file ResourceManager.h
 * @brief Uploads binary data / shader sources to GPU objects created by ResourceAllocator
 */
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <cstdint>
#include <string>
#include <unordered_map>

struct CaptureBuffer;
struct TextureWrapper;
struct CaptureShader;
struct CaptureProgram;
struct CaptureState;
class  FrameCapture;

class ResourceManager {
public:
    static bool uploadBufferData(const CaptureBuffer& metadata,
                                 const std::string& captureDirectoryPath,
                                 std::string& outError);
    static bool uploadTextureData(const TextureWrapper& metadata,
                                  const std::string& captureDirectoryPath,
                                  bool isRenderTarget,
                                  std::string& outError);
    static bool compileShader(const CaptureShader& metadata,
                              std::string& outError);
    static bool linkProgram(const CaptureProgram& metadata,
                            const std::unordered_map<std::string, uint32_t>& attributeBindings,
                            std::string& outError);

    /// Restore GL state from the frame-start snapshot (viewport, blend, depth, etc.)
    /// Takes the whole capture so texture bindings can use each texture's real
    /// target (e.g. cube maps must not be bound to GL_TEXTURE_2D).
    static bool restoreState(const FrameCapture& capture,
                             std::string& outError);

    /// Bind every VAO, set up its vertex attributes, and bind its EBO.
    static bool restoreVertexArrays(const FrameCapture& capture,
                                    std::string& outError);

    /// Bind every FBO and attach textures to its attachment points.
    static bool restoreFramebuffers(const FrameCapture& capture,
                                    std::string& outError);

    /// Full pipeline: buffers → textures → shaders → programs → state → FBOs → VAOs
    static bool uploadAllResources(const FrameCapture& capture,
                                   const std::string& captureDirectoryPath,
                                   std::string& outError);

private:
    ResourceManager() = delete;
};

#endif
