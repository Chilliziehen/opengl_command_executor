#include "CaptureLoader.h"
#include <fstream>
#include <sstream>
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

// ---- single-value extractors ----

template<typename T>
static T safeGet(const Json& j, const char* key, T defaultValue) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return defaultValue;
    return it->template get<T>();
}

template<typename T>
static bool requiredGet(const Json& j, const char* key, T& outValue, std::string& outError) {
    auto it = j.find(key);
    if (it == j.end()) {
        outError = std::string("missing required field: ") + key;
        return false;
    }
    outValue = it->template get<T>();
    return true;
}

// ---- resource parsers ----

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
    b.m_metadataPath      = j.value("dataPath", "");  // kept in metadataPath for simplicity
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
    if (j.contains("parameters") && j["parameters"].is_object()) {
        for (auto& [key, val] : j["parameters"].items())
            t.m_parameters[key] = val.get<int32_t>();
    }
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
    if (j.contains("attachedShaderIds") && j["attachedShaderIds"].is_array()) {
        for (const auto& sid : j["attachedShaderIds"])
            p.m_attachedShaderIds.push_back(sid.get<uint32_t>());
    }
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
    if (j.contains("attribs") && j["attribs"].is_object()) {
        for (auto& [key, val] : j["attribs"].items())
            v.m_vertexAttributes[key] = parseVertexAttribute(val);
    }
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
    if (j.contains("attachments") && j["attachments"].is_array()) {
        for (const auto& a : j["attachments"])
            f.m_attachments.push_back(parseAttachment(a));
    }
    return f;
}

// ---- command parser ----

static Command parseCommand(const Json& j);

static CommandHeader parseCommandHeader(const Json& j) {
    CommandHeader h;
    h.m_eventId     = j.value("eventId", 0U);
    h.m_commandName = j.value("name", "");
    return h;
}

static std::vector<int32_t> parseIntArray(const Json& j) {
    std::vector<int32_t> out;
    if (j.is_array()) {
        for (const auto& v : j)
            out.push_back(v.get<int32_t>());
    }
    return out;
}

static std::vector<float> parseFloatArray(const Json& j) {
    std::vector<float> out;
    if (j.is_array())
        for (const auto& v : j) out.push_back(v.get<float>());
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
    // UploadSkipped / unknown — use monostate
    return std::monostate{};
}

