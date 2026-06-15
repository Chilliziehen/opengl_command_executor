#include "CaptureLoader.h"
#include <fstream>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

// ---- helpers ----

static std::string joinPath(const std::string& directory, const std::string& relative) {
    if (directory.empty()) return relative;
    if (directory.back() == '/' || directory.back() == '\\')
        return directory + relative;
    return directory + "/" + relative;
}

static bool readJsonFile(const std::string& filePath,
                         Json&              outJson,
                         std::string&       outError) {
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        outError = "cannot open file: " + filePath;
        return false;
    }
    try {
        stream >> outJson;
    } catch (const std::exception& e) {
        outError = "JSON parse error in " + filePath + ": " + e.what();
        return false;
    }
    return true;
}

static std::vector<int32_t> parseIntArray(const Json& j) {
    std::vector<int32_t> out;
    if (j.is_array()) for (const auto& v : j) out.push_back(v.get<int32_t>());
    return out;
}

static std::vector<float> parseFloatArray(const Json& j) {
    std::vector<float> out;
    if (j.is_array()) for (const auto& v : j) out.push_back(v.get<float>());
    return out;
}

static CommandDataArgument parseCommandDataArg(const Json& arg) {
    if (!arg.is_object()) return std::monostate{};
    std::string typeStr = arg.value("type", "");
    if (typeStr == "UploadRef") {
        UploadReference ref;
        ref.m_uploadPath = arg.value("uploadPath", "");
        ref.m_byteLength = arg.value("byteLength", 0ULL);
        return ref;
    }
    if (typeStr == "Float32Array" || typeStr == "Int32Array" || typeStr == "Uint32Array") {
        UniformDataPayload p;
        p.m_arrayType = typeStr;
        if (arg.contains("data") && arg["data"].is_array())
            for (const auto& v : arg["data"]) p.m_data.push_back(v.get<double>());
        return p;
    }
    return std::monostate{};
}

// ---- resource parsers (unchanged from original) ----

static CaptureBuffer parseBuffer(const Json& j) {
    CaptureBuffer b;
    b.m_bufferId          = j.value("id", 0U);
    b.m_label             = j.value("label", "");
    b.m_target            = j.value("target", 0U);
    b.m_bufferType        = j.value("bufferType", "");
    b.m_usageHint         = j.value("usage", 0U);
    b.m_byteLength        = j.value("byteLength", 0ULL);
    b.m_capturedByteLength = j.value("capturedByteLength", 0ULL);
    b.m_hasBaselineBytes  = j.value("hasBaselineBytes", false);
    b.m_sourceKind        = j.value("sourceKind", "");
    b.m_metadataPath      = j.value("dataPath", "");
    return b;
}

static TextureWrapper parseTexture(const Json& j) {
    TextureWrapper t;
    t.m_textureId         = j.value("id", 0U);
    t.m_label             = j.value("label", "");
    t.m_target            = j.value("target", 0U);
    t.m_width             = j.value("width", 0U);
    t.m_height            = j.value("height", 0U);
    t.m_depth             = j.value("depth", 0U);
    t.m_internalFormat    = j.value("internalFormat", 0U);
    t.m_format            = j.value("format", 0U);
    t.m_pixelType         = j.value("pixelType", 0U);
    t.m_sourceKind        = j.value("sourceKind", "");
    t.m_compressed        = j.value("compressed", false);
    t.m_mipmapLevels      = j.value("levels", 1U);
    t.m_deleted           = j.value("deleted", false);
    t.m_mipmapsGenerated  = j.value("mipsGenerated", false);
    t.m_captureMethod     = j.value("captureMethod", "");
    t.m_byteLength        = j.value("byteLength", 0ULL);
    t.m_capturedWidth     = j.value("capturedWidth", 0U);
    t.m_capturedHeight    = j.value("capturedHeight", 0U);
    t.m_capturedByteLength = j.value("capturedByteLength", 0ULL);
    t.m_hasBaselineBytes  = j.value("hasBaselineBytes", false);
    t.m_textureBinaryPath = j.value("dataPath", "");
    if (j.contains("parameters") && j["parameters"].is_object())
        for (auto& [key, val] : j["parameters"].items())
            t.m_parameters[key] = val.get<int32_t>();
    return t;
}

