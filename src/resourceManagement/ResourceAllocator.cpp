#include "ResourceAllocator.h"
#include "mappers/UniversalDMapper.h"
#include "framecapture/FrameCapture.h"
#include "drawResources/Buffer.h"
#include "drawResources/Texture.h"
#include "drawResources/Shader.h"
#include "drawResources/Program.h"
#include "drawResources/VertexArrayObject.h"
#include "drawResources/Framebuffer.h"
#include <glad/gl.h>

uint32_t ResourceAllocator::allocateBuffer(const CaptureBuffer& metadata) {
    GLuint handle = 0;
    glGenBuffers(1, &handle);
    addMappedHandle(ResourceKind::Buffer, metadata.m_bufferId, handle);
    return handle;
}

uint32_t ResourceAllocator::allocateTexture(const TextureWrapper& metadata) {
    GLuint handle = 0;
    glGenTextures(1, &handle);
    addMappedHandle(ResourceKind::Texture, metadata.m_textureId, handle);
    return handle;
}

uint32_t ResourceAllocator::allocateShader(const CaptureShader& metadata) {
    GLuint handle = glCreateShader(static_cast<GLenum>(metadata.m_shaderType));
    addMappedHandle(ResourceKind::Shader, metadata.m_shaderId, handle);
    return handle;
}

uint32_t ResourceAllocator::allocateProgram(const CaptureProgram& metadata) {
    GLuint handle = glCreateProgram();
    addMappedHandle(ResourceKind::Program, metadata.m_programId, handle);
    return handle;
}

uint32_t ResourceAllocator::allocateVertexArray(const CaptureVertexArrayObject& metadata) {
    GLuint handle = 0;
    glGenVertexArrays(1, &handle);
    addMappedHandle(ResourceKind::VertexArray, metadata.m_vertexArrayId, handle);
    return handle;
}

uint32_t ResourceAllocator::allocateFramebuffer(const CaptureFramebuffer& metadata) {
    GLuint handle = 0;
    glGenFramebuffers(1, &handle);
    addMappedHandle(ResourceKind::Framebuffer, metadata.m_framebufferId, handle);
    return handle;
}

void ResourceAllocator::deleteBuffer(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::Buffer, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::Buffer, captureId);
    glDeleteBuffers(1, &handle);
    removeMappedHandle(ResourceKind::Buffer, captureId);
}

void ResourceAllocator::deleteTexture(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::Texture, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::Texture, captureId);
    glDeleteTextures(1, &handle);
    removeMappedHandle(ResourceKind::Texture, captureId);
}

void ResourceAllocator::deleteShader(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::Shader, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::Shader, captureId);
    glDeleteShader(handle);
    removeMappedHandle(ResourceKind::Shader, captureId);
}

void ResourceAllocator::deleteProgram(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::Program, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::Program, captureId);
    glDeleteProgram(handle);
    removeMappedHandle(ResourceKind::Program, captureId);
}

void ResourceAllocator::deleteVertexArray(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::VertexArray, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::VertexArray, captureId);
    glDeleteVertexArrays(1, &handle);
    removeMappedHandle(ResourceKind::VertexArray, captureId);
}

void ResourceAllocator::deleteFramebuffer(uint32_t captureId) {
    if (!hasMappedHandle(ResourceKind::Framebuffer, captureId)) return;
    GLuint handle = getMappedHandle(ResourceKind::Framebuffer, captureId);
    glDeleteFramebuffers(1, &handle);
    removeMappedHandle(ResourceKind::Framebuffer, captureId);
}

bool ResourceAllocator::allocateAllResources(const FrameCapture& capture, std::string& /*outError*/) {
    for (const auto& buffer : capture.m_buffers)
        allocateBuffer(buffer);
    for (const auto& texture : capture.m_textures)
        allocateTexture(texture);
    for (const auto& shader : capture.m_shaders)
        allocateShader(shader);
    for (const auto& program : capture.m_programs)
        allocateProgram(program);
    for (const auto& vertexArray : capture.m_vertexArrays)
        allocateVertexArray(vertexArray);
    for (const auto& framebuffer : capture.m_framebuffers)
        allocateFramebuffer(framebuffer);
    return true;
}

void ResourceAllocator::deleteAllResources(const FrameCapture& capture) {
    for (const auto& buffer : capture.m_buffers)
        deleteBuffer(buffer.m_bufferId);
    for (const auto& texture : capture.m_textures)
        deleteTexture(texture.m_textureId);
    for (const auto& shader : capture.m_shaders)
        deleteShader(shader.m_shaderId);
    for (const auto& program : capture.m_programs)
        deleteProgram(program.m_programId);
    for (const auto& vertexArray : capture.m_vertexArrays)
        deleteVertexArray(vertexArray.m_vertexArrayId);
    for (const auto& framebuffer : capture.m_framebuffers)
        deleteFramebuffer(framebuffer.m_framebufferId);

    // The synthetic default VAO (id 0) is created lazily during init and isn't
    // in any capture list, so delete it explicitly.
    deleteVertexArray(0);

    // Drop any remaining mappings (e.g. resources created by in-frame commands).
    clearAllMappedHandles();
}