// Per-command parsers
static ViewportCommand parseViewport(const Json& j) {
    ViewportCommand c;
    if (j.contains("args") && j["args"].is_array())
        c.m_bounds = parseIntArray(j["args"]);
    return c;
}
static EnableCommand parseEnable(const Json& j) {
    EnableCommand c;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) c.m_capability = a[0].get<uint32_t>();
    return c;
}
static ClearCommand parseClear(const Json& j) {
    ClearCommand c;
    c.m_mask          = j.value("mask", 0U);
    c.m_framebufferId = j.value("framebufferId", 0U);
    return c;
}
static UseProgramCommand parseUseProgram(const Json& j) {
    return { j.value("programId", 0U) };
}
static UniformCommand parseUniform(const Json& j) {
    UniformCommand u;
    u.m_programId          = j.value("programId", 0U);
    u.m_uniformName        = j.value("uniformName", "");
    u.m_valueOmitted       = j.value("valueOmitted", false);
    u.m_valueOmittedReason = j.value("valueOmittedReason", "");
    u.m_isSnapshot         = j.value("_snapshot", false);
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) u.m_data = parseCommandDataArg(a[0]);
    return u;
}
static UniformMatrixCommand parseUniformMatrix(const Json& j) {
    UniformMatrixCommand u;
    u.m_programId    = j.value("programId", 0U);
    u.m_uniformName  = j.value("uniformName", "");
    u.m_valueOmitted = j.value("valueOmitted", false);
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 2) {
        u.m_transpose = a[0].get<bool>();
        u.m_data      = parseCommandDataArg(a[1]);
    }
    return u;
}
static UniformSamplerCommand parseUniformSampler(const Json& j) {
    UniformSamplerCommand u;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) u.m_textureUnit = a[0].get<uint32_t>();
    u.m_programId    = j.value("programId", 0U);
    u.m_uniformName  = j.value("uniformName", "");
    u.m_valueOmitted = j.value("valueOmitted", false);
    return u;
}
static BindVertexArrayCommand parseBindVertexArray(const Json& j) {
    return { j.value("vertexArrayId", 0U) };
}
static DrawElementsCommand parseDrawElements(const Json& j) {
    DrawElementsCommand d;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 4) {
        d.m_drawMode    = a[0].get<uint32_t>();
        d.m_indexCount  = a[1].get<uint32_t>();
        d.m_indexType   = a[2].get<uint32_t>();
        d.m_indexOffset = a[3].get<uint32_t>();
    }
    d.m_framebufferId = j.value("framebufferId", 0U);
    return d;
}
static DrawArraysCommand parseDrawArrays(const Json& j) {
    DrawArraysCommand d;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 3) {
        d.m_drawMode    = a[0].get<uint32_t>();
        d.m_firstVertex = a[1].get<uint32_t>();
        d.m_vertexCount = a[2].get<uint32_t>();
    }
    d.m_framebufferId = j.value("framebufferId", 0U);
    return d;
}
static BindFramebufferCommand parseBindFramebuffer(const Json& j) {
    return { j.value("target", 0U), j.value("framebufferId", 0U) };
}
static BindBufferCommand parseBindBuffer(const Json& j) {
    BindBufferCommand b;
    b.m_target   = j.value("target", 0U);
    b.m_bufferId = j.value("bufferId", 0U);
    return b;
}
static BindBufferBaseCommand parseBindBufferBase(const Json& j) {
    BindBufferBaseCommand b;
    b.m_target   = j.value("target", 0U);
    b.m_index    = j.value("index", 0U);
    b.m_bufferId = j.value("bufferId", 0U);
    return b;
}
static BindBufferRangeCommand parseBindBufferRange(const Json& j) {
    BindBufferRangeCommand b;
    b.m_target   = j.value("target", 0U);
    b.m_index    = j.value("index", 0U);
    b.m_bufferId = j.value("bufferId", 0U);
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 5) {
        b.m_offset = a[3].get<uint64_t>();
        b.m_size   = a[4].get<uint64_t>();
    }
    return b;
}
static BindTextureCommand parseBindTexture(const Json& j) {
    BindTextureCommand b;
    b.m_target    = j.value("target", 0U);
    b.m_textureId = j.value("textureId", 0U);
    b.m_unit      = j.value("unit", 0U);
    return b;
}
static ActiveTextureCommand parseActiveTexture(const Json& j) {
    return { j.value("unit", 0U) };
}
static VertexAttribPointerCommand parseVertexAttribPointer(const Json& j) {
    VertexAttribPointerCommand v;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 6) {
        v.m_attributeIndex = a[0].get<uint32_t>();
        v.m_componentCount = a[1].get<uint32_t>();
        v.m_componentType  = a[2].get<uint32_t>();
        v.m_normalized     = a[3].get<bool>();
        v.m_stride         = a[4].get<uint32_t>();
        v.m_offset         = a[5].get<uint32_t>();
    }
    v.m_bufferId       = j.value("bufferId", 0U);
    v.m_attributeName  = j.value("attribName", "");
    return v;
}
static VertexAttribEnableCommand parseVertexAttribEnable(const Json& j) {
    VertexAttribEnableCommand v;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) v.m_attributeIndex = a[0].get<uint32_t>();
    return v;
}
static VertexAttribDivisorCommand parseVertexAttribDivisor(const Json& j) {
    VertexAttribDivisorCommand v;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 2) {
        v.m_attributeIndex = a[0].get<uint32_t>();
        v.m_divisor        = a[1].get<uint32_t>();
    }
    return v;
}
static CreateResourceCommand parseCreateResource(const Json& j) {
    uint32_t id = 0;
    if (j.contains("bufferId"))      id = j["bufferId"].get<uint32_t>();
    else if (j.contains("textureId")) id = j["textureId"].get<uint32_t>();
    else if (j.contains("vertexArrayId")) id = j["vertexArrayId"].get<uint32_t>();
    else if (j.contains("framebufferId")) id = j["framebufferId"].get<uint32_t>();
    return { id };
}
static DeleteResourceCommand parseDeleteResource(const Json& j) {
    uint32_t id = 0;
    if (j.contains("bufferId"))       id = j["bufferId"].get<uint32_t>();
    else if (j.contains("textureId"))  id = j["textureId"].get<uint32_t>();
    else if (j.contains("vertexArrayId")) id = j["vertexArrayId"].get<uint32_t>();
    else if (j.contains("framebufferId")) id = j["framebufferId"].get<uint32_t>();
    return { id };
}
static BufferDataCommand parseBufferData(const Json& j) {
    BufferDataCommand b;
    b.m_target   = j.value("target", 0U);
    b.m_bufferId = j.value("bufferId", 0U);
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 3) {
        b.m_offset = a[1].get<uint64_t>();
        b.m_data   = parseCommandDataArg(a[2]);
    }
    return b;
}
static TextureImageCommand parseTexImage(const Json& j) {
    TextureImageCommand t;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 9) {
        t.m_textureId      = a[0].get<uint32_t>();
        t.m_mipmapLevel    = a[1].get<uint32_t>();
        t.m_internalFormat = a[2].get<uint32_t>();
        t.m_width          = a[3].get<uint32_t>();
        t.m_height         = a[4].get<uint32_t>();
        t.m_format         = a[6].get<uint32_t>();
        t.m_pixelType      = a[7].get<uint32_t>();
        t.m_data           = parseCommandDataArg(a[8]);
    }
    t.m_textureId = j.value("textureId", 0U);
    return t;
}
static GenerateMipmapCommand parseGenerateMipmap(const Json& j) {
    GenerateMipmapCommand g;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) g.m_target = a[0].get<uint32_t>();
    g.m_textureId = j.value("textureId", 0U);
    return g;
}
static ShaderSourceCommand parseShaderSource(const Json& j) {
    ShaderSourceCommand s;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 2) {
        s.m_shaderId = a[0].get<uint32_t>();
        s.m_source   = a[1].get<std::string>();
    }
    return s;
}
static AttachShaderCommand parseAttachShader(const Json& j) {
    return { j.value("programId", 0U), j.value("shaderId", 0U) };
}
static LinkProgramCommand parseLinkProgram(const Json& j) {
    return { j.value("programId", 0U) };
}
static FramebufferTexture2DCommand parseFramebufferTexture2D(const Json& j) {
    FramebufferTexture2DCommand f;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 5) {
        f.m_textureTarget = a[2].get<uint32_t>();
        f.m_textureId     = a[3].get<uint32_t>();
        f.m_mipmapLevel   = a[4].get<uint32_t>();
    }
    f.m_framebufferId = j.value("framebufferId", 0U);
    f.m_attachment    = j.value("attachment", 0U);
    return f;
}
static ScissorCommand parseScissor(const Json& j) {
    ScissorCommand s;
    if (j.contains("args") && j["args"].is_array())
        s.m_box = parseIntArray(j["args"]);
    return s;
}
static BlendFunctionCommand parseBlendFunc(const Json& j) {
    BlendFunctionCommand b;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 2) {
        b.m_sourceFactor      = a[0].get<uint32_t>();
        b.m_destinationFactor = a[1].get<uint32_t>();
    }
    return b;
}
static BlendEquationCommand parseBlendEquation(const Json& j) {
    BlendEquationCommand b;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) b.m_mode = a[0].get<uint32_t>();
    return b;
}
static CullFaceCommand parseCullFace(const Json& j) {
    CullFaceCommand c;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) c.m_mode = a[0].get<uint32_t>();
    return c;
}
static FrontFaceCommand parseFrontFace(const Json& j) {
    FrontFaceCommand f;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) f.m_orientation = a[0].get<uint32_t>();
    return f;
}
static DepthFunctionCommand parseDepthFunc(const Json& j) {
    DepthFunctionCommand d;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) d.m_function = a[0].get<uint32_t>();
    return d;
}
static DepthMaskCommand parseDepthMask(const Json& j) {
    DepthMaskCommand d;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) d.m_enabled = a[0].get<bool>();
    return d;
}
static ColorMaskCommand parseColorMask(const Json& j) {
    ColorMaskCommand c;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 4) {
        c.m_red   = a[0].get<bool>();
        c.m_green = a[1].get<bool>();
        c.m_blue  = a[2].get<bool>();
        c.m_alpha = a[3].get<bool>();
    }
    return c;
}
static LineWidthCommand parseLineWidth(const Json& j) {
    LineWidthCommand l;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) l.m_width = a[0].get<float>();
    return l;
}
static StencilFunctionCommand parseStencilFunc(const Json& j) {
    StencilFunctionCommand s;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 3) {
        s.m_function  = a[0].get<uint32_t>();
        s.m_reference = a[1].get<int32_t>();
        s.m_mask      = a[2].get<uint32_t>();
    }
    return s;
}
static StencilOperationCommand parseStencilOp(const Json& j) {
    StencilOperationCommand s;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 3) {
        s.m_stencilFail = a[0].get<uint32_t>();
        s.m_depthFail   = a[1].get<uint32_t>();
        s.m_depthPass   = a[2].get<uint32_t>();
    }
    return s;
}
static StencilMaskCommand parseStencilMask(const Json& j) {
    StencilMaskCommand s;
    const auto& a = j["args"];
    if (a.is_array() && !a.empty()) s.m_mask = a[0].get<uint32_t>();
    return s;
}
static BlendColorCommand parseBlendColor(const Json& j) {
    BlendColorCommand b;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 4) {
        b.m_red   = a[0].get<float>();
        b.m_green = a[1].get<float>();
        b.m_blue  = a[2].get<float>();
        b.m_alpha = a[3].get<float>();
    }
    return b;
}
static BlitFramebufferCommand parseBlitFramebuffer(const Json& j) {
    BlitFramebufferCommand b;
    const auto& a = j["args"];
    if (a.is_array() && a.size() >= 10) {
        b.m_sourceX0      = a[0].get<int32_t>();
        b.m_sourceY0      = a[1].get<int32_t>();
        b.m_sourceX1      = a[2].get<int32_t>();
        b.m_sourceY1      = a[3].get<int32_t>();
        b.m_destinationX0 = a[4].get<int32_t>();
        b.m_destinationY0 = a[5].get<int32_t>();
        b.m_destinationX1 = a[6].get<int32_t>();
        b.m_destinationY1 = a[7].get<int32_t>();
        b.m_mask          = a[8].get<uint32_t>();
        b.m_filter        = a[9].get<uint32_t>();
    }
    return b;
}