static CaptureShader parseShader(const Json& j) {
    CaptureShader s;
    s.m_shaderId   = j.value("id", 0U);
    s.m_label      = j.value("label", "");
    s.m_shaderType = j.value("shaderType", 0U);
    s.m_source     = j.value("source", "");
    return s;
}

static CaptureProgram parseProgram(const Json& j) {
    CaptureProgram p;
    p.m_programId = j.value("id", 0U);
    p.m_label     = j.value("label", "");
    p.m_linked    = j.value("linked", false);
    if (j.contains("attachedShaderIds") && j["attachedShaderIds"].is_array())
        for (const auto& sid : j["attachedShaderIds"])
            p.m_attachedShaderIds.push_back(sid.get<uint32_t>());
    return p;
}

static VertexAttribute parseVertexAttribute(const Json& j) {
    VertexAttribute a;
    a.m_enabled        = j.value("enabled", false);
    a.m_componentCount = j.value("size", 0U);
    a.m_componentType  = j.value("type", 0U);
    a.m_normalized     = j.value("normalized", false);
    a.m_stride         = j.value("stride", 0U);
    a.m_offset         = j.value("offset", 0U);
    a.m_bufferId       = j.value("bufferId", 0U);
    a.m_attributeName  = j.value("name", "");
    return a;
}

static CaptureVertexArrayObject parseVertexArrayObject(const Json& j) {
    CaptureVertexArrayObject v;
    v.m_vertexArrayId       = j.value("id", 0U);
    v.m_label               = j.value("label", "");
    v.m_elementArrayBufferId = j.value("elementArrayBufferId", 0U);
    v.m_deleted             = j.value("deleted", false);
    if (j.contains("attribs") && j["attribs"].is_object())
        for (auto& [key, val] : j["attribs"].items())
            v.m_vertexAttributes[key] = parseVertexAttribute(val);
    return v;
}

static FramebufferAttachment parseAttachment(const Json& j) {
    FramebufferAttachment a;
    a.m_attachmentPoint = j.value("attachment", 0U);
    a.m_textureId       = j.value("textureId", 0U);
    a.m_mipmapLevel     = j.value("level", 0U);
    a.m_textureTarget   = j.value("textarget", 0U);
    return a;
}

static CaptureFramebuffer parseFramebuffer(const Json& j) {
    CaptureFramebuffer f;
    f.m_framebufferId = j.value("id", 0U);
    f.m_label         = j.value("label", "");
    if (j.contains("attachments") && j["attachments"].is_array())
        for (const auto& a : j["attachments"])
            f.m_attachments.push_back(parseAttachment(a));
    return f;
}

// ---- command parser dispatch (polymorphic) ----

using CommandParserFunc = std::function<CommandPtr(const Json&)>;

static CreateResourceCommand::ResourceKind resourceKindFromName(const std::string& name) {
    if (name.find("Buffer")      != std::string::npos) return CreateResourceCommand::ResourceKind::Buffer;
    if (name.find("Texture")     != std::string::npos) return CreateResourceCommand::ResourceKind::Texture;
    if (name.find("VertexArray") != std::string::npos) return CreateResourceCommand::ResourceKind::VertexArray;
    if (name.find("Framebuffer") != std::string::npos) return CreateResourceCommand::ResourceKind::Framebuffer;
    if (name.find("Shader")      != std::string::npos) return CreateResourceCommand::ResourceKind::Shader;
    if (name.find("Program")     != std::string::npos) return CreateResourceCommand::ResourceKind::Program;
    return CreateResourceCommand::ResourceKind::Buffer;
}

static uint32_t resourceIdFromJson(const Json& j) {
    if (j.contains("bufferId"))      return j["bufferId"].get<uint32_t>();
    if (j.contains("textureId"))     return j["textureId"].get<uint32_t>();
    if (j.contains("vertexArrayId")) return j["vertexArrayId"].get<uint32_t>();
    if (j.contains("framebufferId")) return j["framebufferId"].get<uint32_t>();
    if (j.contains("shaderId"))      return j["shaderId"].get<uint32_t>();
    if (j.contains("programId"))     return j["programId"].get<uint32_t>();
    return 0U;
}

