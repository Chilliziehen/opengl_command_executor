/**
 * @file ResourceAllocator.h
 * @brief Creates GPU objects (glGen*) and registers captureId→GLHandle in ResourceMapper
 */
#ifndef RESOURCE_ALLOCATOR_H
#define RESOURCE_ALLOCATOR_H

#include <cstdint>
#include <string>

struct CaptureBuffer;
struct TextureWrapper;
struct CaptureShader;
struct CaptureProgram;
struct CaptureVertexArrayObject;
struct CaptureFramebuffer;
class  FrameCapture;

class ResourceAllocator {
public:
    static uint32_t allocateBuffer(const CaptureBuffer& metadata);
    static uint32_t allocateTexture(const TextureWrapper& metadata);
    static uint32_t allocateShader(const CaptureShader& metadata);
    static uint32_t allocateProgram(const CaptureProgram& metadata);
    static uint32_t allocateVertexArray(const CaptureVertexArrayObject& metadata);
    static uint32_t allocateFramebuffer(const CaptureFramebuffer& metadata);

    static void deleteBuffer(uint32_t captureId);
    static void deleteTexture(uint32_t captureId);
    static void deleteShader(uint32_t captureId);
    static void deleteProgram(uint32_t captureId);
    static void deleteVertexArray(uint32_t captureId);
    static void deleteFramebuffer(uint32_t captureId);

    static bool allocateAllResources(const FrameCapture& capture, std::string& outError);

    /// Delete every GL object created for this capture and clear all id→handle
    /// mappings (including the synthetic default VAO id 0). Call on teardown or
    /// before loading a different capture so nothing leaks / goes stale.
    static void deleteAllResources(const FrameCapture& capture);

private:
    ResourceAllocator() = delete;
};

#endif