// Command dispatch table
using CommandParser = std::function<CommandPayload(const Json&)>;

static const std::unordered_map<std::string, CommandParser>& commandParsers() {
    static const std::unordered_map<std::string, CommandParser> parsers = {
        {"viewport",                [](const Json& j) -> CommandPayload { return parseViewport(j); }},
        {"enable",                  [](const Json& j) -> CommandPayload { return parseEnable(j); }},
        {"disable",                 [](const Json& j) -> CommandPayload { return parseEnable(j); }},
        {"clearColor",              [](const Json& j) -> CommandPayload { return std::monostate{}; }},
        {"clear",                   [](const Json& j) -> CommandPayload { return parseClear(j); }},
        {"useProgram",              [](const Json& j) -> CommandPayload { return parseUseProgram(j); }},
        {"uniform1i",               [](const Json& j) -> CommandPayload { return parseUniformSampler(j); }},
        {"uniform4fv",              [](const Json& j) -> CommandPayload { return parseUniform(j); }},
        {"uniformMatrix4fv",        [](const Json& j) -> CommandPayload { return parseUniformMatrix(j); }},
        {"bindVertexArray",         [](const Json& j) -> CommandPayload { return parseBindVertexArray(j); }},
        {"drawElements",            [](const Json& j) -> CommandPayload { return parseDrawElements(j); }},
        {"drawArrays",              [](const Json& j) -> CommandPayload { return parseDrawArrays(j); }},
        {"bindFramebuffer",         [](const Json& j) -> CommandPayload { return parseBindFramebuffer(j); }},
        {"bindBuffer",              [](const Json& j) -> CommandPayload { return parseBindBuffer(j); }},
        {"bindBufferBase",          [](const Json& j) -> CommandPayload { return parseBindBufferBase(j); }},
        {"bindBufferRange",         [](const Json& j) -> CommandPayload { return parseBindBufferRange(j); }},
        {"bindTexture",             [](const Json& j) -> CommandPayload { return parseBindTexture(j); }},
        {"activeTexture",           [](const Json& j) -> CommandPayload { return parseActiveTexture(j); }},
        {"vertexAttribPointer",     [](const Json& j) -> CommandPayload { return parseVertexAttribPointer(j); }},
        {"enableVertexAttribArray",  [](const Json& j) -> CommandPayload { return parseVertexAttribEnable(j); }},
        {"disableVertexAttribArray", [](const Json& j) -> CommandPayload { return parseVertexAttribEnable(j); }},
        {"vertexAttribDivisor",     [](const Json& j) -> CommandPayload { return parseVertexAttribDivisor(j); }},
        {"createVertexArray",       [](const Json& j) -> CommandPayload { return parseCreateResource(j); }},
        {"deleteVertexArray",       [](const Json& j) -> CommandPayload { return parseDeleteResource(j); }},
        {"createBuffer",            [](const Json& j) -> CommandPayload { return parseCreateResource(j); }},
        {"deleteBuffer",            [](const Json& j) -> CommandPayload { return parseDeleteResource(j); }},
        {"createTexture",           [](const Json& j) -> CommandPayload { return parseCreateResource(j); }},
        {"deleteTexture",           [](const Json& j) -> CommandPayload { return parseDeleteResource(j); }},
        {"createFramebuffer",       [](const Json& j) -> CommandPayload { return parseCreateResource(j); }},
        {"deleteFramebuffer",       [](const Json& j) -> CommandPayload { return parseDeleteResource(j); }},
        {"bufferData",              [](const Json& j) -> CommandPayload { return parseBufferData(j); }},
        {"bufferSubData",           [](const Json& j) -> CommandPayload { return parseBufferData(j); }},
        {"texImage2D",              [](const Json& j) -> CommandPayload { return parseTexImage(j); }},
        {"texSubImage2D",           [](const Json& j) -> CommandPayload { return parseTexImage(j); }},
        {"generateMipmap",          [](const Json& j) -> CommandPayload { return parseGenerateMipmap(j); }},
        {"shaderSource",            [](const Json& j) -> CommandPayload { return parseShaderSource(j); }},
        {"compileShader",           [](const Json& j) -> CommandPayload { return std::monostate{}; }},
        {"attachShader",            [](const Json& j) -> CommandPayload { return parseAttachShader(j); }},
        {"linkProgram",             [](const Json& j) -> CommandPayload { return parseLinkProgram(j); }},
        {"framebufferTexture2D",    [](const Json& j) -> CommandPayload { return parseFramebufferTexture2D(j); }},
        {"scissor",                 [](const Json& j) -> CommandPayload { return parseScissor(j); }},
        {"blendFunc",               [](const Json& j) -> CommandPayload { return parseBlendFunc(j); }},
        {"blendFuncSeparate",       [](const Json& j) -> CommandPayload { return parseBlendFunc(j); }},
        {"blendEquation",           [](const Json& j) -> CommandPayload { return parseBlendEquation(j); }},
        {"blendEquationSeparate",   [](const Json& j) -> CommandPayload { return parseBlendEquation(j); }},
        {"cullFace",                [](const Json& j) -> CommandPayload { return parseCullFace(j); }},
        {"frontFace",               [](const Json& j) -> CommandPayload { return parseFrontFace(j); }},
        {"depthFunc",               [](const Json& j) -> CommandPayload { return parseDepthFunc(j); }},
        {"depthMask",               [](const Json& j) -> CommandPayload { return parseDepthMask(j); }},
        {"colorMask",               [](const Json& j) -> CommandPayload { return parseColorMask(j); }},
        {"lineWidth",               [](const Json& j) -> CommandPayload { return parseLineWidth(j); }},
        {"stencilFunc",             [](const Json& j) -> CommandPayload { return parseStencilFunc(j); }},
        {"stencilFuncSeparate",     [](const Json& j) -> CommandPayload { return parseStencilFunc(j); }},
        {"stencilOp",               [](const Json& j) -> CommandPayload { return parseStencilOp(j); }},
        {"stencilOpSeparate",       [](const Json& j) -> CommandPayload { return parseStencilOp(j); }},
        {"stencilMask",             [](const Json& j) -> CommandPayload { return parseStencilMask(j); }},
        {"stencilMaskSeparate",     [](const Json& j) -> CommandPayload { return parseStencilMask(j); }},
        {"blendColor",              [](const Json& j) -> CommandPayload { return parseBlendColor(j); }},
        {"blitFramebuffer",         [](const Json& j) -> CommandPayload { return parseBlitFramebuffer(j); }},
        {"readPixels",              [](const Json& j) -> CommandPayload { return std::monostate{}; }},
        {"drawBuffers",             [](const Json& j) -> CommandPayload { return std::monostate{}; }},
    };
    return parsers;
}