static const std::unordered_map<std::string, CommandParserFunc>& getCommandParsers() {
    static const std::unordered_map<std::string, CommandParserFunc> parsers = {
        // --- state commands ---
        {"viewport", [](const Json& j) -> CommandPtr {
            return std::make_unique<ViewportCommand>(
                j.value("eventId", 0U), parseIntArray(j["args"]));
        }},
        {"enable", [](const Json& j) -> CommandPtr {
            uint32_t cap = (j["args"].is_array() && !j["args"].empty()) ? j["args"][0].get<uint32_t>() : 0U;
            return std::make_unique<EnableCommand>(j.value("eventId", 0U), cap, true);
        }},
        {"disable", [](const Json& j) -> CommandPtr {
            uint32_t cap = (j["args"].is_array() && !j["args"].empty()) ? j["args"][0].get<uint32_t>() : 0U;
            return std::make_unique<EnableCommand>(j.value("eventId", 0U), cap, false);
        }},
        {"clearColor", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<ClearColorCommand>(j.value("eventId", 0U),
                a[0].get<float>(), a[1].get<float>(), a[2].get<float>(), a[3].get<float>());
        }},
        {"clear", [](const Json& j) -> CommandPtr {
            return std::make_unique<ClearCommand>(j.value("eventId", 0U),
                j.value("mask", 0U), j.value("framebufferId", 0U));
        }},
        {"scissor", [](const Json& j) -> CommandPtr {
            return std::make_unique<ScissorCommand>(j.value("eventId", 0U), parseIntArray(j["args"]));
        }},
        {"blendFunc", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlendFunctionCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>());
        }},
        {"blendFuncSeparate", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlendFunctionCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>());
        }},
        {"blendEquation", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlendEquationCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"blendEquationSeparate", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlendEquationCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"cullFace", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<CullFaceCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"frontFace", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<FrontFaceCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"depthFunc", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<DepthFunctionCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"depthMask", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<DepthMaskCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<bool>() : true);
        }},
        {"colorMask", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<ColorMaskCommand>(j.value("eventId", 0U),
                a[0].get<bool>(), a[1].get<bool>(), a[2].get<bool>(), a[3].get<bool>());
        }},
        {"lineWidth", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<LineWidthCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<float>() : 1.0f);
        }},
        {"stencilFunc", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilFunctionCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<int32_t>(), a[2].get<uint32_t>());
        }},
        {"stencilFuncSeparate", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilFunctionCommand>(j.value("eventId", 0U),
                a[1].get<uint32_t>(), a[2].get<int32_t>(), a[3].get<uint32_t>());
        }},
        {"stencilOp", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilOperationCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>());
        }},
        {"stencilOpSeparate", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilOperationCommand>(j.value("eventId", 0U),
                a[1].get<uint32_t>(), a[2].get<uint32_t>(), a[3].get<uint32_t>());
        }},
        {"stencilMask", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilMaskCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U);
        }},
        {"stencilMaskSeparate", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<StencilMaskCommand>(j.value("eventId", 0U),
                (a.is_array() && a.size() >= 2) ? a[1].get<uint32_t>() : 0U);
        }},
        {"blendColor", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlendColorCommand>(j.value("eventId", 0U),
                a[0].get<float>(), a[1].get<float>(), a[2].get<float>(), a[3].get<float>());
        }},

        // --- resource-binding commands ---
        {"useProgram", [](const Json& j) -> CommandPtr {
            return std::make_unique<UseProgramCommand>(j.value("eventId", 0U), j.value("programId", 0U));
        }},
        {"bindFramebuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<BindFramebufferCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("framebufferId", 0U));
        }},
        {"bindBuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<BindBufferCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("bufferId", 0U));
        }},
        {"bindBufferBase", [](const Json& j) -> CommandPtr {
            return std::make_unique<BindBufferBaseCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("index", 0U), j.value("bufferId", 0U));
        }},
        {"bindBufferRange", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            uint64_t off = (a.is_array() && a.size() >= 5) ? a[3].get<uint64_t>() : 0ULL;
            uint64_t sz  = (a.is_array() && a.size() >= 5) ? a[4].get<uint64_t>() : 0ULL;
            return std::make_unique<BindBufferRangeCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("index", 0U), j.value("bufferId", 0U), off, sz);
        }},
        {"bindTexture", [](const Json& j) -> CommandPtr {
            return std::make_unique<BindTextureCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("textureId", 0U), j.value("unit", 0U));
        }},
        {"activeTexture", [](const Json& j) -> CommandPtr {
            return std::make_unique<ActiveTextureCommand>(j.value("eventId", 0U), j.value("unit", 0U));
        }},
        {"bindVertexArray", [](const Json& j) -> CommandPtr {
            return std::make_unique<BindVertexArrayCommand>(j.value("eventId", 0U),
                j.value("vertexArrayId", 0U));
        }},
        {"vertexAttribPointer", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<VertexAttribPointerCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>(),
                a[3].get<bool>(), a[4].get<uint32_t>(), a[5].get<uint32_t>(),
                j.value("bufferId", 0U), j.value("attribName", ""));
        }},
        {"enableVertexAttribArray", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<VertexAttribEnableCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U, true);
        }},
        {"disableVertexAttribArray", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<VertexAttribEnableCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U, false);
        }},
        {"vertexAttribDivisor", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<VertexAttribDivisorCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>());
        }},

        // --- draw commands ---
        {"drawElements", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<DrawElementsCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>(),
                a[3].get<uint32_t>(), j.value("framebufferId", 0U));
        }},
        {"drawArrays", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<DrawArraysCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>(),
                j.value("framebufferId", 0U));
        }},

        // --- resource lifecycle ---
        {"createBuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<CreateResourceCommand>(j.value("eventId", 0U),
                CreateResourceCommand::ResourceKind::Buffer, j.value("bufferId", 0U), "createBuffer");
        }},
        {"createTexture", [](const Json& j) -> CommandPtr {
            return std::make_unique<CreateResourceCommand>(j.value("eventId", 0U),
                CreateResourceCommand::ResourceKind::Texture, j.value("textureId", 0U), "createTexture");
        }},
        {"createVertexArray", [](const Json& j) -> CommandPtr {
            return std::make_unique<CreateResourceCommand>(j.value("eventId", 0U),
                CreateResourceCommand::ResourceKind::VertexArray, j.value("vertexArrayId", 0U), "createVertexArray");
        }},
        {"createFramebuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<CreateResourceCommand>(j.value("eventId", 0U),
                CreateResourceCommand::ResourceKind::Framebuffer, j.value("framebufferId", 0U), "createFramebuffer");
        }},
        {"deleteBuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::Buffer, j.value("bufferId", 0U), "deleteBuffer");
        }},
        {"deleteTexture", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::Texture, j.value("textureId", 0U), "deleteTexture");
        }},
        {"deleteVertexArray", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::VertexArray, j.value("vertexArrayId", 0U), "deleteVertexArray");
        }},
        {"deleteFramebuffer", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::Framebuffer, j.value("framebufferId", 0U), "deleteFramebuffer");
        }},
        {"deleteShader", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::Shader, j.value("shaderId", 0U), "deleteShader");
        }},
        {"deleteProgram", [](const Json& j) -> CommandPtr {
            return std::make_unique<DeleteResourceCommand>(j.value("eventId", 0U),
                DeleteResourceCommand::ResourceKind::Program, j.value("programId", 0U), "deleteProgram");
        }},

        // --- data upload ---
        {"bufferData", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            CommandDataArgument data = (a.is_array() && a.size() >= 3) ? parseCommandDataArg(a[2]) : CommandDataArgument{};
            return std::make_unique<BufferDataCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("bufferId", 0U),
                (a.is_array() && a.size() >= 2) ? a[1].get<uint64_t>() : 0ULL,
                std::move(data), "bufferData");
        }},
        {"bufferSubData", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            CommandDataArgument data = (a.is_array() && a.size() >= 3) ? parseCommandDataArg(a[2]) : CommandDataArgument{};
            return std::make_unique<BufferDataCommand>(j.value("eventId", 0U),
                j.value("target", 0U), j.value("bufferId", 0U),
                (a.is_array() && a.size() >= 2) ? a[1].get<uint64_t>() : 0ULL,
                std::move(data), "bufferSubData");
        }},
        {"texImage2D", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<TextureImageCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>(),
                a[3].get<uint32_t>(), a[4].get<uint32_t>(), a[6].get<uint32_t>(),
                a[7].get<uint32_t>(), j.value("textureId", 0U),
                (a.is_array() && a.size() >= 9) ? parseCommandDataArg(a[8]) : CommandDataArgument{},
                "texImage2D");
        }},
        {"texSubImage2D", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<TextureImageCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(), a[1].get<uint32_t>(), a[2].get<uint32_t>(),
                a[3].get<uint32_t>(), a[4].get<uint32_t>(), a[6].get<uint32_t>(),
                a[7].get<uint32_t>(), j.value("textureId", 0U),
                (a.is_array() && a.size() >= 9) ? parseCommandDataArg(a[8]) : CommandDataArgument{},
                "texSubImage2D");
        }},
        {"generateMipmap", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<GenerateMipmapCommand>(j.value("eventId", 0U),
                (a.is_array() && !a.empty()) ? a[0].get<uint32_t>() : 0U,
                j.value("textureId", 0U));
        }},

        // --- shader / program ---
        {"shaderSource", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<ShaderSourceCommand>(j.value("eventId", 0U),
                a[0].get<uint32_t>(),
                (a.is_array() && a.size() >= 2) ? a[1].get<std::string>() : "");
        }},
        {"compileShader", [](const Json& j) -> CommandPtr {
            return std::make_unique<NoOpCommand>(j.value("eventId", 0U), "compileShader");
        }},
        {"attachShader", [](const Json& j) -> CommandPtr {
            return std::make_unique<AttachShaderCommand>(j.value("eventId", 0U),
                j.value("programId", 0U), j.value("shaderId", 0U));
        }},
        {"linkProgram", [](const Json& j) -> CommandPtr {
            return std::make_unique<LinkProgramCommand>(j.value("eventId", 0U), j.value("programId", 0U));
        }},

        // --- uniforms ---
        {"uniform1i", [](const Json& j) -> CommandPtr {
            return std::make_unique<UniformSamplerCommand>(j.value("eventId", 0U),
                j.value("uniformName", ""), j.value("programId", 0U),
                (j["args"].is_array() && !j["args"].empty()) ? j["args"][0].get<uint32_t>() : 0U,
                j.value("valueOmitted", false));
        }},
        {"uniform4fv", [](const Json& j) -> CommandPtr {
            CommandDataArgument data = (j["args"].is_array() && !j["args"].empty())
                ? parseCommandDataArg(j["args"][0]) : CommandDataArgument{};
            return std::make_unique<UniformCommand>(j.value("eventId", 0U),
                j.value("uniformName", ""), j.value("programId", 0U),
                j.value("valueOmitted", false), j.value("valueOmittedReason", ""),
                j.value("_snapshot", false), std::move(data), "uniform4fv");
        }},
        {"uniformMatrix4fv", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            bool transpose = (a.is_array() && a.size() >= 1) ? a[0].get<bool>() : false;
            CommandDataArgument data = (a.is_array() && a.size() >= 2)
                ? parseCommandDataArg(a[1]) : CommandDataArgument{};
            return std::make_unique<UniformMatrixCommand>(j.value("eventId", 0U),
                j.value("uniformName", ""), j.value("programId", 0U),
                transpose, j.value("valueOmitted", false), std::move(data));
        }},

        // --- framebuffer ---
        {"framebufferTexture2D", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<FramebufferTexture2DCommand>(j.value("eventId", 0U),
                j.value("framebufferId", 0U), j.value("attachment", 0U),
                (a.is_array() && a.size() >= 3) ? a[2].get<uint32_t>() : 0U,
                (a.is_array() && a.size() >= 4) ? a[3].get<uint32_t>() : 0U,
                (a.is_array() && a.size() >= 5) ? a[4].get<uint32_t>() : 0U);
        }},
        {"blitFramebuffer", [](const Json& j) -> CommandPtr {
            const auto& a = j["args"];
            return std::make_unique<BlitFramebufferCommand>(j.value("eventId", 0U),
                a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<int32_t>(), a[3].get<int32_t>(),
                a[4].get<int32_t>(), a[5].get<int32_t>(), a[6].get<int32_t>(), a[7].get<int32_t>(),
                a[8].get<uint32_t>(), a[9].get<uint32_t>());
        }},

        // --- uncaptured / unsupported (no-op) ---
        {"readPixels",    [](const Json& j) -> CommandPtr { return std::make_unique<NoOpCommand>(j.value("eventId", 0U), "readPixels"); }},
        {"drawBuffers",   [](const Json& j) -> CommandPtr { return std::make_unique<NoOpCommand>(j.value("eventId", 0U), "drawBuffers"); }},
    };
    return parsers;
}

