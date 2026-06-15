/**
 * @file Command.h
 * @brief Polymorphic GL command hierarchy — each subclass::execute() replays one GL call
 */
#ifndef COMMAND_H
#define COMMAND_H

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// ---- retained data payload types ----
struct UniformDataPayload {
    std::string m_arrayType;
    std::vector<double> m_data;
};
struct UploadReference {
    std::string m_uploadPath;
    uint64_t    m_byteLength = 0;
};
using CommandDataArgument = std::variant<UniformDataPayload, UploadReference, std::monostate>;

// ---- base class ----
class Command {
public:
    Command(uint32_t eventId, std::string commandName)
        : m_eventId(eventId), m_commandName(std::move(commandName)) {}
    virtual ~Command() = default;

    uint32_t            getEventId()     const { return m_eventId; }
    const std::string&  getCommandName() const { return m_commandName; }

    virtual void execute() = 0;

    static void setCaptureDirectory(const std::string& path) { s_captureDirectory = path; }

    // Depth-texture registry (GL handles) used by the per-draw sampler fixup to
    // route sampler2DShadow uniforms to depth texture units.
    static void clearDepthTextureRegistry();
    static void registerDepthTexture(uint32_t glHandle);
    static bool isDepthTexture(uint32_t glHandle);

protected:
    static const std::string& captureDirectory() { return s_captureDirectory; }

private:
    uint32_t    m_eventId;
    std::string m_commandName;
    static std::string s_captureDirectory;
};

using CommandPtr = std::unique_ptr<Command>;

// ---- state commands (no mapper lookup needed) ----
class ViewportCommand final : public Command {
public:
    ViewportCommand(uint32_t eventId, std::vector<int32_t> bounds);
    void execute() override;
private:
    std::vector<int32_t> m_bounds;
};

class EnableCommand final : public Command {
public:
    EnableCommand(uint32_t eventId, uint32_t capability, bool enable);
    void execute() override;
private:
    uint32_t m_capability;
    bool     m_enable;
};

class ClearColorCommand final : public Command {
public:
    ClearColorCommand(uint32_t eventId, float r, float g, float b, float a);
    void execute() override;
private:
    float m_red, m_green, m_blue, m_alpha;
};

class ClearCommand final : public Command {
public:
    ClearCommand(uint32_t eventId, uint32_t mask, uint32_t framebufferId);
    void execute() override;
private:
    uint32_t m_mask;
    uint32_t m_framebufferId;
};

class ScissorCommand final : public Command {
public:
    ScissorCommand(uint32_t eventId, std::vector<int32_t> box);
    void execute() override;
private:
    std::vector<int32_t> m_box;
};

class BlendFunctionCommand final : public Command {
public:
    BlendFunctionCommand(uint32_t eventId, uint32_t sourceFactor, uint32_t destinationFactor);
    void execute() override;
private:
    uint32_t m_sourceFactor, m_destinationFactor;
};

class BlendEquationCommand final : public Command {
public:
    BlendEquationCommand(uint32_t eventId, uint32_t mode);
    void execute() override;
private:
    uint32_t m_mode;
};

class CullFaceCommand final : public Command {
public:
    CullFaceCommand(uint32_t eventId, uint32_t mode);
    void execute() override;
private:
    uint32_t m_mode;
};

class FrontFaceCommand final : public Command {
public:
    FrontFaceCommand(uint32_t eventId, uint32_t orientation);
    void execute() override;
private:
    uint32_t m_orientation;
};

class DepthFunctionCommand final : public Command {
public:
    DepthFunctionCommand(uint32_t eventId, uint32_t function);
    void execute() override;
private:
    uint32_t m_function;
};

class DepthMaskCommand final : public Command {
public:
    DepthMaskCommand(uint32_t eventId, bool enabled);
    void execute() override;
private:
    bool m_enabled;
};

class ColorMaskCommand final : public Command {
public:
    ColorMaskCommand(uint32_t eventId, bool red, bool green, bool blue, bool alpha);
    void execute() override;
private:
    bool m_red, m_green, m_blue, m_alpha;
};

class LineWidthCommand final : public Command {
public:
    LineWidthCommand(uint32_t eventId, float width);
    void execute() override;
private:
    float m_width;
};

class StencilFunctionCommand final : public Command {
public:
    StencilFunctionCommand(uint32_t eventId, uint32_t function, int32_t reference, uint32_t mask);
    void execute() override;
private:
    uint32_t m_function;
    int32_t  m_reference;
    uint32_t m_mask;
};

class StencilOperationCommand final : public Command {
public:
    StencilOperationCommand(uint32_t eventId, uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass);
    void execute() override;
private:
    uint32_t m_stencilFail, m_depthFail, m_depthPass;
};

class StencilMaskCommand final : public Command {
public:
    StencilMaskCommand(uint32_t eventId, uint32_t mask);
    void execute() override;
private:
    uint32_t m_mask;
};