static Command parseCommand(const Json& j) {
    Command cmd;
    cmd.m_header = parseCommandHeader(j);
    auto& parsers = commandParsers();
    auto it = parsers.find(cmd.m_header.m_commandName);
    if (it != parsers.end())
        cmd.m_payload = it->second(j);
    else
        cmd.m_payload = std::monostate{};
    return cmd;
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
        outManifest.m_canvas.width            = c.value("width", 0U);
        outManifest.m_canvas.height           = c.value("height", 0U);
        outManifest.m_canvas.cssWidth         = c.value("cssWidth", 0U);
        outManifest.m_canvas.cssHeight        = c.value("cssHeight", 0U);
        outManifest.m_canvas.devicePixelRatio = c.value("devicePixelRatio", 1.0f);
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

// ---- top-level loaders for each resource file ----

bool CaptureLoader::parseBuffers(const std::string&     directoryPath,
                                  const CaptureManifest& manifest,
                                  std::vector<CaptureBuffer>& outBuffers,
                                  std::string&                outError) {
    for (auto& entry : manifest.m_files.m_buffers) {
        if (entry.metadataPath.empty()) continue;
        Json j;
        if (!readJsonFile(joinPath(directoryPath, entry.metadataPath), j, outError))
            return false;
        CaptureBuffer b = parseBuffer(j);
        b.m_metadataPath = entry.metadataPath;
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
        if (!readJsonFile(joinPath(directoryPath, entry.metadataPath), j, outError))
            return false;
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
    for (auto& entry : j)
        outShaders.push_back(parseShader(entry));
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
    for (auto& entry : j)
        outPrograms.push_back(parseProgram(entry));
    return true;
}

bool CaptureLoader::parseVertexArrays(const std::string&     directoryPath,
                                       const CaptureManifest& manifest,
                                       std::vector<CaptureVertexArrayObject>& outVertexArrays,
                                       std::string&                         outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_vertexArraysPath);
    if (filePath.empty()) return true; // vaos.json might be absent
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "vaos.json is not an array"; return false; }
    for (auto& entry : j)
        outVertexArrays.push_back(parseVertexArrayObject(entry));
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
    for (auto& entry : j)
        outFramebuffers.push_back(parseFramebuffer(entry));
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
                                   std::vector<Command>&  outCommands,
                                   std::string&           outError) {
    std::string filePath = joinPath(directoryPath, manifest.m_files.m_commandsPath);
    Json j;
    if (!readJsonFile(filePath, j, outError)) return false;
    if (!j.is_array()) { outError = "commands.json is not an array"; return false; }
    for (auto& entry : j)
        outCommands.push_back(parseCommand(entry));
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