static CommandPtr parseCommand(const Json& j) {
    auto& parsers = getCommandParsers();
    auto it = parsers.find(j.value("name", ""));
    if (it != parsers.end()) return it->second(j);
    return std::make_unique<NoOpCommand>(j.value("eventId", 0U), j.value("name", "unknown"));
}

// ---- manifest ----

bool CaptureLoader::parseManifest(const std::string& directoryPath,
                                   CaptureManifest&   outManifest,
                                   std::string&       outError) {
    Json j;
    if (!readJsonFile(joinPath(directoryPath, "manifest.json"), j, outError))
        return false;

    outManifest.m_format       = j.value("format", "");
    outManifest.m_version      = j.value("version", 0U);
    outManifest.m_captureModel = j.value("captureModel", "");
    outManifest.m_generatedAt  = j.value("generatedAt", "");
    outManifest.m_frameIndex   = j.value("frameIndex", 0U);
    outManifest.m_projectRoot  = j.value("projectRoot", "");
    outManifest.m_sandboxRoot  = j.value("sandboxRoot", "");

    if (j.contains("canvas")) {
        auto& c = j["canvas"];
        outManifest.m_canvas.width             = c.value("width", 0U);
        outManifest.m_canvas.height            = c.value("height", 0U);
        outManifest.m_canvas.cssWidth          = c.value("cssWidth", 0U);
        outManifest.m_canvas.cssHeight         = c.value("cssHeight", 0U);
        outManifest.m_canvas.devicePixelRatio  = c.value("devicePixelRatio", 1.0f);
    }
    if (j.contains("replayerCompatibility")) {
        auto& r = j["replayerCompatibility"];
        outManifest.m_replayerCompatibility.m_currentStandaloneReplayer = r.value("currentStandaloneReplayer", false);
        outManifest.m_replayerCompatibility.m_hasPreviewPayload        = r.value("hasPreviewPayload", false);
        outManifest.m_replayerCompatibility.m_reason                   = r.value("reason", "");
    }
    if (j.contains("diagnostics")) {
        auto& d = j["diagnostics"];
        outManifest.m_diagnostics.profile               = d.value("profile", "");
        outManifest.m_diagnostics.commandLimit           = d.value("commandLimit", 0U);
        outManifest.m_diagnostics.commandCount           = d.value("commandCount", 0U);
        outManifest.m_diagnostics.uniformCommandCount    = d.value("uniformCommandCount", 0U);
        outManifest.m_diagnostics.commandLimitReached    = d.value("commandLimitReached", false);
        outManifest.m_diagnostics.resourceBaselineMode   = d.value("resourceBaselineMode", "");
        outManifest.m_diagnostics.maxResourceBaselineBytes = d.value("maxResourceBaselineBytes", 0U);
        outManifest.m_diagnostics.bufferUploadPayloadEnabled  = d.value("bufferUploadPayloadEnabled", false);
        outManifest.m_diagnostics.textureUploadPayloadEnabled = d.value("textureUploadPayloadEnabled", false);
        outManifest.m_diagnostics.uniformValueMode       = d.value("uniformValueMode", "");
    }
    if (j.contains("files")) {
        auto& f = j["files"];
        outManifest.m_files.m_commandsPath      = f.value("commands", "");
        outManifest.m_files.m_statePath         = f.value("state", "");
        outManifest.m_files.m_shadersPath       = f.value("shaders", "");
        outManifest.m_files.m_programsPath      = f.value("programs", "");
        outManifest.m_files.m_framebuffersPath  = f.value("framebuffers", "");
        outManifest.m_files.m_vertexArraysPath  = f.value("vaos", "");
        outManifest.m_files.m_debugLogPath      = f.value("debugLog", "");
        outManifest.m_files.m_runtimeLogsPath   = f.value("runtimeLogs", "");

        if (f.contains("textures") && f["textures"].is_array()) {
            for (auto& t : f["textures"]) {
                ResourceFileEntry e;
                e.resourceId     = t.value("id", 0U);
                e.label          = t.value("label", "");
                e.metadataPath   = t.value("path", "");
                e.binaryDataPath = t.value("dataPath", "");
                outManifest.m_files.m_textures.push_back(e);
            }
        }
        if (f.contains("buffers") && f["buffers"].is_array()) {
            for (auto& b : f["buffers"]) {
                ResourceFileEntry e;
                e.resourceId     = b.value("id", 0U);
                e.label          = b.value("label", "");
                e.metadataPath   = b.value("path", "");
                e.binaryDataPath = b.value("dataPath", "");
                outManifest.m_files.m_buffers.push_back(e);
            }
        }
        if (f.contains("uploads") && f["uploads"].is_array()) {
            for (auto& u : f["uploads"]) {
                ResourceFileEntry e;
                e.metadataPath   = u.value("path", "");
                e.binaryDataPath = u.value("dataPath", "");
                outManifest.m_files.m_uploads.push_back(e);
            }
        }
    }
    return true;
}