class BlendColorCommand final : public Command {
public:
    BlendColorCommand(uint32_t eventId, float r, float g, float b, float a);
    void execute() override;
private:
    float m_red, m_green, m_blue, m_alpha;
};

// ---- resource-binding commands (mapper lookup needed) ----
class UseProgramCommand final : public Command {
public:
    UseProgramCommand(uint32_t eventId, uint32_t programId);
    void execute() override;
private:
    uint32_t m_programId;
};

class BindFramebufferCommand final : public Command {
public:
    BindFramebufferCommand(uint32_t eventId, uint32_t target, uint32_t framebufferId);
    void execute() override;
private:
    uint32_t m_target;
    uint32_t m_framebufferId;
};

class BindBufferCommand final : public Command {
public:
    BindBufferCommand(uint32_t eventId, uint32_t target, uint32_t bufferId);
    void execute() override;
private:
    uint32_t m_target;
    uint32_t m_bufferId;
};

class BindBufferBaseCommand final : public Command {
public:
    BindBufferBaseCommand(uint32_t eventId, uint32_t target, uint32_t index, uint32_t bufferId);
    void execute() override;
private:
    uint32_t m_target, m_index, m_bufferId;
};

class BindBufferRangeCommand final : public Command {
public:
    BindBufferRangeCommand(uint32_t eventId, uint32_t target, uint32_t index,
                           uint32_t bufferId, uint64_t offset, uint64_t size);
    void execute() override;
private:
    uint32_t m_target, m_index, m_bufferId;
    uint64_t m_offset, m_size;
};

class BindTextureCommand final : public Command {
public:
    BindTextureCommand(uint32_t eventId, uint32_t target, uint32_t textureId, uint32_t unit);
    void execute() override;

    uint32_t target() const    { return m_target; }
    uint32_t textureId() const { return m_textureId; }
    uint32_t unit() const      { return m_unit; }
private:
    uint32_t m_target, m_textureId, m_unit;
};

class ActiveTextureCommand final : public Command {
public:
    ActiveTextureCommand(uint32_t eventId, uint32_t unit);
    void execute() override;
private:
    uint32_t m_unit;
};

class BindVertexArrayCommand final : public Command {
public:
    BindVertexArrayCommand(uint32_t eventId, uint32_t vertexArrayId);
    void execute() override;
private:
    uint32_t m_vertexArrayId;
};

class VertexAttribPointerCommand final : public Command {
public:
    VertexAttribPointerCommand(uint32_t eventId, uint32_t attributeIndex, uint32_t componentCount,
                               uint32_t componentType, bool normalized, uint32_t stride,
                               uint32_t offset, uint32_t bufferId, std::string attributeName);
    void execute() override;

    uint32_t           attributeIndex() const { return m_attributeIndex; }
    const std::string& attributeName() const  { return m_attributeName; }
private:
    uint32_t    m_attributeIndex, m_componentCount, m_componentType;
    bool        m_normalized;
    uint32_t    m_stride, m_offset, m_bufferId;
    std::string m_attributeName;
};

class VertexAttribEnableCommand final : public Command {
public:
    VertexAttribEnableCommand(uint32_t eventId, uint32_t attributeIndex, bool enable);
    void execute() override;
private:
    uint32_t m_attributeIndex;
    bool     m_enable;
};

class VertexAttribDivisorCommand final : public Command {
public:
    VertexAttribDivisorCommand(uint32_t eventId, uint32_t attributeIndex, uint32_t divisor);
    void execute() override;
private:
    uint32_t m_attributeIndex, m_divisor;
};

// ---- draw commands ----
class DrawElementsCommand final : public Command {
public:
    DrawElementsCommand(uint32_t eventId, uint32_t drawMode, uint32_t indexCount,
                        uint32_t indexType, uint32_t indexOffset, uint32_t framebufferId);
    void execute() override;
private:
    uint32_t m_drawMode, m_indexCount, m_indexType, m_indexOffset, m_framebufferId;
};

class DrawArraysCommand final : public Command {
public:
    DrawArraysCommand(uint32_t eventId, uint32_t drawMode, uint32_t firstVertex,
                      uint32_t vertexCount, uint32_t framebufferId);
    void execute() override;
private:
    uint32_t m_drawMode, m_firstVertex, m_vertexCount, m_framebufferId;
};

// ---- resource lifecycle ----
class CreateResourceCommand final : public Command {
public:
    enum class ResourceKind { Buffer, Texture, VertexArray, Framebuffer, Shader, Program };
    CreateResourceCommand(uint32_t eventId, ResourceKind kind, uint32_t resourceId, std::string name);
    void execute() override;
private:
    ResourceKind m_resourceKind;
    uint32_t     m_resourceId;
};

