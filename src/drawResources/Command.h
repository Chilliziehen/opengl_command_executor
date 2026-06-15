/**
 * @file Command.h
 * @brief GL command stream structures — maps to entries in commands.json
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

/// GPU data reference — inline array or external upload file
struct UniformDataPayload {
    /// JavaScript typed-array type name: "Float32Array", "Int32Array", etc.
    std::string m_arrayType;

    /// Inline numeric data
    std::vector<double> m_data;
};

/// Reference to an external upload binary file
struct UploadReference {
    std::string m_uploadPath;
    uint64_t    m_byteLength = 0;
};

/// Variant for data arguments that can be inline or external
using CommandDataArgument = std::variant<UniformDataPayload, UploadReference, std::monostate>;

/// Base command header present in every command entry
struct CommandHeader {
    /// Monotonically increasing event sequence number
    uint32_t m_eventId = 0;

    /// GL command name, e.g. "viewport", "drawElements", "uniformMatrix4fv"
    std::string m_commandName;
};

// --- Specific command types with typed extra fields ---

struct ViewportCommand {
    std::vector<int32_t> m_bounds; // [x, y, width, height]
};

struct EnableCommand {
    uint32_t m_capability = 0; // GLenum
};

struct ClearCommand {
    uint32_t m_mask          = 0;
    uint32_t m_framebufferId = 0;
};

struct UseProgramCommand {
    uint32_t m_programId = 0;
};

struct UniformCommand {
    uint32_t    m_programId  = 0;
    std::string m_uniformName;
    bool        m_valueOmitted       = false;
    std::string m_valueOmittedReason;
    bool        m_isSnapshot         = false;
    CommandDataArgument m_data;
};

struct UniformMatrixCommand {
    uint32_t    m_programId  = 0;
    std::string m_uniformName;
    bool        m_transpose  = false;
    bool        m_valueOmitted = false;
    CommandDataArgument m_data;
};

struct UniformSamplerCommand {
    uint32_t    m_textureUnit = 0;
    uint32_t    m_programId   = 0;
    std::string m_uniformName;
    bool        m_valueOmitted = false;
};

struct BindVertexArrayCommand {
    uint32_t m_vertexArrayId = 0;
};

struct DrawElementsCommand {
    uint32_t m_drawMode      = 0; // 4=GL_TRIANGLES
    uint32_t m_indexCount    = 0;
    uint32_t m_indexType     = 0; // 5123=GL_UNSIGNED_SHORT
    uint32_t m_indexOffset   = 0;
    uint32_t m_framebufferId = 0;
};

struct DrawArraysCommand {
    uint32_t m_drawMode      = 0;
    uint32_t m_firstVertex   = 0;
    uint32_t m_vertexCount   = 0;
    uint32_t m_framebufferId = 0;
};

struct BindFramebufferCommand {
    uint32_t m_target        = 0;
    uint32_t m_framebufferId = 0;
};

struct BindBufferCommand {
    uint32_t m_target   = 0;
    uint32_t m_bufferId = 0;
};

struct BindBufferBaseCommand {
    uint32_t m_target   = 0;
    uint32_t m_index    = 0;
    uint32_t m_bufferId = 0;
};

struct BindBufferRangeCommand {
    uint32_t m_target   = 0;
    uint32_t m_index    = 0;
    uint32_t m_bufferId = 0;
    uint64_t m_offset   = 0;
    uint64_t m_size     = 0;
};

struct BindTextureCommand {
    uint32_t m_target    = 0;
    uint32_t m_textureId = 0;
    uint32_t m_unit      = 0;
};

struct ActiveTextureCommand {
    uint32_t m_unit = 0;
};

struct VertexAttribPointerCommand {
    uint32_t    m_attributeIndex  = 0;
    uint32_t    m_componentCount  = 0;
    uint32_t    m_componentType   = 0;
    bool        m_normalized      = false;
    uint32_t    m_stride          = 0;
    uint32_t    m_offset          = 0;
    uint32_t    m_bufferId        = 0;
    std::string m_attributeName;
};

struct VertexAttribEnableCommand {
    uint32_t m_attributeIndex = 0;
};

struct VertexAttribDivisorCommand {
    uint32_t m_attributeIndex = 0;
    uint32_t m_divisor        = 0;
};

struct CreateResourceCommand {
    uint32_t m_resourceId = 0;
};

struct DeleteResourceCommand {
    uint32_t m_resourceId = 0;
};

struct BufferDataCommand {
    uint32_t              m_target   = 0;
    uint32_t              m_bufferId = 0;
    uint64_t              m_offset   = 0;
    CommandDataArgument   m_data;
};

struct TextureImageCommand {
    uint32_t              m_target         = 0;
    uint32_t              m_mipmapLevel    = 0;
    uint32_t              m_internalFormat = 0;
    uint32_t              m_width          = 0;
    uint32_t              m_height         = 0;
    uint32_t              m_format         = 0;
    uint32_t              m_pixelType      = 0;
    uint32_t              m_textureId      = 0;
    uint32_t              m_unit           = 0;
    CommandDataArgument   m_data;
};

struct GenerateMipmapCommand {
    uint32_t m_target    = 0;
    uint32_t m_textureId = 0;
};

struct ShaderSourceCommand {
    uint32_t    m_shaderId = 0;
    std::string m_source;
};

struct AttachShaderCommand {
    uint32_t m_programId = 0;
    uint32_t m_shaderId  = 0;
};

struct LinkProgramCommand {
    uint32_t m_programId = 0;
};

struct FramebufferTexture2DCommand {
    uint32_t m_framebufferId = 0;
    uint32_t m_attachment    = 0;
    uint32_t m_textureTarget = 0;
    uint32_t m_textureId     = 0;
    uint32_t m_mipmapLevel   = 0;
};

struct ScissorCommand {
    std::vector<int32_t> m_box; // [x, y, width, height]
};

struct BlendFunctionCommand {
    uint32_t m_sourceFactor      = 0;
    uint32_t m_destinationFactor = 0;
};

struct BlendEquationCommand {
    uint32_t m_mode = 0;
};

struct CullFaceCommand {
    uint32_t m_mode = 0;
};

struct FrontFaceCommand {
    uint32_t m_orientation = 0;
};

struct DepthFunctionCommand {
    uint32_t m_function = 0;
};

struct DepthMaskCommand {
    bool m_enabled = true;
};

struct ColorMaskCommand {
    bool m_red   = true;
    bool m_green = true;
    bool m_blue  = true;
    bool m_alpha = true;
};

struct LineWidthCommand {
    float m_width = 1.0f;
};

struct StencilFunctionCommand {
    uint32_t m_function  = 0;
    int32_t  m_reference = 0;
    uint32_t m_mask      = 0;
};

struct StencilOperationCommand {
    uint32_t m_stencilFail      = 0;
    uint32_t m_depthFail        = 0;
    uint32_t m_depthPass        = 0;
};

struct StencilMaskCommand {
    uint32_t m_mask = 0;
};

struct BlendColorCommand {
    float m_red   = 0.0f;
    float m_green = 0.0f;
    float m_blue  = 0.0f;
    float m_alpha = 0.0f;
};

struct BlitFramebufferCommand {
    int32_t  m_sourceX0      = 0;
    int32_t  m_sourceY0      = 0;
    int32_t  m_sourceX1      = 0;
    int32_t  m_sourceY1      = 0;
    int32_t  m_destinationX0 = 0;
    int32_t  m_destinationY0 = 0;
    int32_t  m_destinationX1 = 0;
    int32_t  m_destinationY1 = 0;
    uint32_t m_mask          = 0;
    uint32_t m_filter        = 0;
};

/// Variant over all command-specific data types
using CommandPayload = std::variant<
    std::monostate,                  // commands with no extra data beyond args
    ViewportCommand,
    EnableCommand,
    ClearCommand,
    UseProgramCommand,
    UniformCommand,
    UniformMatrixCommand,
    UniformSamplerCommand,
    BindVertexArrayCommand,
    DrawElementsCommand,
    DrawArraysCommand,
    BindFramebufferCommand,
    BindBufferCommand,
    BindBufferBaseCommand,
    BindBufferRangeCommand,
    BindTextureCommand,
    ActiveTextureCommand,
    VertexAttribPointerCommand,
    VertexAttribEnableCommand,
    VertexAttribDivisorCommand,
    CreateResourceCommand,
    DeleteResourceCommand,
    BufferDataCommand,
    TextureImageCommand,
    GenerateMipmapCommand,
    ShaderSourceCommand,
    AttachShaderCommand,
    LinkProgramCommand,
    FramebufferTexture2DCommand,
    ScissorCommand,
    BlendFunctionCommand,
    BlendEquationCommand,
    CullFaceCommand,
    FrontFaceCommand,
    DepthFunctionCommand,
    DepthMaskCommand,
    ColorMaskCommand,
    LineWidthCommand,
    StencilFunctionCommand,
    StencilOperationCommand,
    StencilMaskCommand,
    BlendColorCommand,
    BlitFramebufferCommand
>;

/// Full command entry combining the header and typed payload
struct Command {
    CommandHeader  m_header;
    CommandPayload m_payload;
};

#endif // COMMAND_H