// ---- top-level resource file loaders ----

bool CaptureLoader::parseBuffers(const std::string&     directoryPath,
                                  const CaptureManifest& manifest,
                                  std::vector<CaptureBuffer>& outBuffers,
                                  std::string&                outError) {
    for (auto& entry : manifest.m_files.m_buffers) {
        if (entry.metadataPath.empty()) continue;
        Json j;
        if (!readJsonFile(joinPath(directoryPath, entry.metadataPath), j, outError)) return false;
        CaptureBuffer b = parseBuffer(j);
        b.m_metadataPath   = entry.metadataPath;
        b.m_binaryDataPath = entry.binaryDataPath;
        outBuffers.push_back(std::move(b));
    }
    return true;
}

bool CaptureLoader::parseTextures(const std::string&        directoryPath,
                                   const CaptureManifest&    manifest,
                                   std::vector<TextureWrapper>& outTextures,
                                   std::string&                outError) {
    for (auto& entry : manifest.m_files.m_textures) {
        if (entry.metadataPath.empty()) continue;
        Json j;
        if (!readJsonFile(joinPath(directoryPath, entry.metadataPath), j, outError)) return false;
        TextureWrapper t = parseTexture(j);
        t.m_textureMetaPath   = entry.metadataPath;
        t.m_textureBinaryPath = entry.binaryDataPath;
        outTextures.push_back(std::move(t));
    }
    return true;
}