class DeleteResourceCommand final : public Command {
public:
    using ResourceKind = CreateResourceCommand::ResourceKind;
    DeleteResourceCommand(uint32_t eventId, ResourceKind kind, uint32_t resourceId, std::string name);
    void execute() override;
private:
    ResourceKind m_resourceKind;
    uint32_t     m_resourceId;
};

// ---- data upload commands ----
class BufferDataCommand final : public Command {
public:
    BufferDataCommand(uint32_t eventId, uint32_t target, uint32_t bufferId,
                      uint64_t offset, CommandDataArgument data, std::string commandName);
    void execute() override;
private:
    uint32_t            m_target, m_bufferId;
    uint64_t            m_offset;
    CommandDataArgument m_data;
};

class TextureImageCommand final : public Command {
public:
    TextureImageCommand(uint32_t eventId, uint32_t target, uint32_t mipmapLevel,
                        uint32_t internalFormat, uint32_t width, uint32_t height,
                        uint32_t format, uint32_t pixelType, uint32_t textureId,
                        CommandDataArgument data, std::string commandName,
                        uint32_t xOffset = 0, uint32_t yOffset = 0);
    void execute() override;
private:
    uint32_t m_target, m_mipmapLevel, m_internalFormat, m_width, m_height;
    uint32_t m_format, m_pixelType, m_textureId;
    uint32_t m_xOffset, m_yOffset;
    CommandDataArgument m_data;
};

class GenerateMipmapCommand final : public Command {
public:
    GenerateMipmapCommand(uint32_t eventId, uint32_t target, uint32_t textureId);
    void execute() override;
private:
    uint32_t m_target, m_textureId;
};

// ---- shader / program commands ----
class ShaderSourceCommand final : public Command {
public:
    ShaderSourceCommand(uint32_t eventId, uint32_t shaderId, std::string source);
    void execute() override;
private:
    uint32_t    m_shaderId;
    std::string m_source;
};

class AttachShaderCommand final : public Command {
public:
    AttachShaderCommand(uint32_t eventId, uint32_t programId, uint32_t shaderId);
    void execute() override;
private:
    uint32_t m_programId, m_shaderId;
};

class LinkProgramCommand final : public Command {
public:
    LinkProgramCommand(uint32_t eventId, uint32_t programId);
    void execute() override;
private:
    uint32_t m_programId;
};

// ---- uniform commands ----
class UniformCommand final : public Command {
public:
    UniformCommand(uint32_t eventId, std::string uniformName, uint32_t programId,
                   bool valueOmitted, std::string valueOmittedReason, bool isSnapshot,
                   CommandDataArgument data, std::string commandName);
    void execute() override;
private:
    std::string m_uniformName;
    uint32_t    m_programId;
    bool        m_valueOmitted, m_isSnapshot;
    std::string m_valueOmittedReason;
    CommandDataArgument m_data;
};

class UniformMatrixCommand final : public Command {
public:
    UniformMatrixCommand(uint32_t eventId, std::string uniformName, uint32_t programId,
                         bool transpose, bool valueOmitted, CommandDataArgument data);
    void execute() override;
private:
    std::string m_uniformName;
    uint32_t    m_programId;
    bool        m_transpose, m_valueOmitted;
    CommandDataArgument m_data;
};

class UniformSamplerCommand final : public Command {
public:
    UniformSamplerCommand(uint32_t eventId, std::string uniformName, uint32_t programId,
                          uint32_t textureUnit, bool valueOmitted);
    void execute() override;
private:
    std::string m_uniformName;
    uint32_t    m_programId, m_textureUnit;
    bool        m_valueOmitted;
};

// ---- framebuffer commands ----
class FramebufferTexture2DCommand final : public Command {
public:
    FramebufferTexture2DCommand(uint32_t eventId, uint32_t framebufferId, uint32_t attachment,
                                uint32_t textureTarget, uint32_t textureId, uint32_t mipmapLevel);
    void execute() override;
private:
    uint32_t m_framebufferId, m_attachment, m_textureTarget, m_textureId, m_mipmapLevel;
};

class BlitFramebufferCommand final : public Command {
public:
    BlitFramebufferCommand(uint32_t eventId, int32_t sourceX0, int32_t sourceY0,
                           int32_t sourceX1, int32_t sourceY1, int32_t destinationX0,
                           int32_t destinationY0, int32_t destinationX1, int32_t destinationY1,
                           uint32_t mask, uint32_t filter);
    void execute() override;
private:
    int32_t  m_sourceX0, m_sourceY0, m_sourceX1, m_sourceY1;
    int32_t  m_destinationX0, m_destinationY0, m_destinationX1, m_destinationY1;
    uint32_t m_mask, m_filter;
};

// ---- no-op fallback ----
class NoOpCommand final : public Command {
public:
    NoOpCommand(uint32_t eventId, std::string name);
    void execute() override {}
};

#endif