bool CaptureLoader::parseShaders(const std::string&       directoryPath,
                                  const CaptureManifest&   manifest,
                                  std::vector<CaptureShader>& outShaders,
                                  std::string&                outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_shadersPath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "shaders.json is not an array"; return false; }
    for (auto& entry : j) outShaders.push_back(parseShader(entry));
    return true;
}

bool CaptureLoader::parsePrograms(const std::string&         directoryPath,
                                   const CaptureManifest&     manifest,
                                   std::vector<CaptureProgram>& outPrograms,
                                   std::string&                outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_programsPath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "programs.json is not an array"; return false; }
    for (auto& entry : j) outPrograms.push_back(parseProgram(entry));
    return true;
}

bool CaptureLoader::parseVertexArrays(const std::string&     directoryPath,
                                       const CaptureManifest& manifest,
                                       std::vector<CaptureVertexArrayObject>& outVertexArrays,
                                       std::string&                         outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_vertexArraysPath);
    if (filePath.empty()) return true;
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "vaos.json is not an array"; return false; }
    for (auto& entry : j) outVertexArrays.push_back(parseVertexArrayObject(entry));
    return true;
}

bool CaptureLoader::parseFramebuffers(const std::string&       directoryPath,
                                       const CaptureManifest&   manifest,
                                       std::vector<CaptureFramebuffer>& outFramebuffers,
                                       std::string&                   outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_framebuffersPath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "framebuffers.json is not an array"; return false; }
    for (auto& entry : j) outFramebuffers.push_back(parseFramebuffer(entry));
    return true;
}

bool CaptureLoader::parseState(const std::string&     directoryPath,
                                const CaptureManifest& manifest,
                                CaptureState&          outState,
                                std::string&           outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_statePath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;

    outState.m_currentProgramId            = j.value("currentProgramId", 0U);
    outState.m_currentFramebufferId        = j.value("currentFramebufferId", 0U);
    outState.m_currentActiveTextureUnit    = j.value("currentActiveTexture", 0U);
    outState.m_currentArrayBufferId        = j.value("currentArrayBufferId", 0U);
    outState.m_currentElementArrayBufferId = j.value("currentElementArrayBufferId", 0U);
    outState.m_currentVertexArrayId        = j.value("currentVertexArrayId", 0U);

    if (j.contains("currentTextures") && j["currentTextures"].is_object())
        for (auto& [k, v] : j["currentTextures"].items())
            outState.m_currentTextureBindings[k] = v.get<uint32_t>();

    if (j.contains("attribs") && j["attribs"].is_object())
        for (auto& [k, v] : j["attribs"].items())
            outState.m_vertexAttributes[k] = parseVertexAttribute(v);

    if (j.contains("viewport") && j["viewport"].is_array())
        outState.m_viewport = parseIntArray(j["viewport"]);

    if (j.contains("capabilities") && j["capabilities"].is_object())
        for (auto& [k, v] : j["capabilities"].items())
            outState.m_capabilities[k] = v.get<bool>();

    if (j.contains("clearColor") && j["clearColor"].is_array())
        outState.m_clearColor = parseFloatArray(j["clearColor"]);

    outState.m_clearDepth   = j.value("clearDepth", 1.0f);
    outState.m_clearStencil = j.value("clearStencil", 0);

    if (j.contains("colorMask") && j["colorMask"].is_array()) {
        auto& cm = j["colorMask"];
        outState.m_colorMask = { cm[0].get<bool>(), cm[1].get<bool>(), cm[2].get<bool>(), cm[3].get<bool>() };
    }

    outState.m_depthMask       = j.value("depthMask", true);
    outState.m_depthFunction   = j.value("depthFunc", 0U);
    outState.m_cullFaceMode    = j.value("cullFaceMode", 0U);
    outState.m_frontFaceOrientation = j.value("frontFace", 0U);

    if (j.contains("blendFunc")) {
        auto& bf = j["blendFunc"];
        outState.m_blendFunction.m_sourceRgb        = bf.value("srcRGB", 0U);
        outState.m_blendFunction.m_destinationRgb   = bf.value("dstRGB", 0U);
        outState.m_blendFunction.m_sourceAlpha      = bf.value("srcAlpha", 0U);
        outState.m_blendFunction.m_destinationAlpha = bf.value("dstAlpha", 0U);
    }
    if (j.contains("blendEquation")) {
        auto& be = j["blendEquation"];
        outState.m_blendEquation.m_rgbMode   = be.value("rgb", 0U);
        outState.m_blendEquation.m_alphaMode = be.value("alpha", 0U);
    }
    if (j.contains("scissorBox") && j["scissorBox"].is_array())
        outState.m_scissorBox = parseIntArray(j["scissorBox"]);
    if (j.contains("pixelStore") && j["pixelStore"].is_object())
        for (auto& [k, v] : j["pixelStore"].items())
            outState.m_pixelStoreParameters[k] = v.get<int32_t>();

    return true;
}

bool CaptureLoader::parseCommands(const std::string&     directoryPath,
                                   const CaptureManifest& manifest,
                                   std::vector<CommandPtr>&  outCommands,
                                   std::string&           outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_commandsPath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "commands.json is not an array"; return false; }
    for (auto& entry : j) {
        auto cmd = parseCommand(entry);
        if (cmd) outCommands.push_back(std::move(cmd));
    }
    return true;
}

// ---- public entry point ----

bool CaptureLoader::loadFromDirectory(const std::string& captureDirectoryPath,
                                       FrameCapture&        outCapture,
                                       std::string&         outErrorMessage) {
    outCapture.clear();
    outErrorMessage.clear();

    if (!parseManifest(captureDirectoryPath, outCapture.m_manifest, outErrorMessage))
        return false;

    const auto& manifest = outCapture.m_manifest;

    if (!parseBuffers(captureDirectoryPath, manifest, outCapture.m_buffers, outErrorMessage))
        return false;
    if (!parseTextures(captureDirectoryPath, manifest, outCapture.m_textures, outErrorMessage))
        return false;
    if (!parseShaders(captureDirectoryPath, manifest, outCapture.m_shaders, outErrorMessage))
        return false;
    if (!parsePrograms(captureDirectoryPath, manifest, outCapture.m_programs, outErrorMessage))
        return false;
    if (!parseVertexArrays(captureDirectoryPath, manifest, outCapture.m_vertexArrays, outErrorMessage))
        return false;
    if (!parseFramebuffers(captureDirectoryPath, manifest, outCapture.m_framebuffers, outErrorMessage))
        return false;
    if (!parseState(captureDirectoryPath, manifest, outCapture.m_state, outErrorMessage))
        return false;
    if (!parseCommands(captureDirectoryPath, manifest, outCapture.m_commands, outErrorMessage))
        return false;

    return true;
}
