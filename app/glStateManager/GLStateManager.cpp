
// 注意：本文件需要 GL 函数加载器。下面按 GLAD 写；若你用 GLEW/其它，
// 把这一行换成对应头文件即可（确保在包含前 loader 已初始化）。
#include <glad/gl.h>

// ---- GL 4.x 常量补丁 ----
// 当前 glad 基于 GL 3.3 compatibility；以下 GL 4.1 / 4.3 常量不在其中，
// 但对应的查询函数（glGetIntegeri_v 等）已由 glad 声明，运行时由驱动实现。
// 常量均为既定 GLenum 值，手动定义仅用于编译通过，不会改变运行时行为。
#ifndef GL_PROGRAM_PIPELINE_BINDING
#define GL_PROGRAM_PIPELINE_BINDING   0x825A
#endif
#ifndef GL_VERTEX_ATTRIB_BINDING
#define GL_VERTEX_ATTRIB_BINDING      0x82D4
#endif
#ifndef GL_VERTEX_ATTRIB_RELATIVE_OFFSET
#define GL_VERTEX_ATTRIB_RELATIVE_OFFSET 0x82D5
#endif
#ifndef GL_VERTEX_BINDING_DIVISOR
#define GL_VERTEX_BINDING_DIVISOR     0x82D6
#endif
#ifndef GL_VERTEX_BINDING_OFFSET
#define GL_VERTEX_BINDING_OFFSET      0x82D7
#endif
#ifndef GL_VERTEX_BINDING_STRIDE
#define GL_VERTEX_BINDING_STRIDE      0x82D8
#endif
#ifndef GL_MAX_VERTEX_ATTRIB_BINDINGS
#define GL_MAX_VERTEX_ATTRIB_BINDINGS 0x82DA
#endif
#ifndef GL_VERTEX_BINDING_BUFFER
#define GL_VERTEX_BINDING_BUFFER      0x8F4F
#endif
#ifndef GL_VERTEX_ATTRIB_ARRAY_LONG
#define GL_VERTEX_ATTRIB_ARRAY_LONG   0x874E
#endif

#include "GLStateManager.h"

#include <algorithm> // std::min

namespace {

// ---- 小工具：单值查询 ----
inline GLint geti(GLenum pname) { GLint v = 0; glGetIntegerv(pname, &v); return v; }
inline GLfloat getf(GLenum pname) { GLfloat v = 0.0f; glGetFloatv(pname, &v); return v; }
inline bool getb(GLenum pname) { GLboolean v = GL_FALSE; glGetBooleanv(pname, &v); return v == GL_TRUE; }
inline bool isOn(GLenum cap) { return glIsEnabled(cap) == GL_TRUE; }

// TextureTarget -> 对应的 GL_TEXTURE_BINDING_* 查询枚举（顺序须与枚举定义一致）
GLenum textureBindingPName(TextureTarget t) {
    switch (t) {
        case TextureTarget::Tex1D:        return GL_TEXTURE_BINDING_1D;
        case TextureTarget::Tex2D:        return GL_TEXTURE_BINDING_2D;
        case TextureTarget::Tex3D:        return GL_TEXTURE_BINDING_3D;
        case TextureTarget::Tex1DArray:   return GL_TEXTURE_BINDING_1D_ARRAY;
        case TextureTarget::Tex2DArray:   return GL_TEXTURE_BINDING_2D_ARRAY;
        case TextureTarget::TexRectangle: return GL_TEXTURE_BINDING_RECTANGLE;
        case TextureTarget::TexBuffer:    return GL_TEXTURE_BINDING_BUFFER;
        case TextureTarget::TexCube:      return GL_TEXTURE_BINDING_CUBE_MAP;
        case TextureTarget::TexCubeArray: return -1; // Not supported in OpenGL
        case TextureTarget::Tex2DMS:      return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
        case TextureTarget::Tex2DMSArray: return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
        default:                          return 0;
    }
}

// ---- 资源绑定：纹理 / 采样器 / UBO ----
ResourceBindingState captureResourceBindings() {
    ResourceBindingState s;

    const GLint maxUnits = geti(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    const size_t units = std::min<size_t>(static_cast<size_t>(maxUnits), MAX_TEXTURE_UNITS);

    const GLint originalActive = geti(GL_ACTIVE_TEXTURE); // GL_TEXTURE0 + slot
    s.m_activeTextureSlot = static_cast<uint32_t>(originalActive - GL_TEXTURE0);

    for (size_t u = 0; u < units; ++u) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + u));
        TextureUnitState& unit = s.m_textureUnits[u];

        for (size_t t = 0; t < static_cast<size_t>(TextureTarget::Count); ++t) {
            const GLenum pname = textureBindingPName(static_cast<TextureTarget>(t));
            GLint tex = 0;
            glGetIntegerv(pname, &tex);
            if (tex != 0) unit.m_textureBindings[t] = static_cast<uint32_t>(tex);
        }
        const GLint smp = geti(GL_SAMPLER_BINDING); // 针对当前 active 单元
        if (smp != 0) unit.m_samplerId = static_cast<uint32_t>(smp);
    }
    glActiveTexture(static_cast<GLenum>(originalActive)); // 还原，保持无副作用

    const GLint maxUbo = geti(GL_MAX_UNIFORM_BUFFER_BINDINGS);
    const size_t ubos = std::min<size_t>(static_cast<size_t>(maxUbo), MAX_UNIFORM_BUFFERS);
    for (size_t i = 0; i < ubos; ++i) {
        GLint buf = 0;
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, static_cast<GLuint>(i), &buf);
        if (buf != 0) {
            GLint64 off = 0, sz = 0;
            glGetInteger64i_v(GL_UNIFORM_BUFFER_START, static_cast<GLuint>(i), &off);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE,  static_cast<GLuint>(i), &sz);
            UniformBufferBinding b;
            b.m_bufferId = static_cast<uint32_t>(buf);
            b.m_offset   = off;
            b.m_size     = sz;
            s.m_uniformBufferBindings[i] = b;
        }
    }
    return s;
}

// ---- 着色器：当前 program / program pipeline ----
ShaderState captureShader() {
    ShaderState s;
    const GLint prog = geti(GL_CURRENT_PROGRAM);
    if (prog != 0) s.m_programId = static_cast<uint32_t>(prog);
    const GLint pipe = geti(GL_PROGRAM_PIPELINE_BINDING);
    if (pipe != 0) s.m_programPipelineId = static_cast<uint32_t>(pipe);
    return s;
}

// ---- 顶点输入：VAO + 属性格式 + binding + EBO ----
VertexInputState captureVertexInput() {
    VertexInputState s;
    const GLint vao = geti(GL_VERTEX_ARRAY_BINDING);
    if (vao != 0) s.m_vaoId = static_cast<uint32_t>(vao);

    // core profile 下没有绑定 VAO 时，下面的属性/binding 查询会产生 GL 错误，
    // 故仅在确有 VAO 时才展开（绘制本就要求绑定 VAO）。
    if (vao != 0) {
        const GLint maxAttribs = geti(GL_MAX_VERTEX_ATTRIBS);
        const size_t na = std::min<size_t>(static_cast<size_t>(maxAttribs), MAX_VERTEX_ATTRIBUTES);
        for (size_t i = 0; i < na; ++i) {
            VertexAttributeState& a = s.m_attributes[i];
            GLint v = 0;
            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_ENABLED,&v); 
            a.m_enabled = (v != 0);

            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_SIZE,&v); 
            a.m_size = v;

            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_TYPE,&v); 
            a.m_type = static_cast<uint32_t>(v);

            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&v); 
            a.m_normalized = (v != 0);

            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &v);
            a.m_relativeOffset = static_cast<uint32_t>(v);

            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_BINDING,               &v); 
            a.m_bindingIndex = static_cast<uint32_t>(v);

            GLint isInt = 0, isLong = 0;
            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &isInt);
            glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_LONG,    &isLong);
            a.m_formatKind = isLong ? AttribFormatKind::Long
                           : (isInt ? AttribFormatKind::Int : AttribFormatKind::Float);
        }

        const GLint maxBind = geti(GL_MAX_VERTEX_ATTRIB_BINDINGS);
        const size_t nb = std::min<size_t>(static_cast<size_t>(maxBind), MAX_VERTEX_ATTRIB_BINDINGS);
        for (size_t i = 0; i < nb; ++i) {
            VertexBufferBinding& b = s.m_bindings[i];
            GLint   buf = 0; glGetIntegeri_v  (GL_VERTEX_BINDING_BUFFER,  (GLuint)i, &buf); b.m_buffer = static_cast<uint32_t>(buf);
            GLint64 off = 0; glGetInteger64i_v(GL_VERTEX_BINDING_OFFSET,  (GLuint)i, &off); b.m_offset = off;
            GLint   str = 0; glGetIntegeri_v  (GL_VERTEX_BINDING_STRIDE,  (GLuint)i, &str); b.m_stride = str;
            GLint   div = 0; glGetIntegeri_v  (GL_VERTEX_BINDING_DIVISOR, (GLuint)i, &div); b.m_divisor = static_cast<uint32_t>(div);
        }

        const GLint ebo = geti(GL_ELEMENT_ARRAY_BUFFER_BINDING); // 属于当前 VAO
        if (ebo != 0) s.m_elementBuffer = static_cast<uint32_t>(ebo);
    }
    return s;
}

// ---- 帧缓冲：绑定 + draw buffers + read buffer ----
FramebufferState captureFramebuffer() {
    FramebufferState s;
    s.m_drawFramebuffer = static_cast<uint32_t>(geti(GL_DRAW_FRAMEBUFFER_BINDING));
    s.m_readFramebuffer = static_cast<uint32_t>(geti(GL_READ_FRAMEBUFFER_BINDING));

    const GLint maxDraw = geti(GL_MAX_DRAW_BUFFERS);
    const size_t nd = std::min<size_t>(static_cast<size_t>(maxDraw), MAX_DRAW_BUFFERS);
    for (size_t i = 0; i < nd; ++i) {
        s.m_drawBuffers[i] = static_cast<uint32_t>(geti(static_cast<GLenum>(GL_DRAW_BUFFER0 + i)));
    }
    s.m_drawBufferCount = static_cast<uint32_t>(nd);
    s.m_readBuffer = static_cast<uint32_t>(geti(GL_READ_BUFFER));
    return s;
}

// ---- 光栅化 ----
RasterizerState captureRasterizer() {
    RasterizerState s;
    s.m_cullFaceEnabled = isOn(GL_CULL_FACE);
    s.m_cullMode  = static_cast<CullMode>(geti(GL_CULL_FACE_MODE));
    s.m_frontFace = static_cast<FrontFace>(geti(GL_FRONT_FACE));

    GLint pm[2] = {0, 0};
    glGetIntegerv(GL_POLYGON_MODE, pm); // core 下 front/back 相同，取其一
    s.m_polygonMode = static_cast<PolygonMode>(pm[0]);

    s.m_scissorTestEnabled = isOn(GL_SCISSOR_TEST);
    GLint sb[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_SCISSOR_BOX, sb);
    for (int i = 0; i < 4; ++i) s.m_scissorBox[i] = sb[i];

    s.m_polygonOffsetFillEnabled = isOn(GL_POLYGON_OFFSET_FILL);
    s.m_polygonOffsetFactor = getf(GL_POLYGON_OFFSET_FACTOR);
    s.m_polygonOffsetUnits  = getf(GL_POLYGON_OFFSET_UNITS);

    s.m_lineWidth = getf(GL_LINE_WIDTH);
    s.m_multisampleEnabled       = isOn(GL_MULTISAMPLE);
    s.m_depthClampEnabled        = isOn(GL_DEPTH_CLAMP);
    s.m_rasterizerDiscardEnabled = isOn(GL_RASTERIZER_DISCARD);
    return s;
}

// ---- 深度 + 模板 ----
DepthStencilState captureDepthStencil() {
    DepthStencilState s;
    s.m_depthTestEnabled  = isOn(GL_DEPTH_TEST);
    s.m_depthFunc         = static_cast<CompareFunc>(geti(GL_DEPTH_FUNC));
    s.m_depthWriteEnabled = getb(GL_DEPTH_WRITEMASK);

    GLfloat dr[2] = {0.0f, 0.0f};
    glGetFloatv(GL_DEPTH_RANGE, dr);
    s.m_depthRangeNear = dr[0];
    s.m_depthRangeFar  = dr[1];

    s.m_stencilTestEnabled = isOn(GL_STENCIL_TEST);

    s.m_front.m_func        = static_cast<CompareFunc>(geti(GL_STENCIL_FUNC));
    s.m_front.m_ref         = geti(GL_STENCIL_REF);
    s.m_front.m_compareMask = static_cast<uint32_t>(geti(GL_STENCIL_VALUE_MASK));
    s.m_front.m_writeMask   = static_cast<uint32_t>(geti(GL_STENCIL_WRITEMASK));
    s.m_front.m_sfail       = static_cast<StencilOp>(geti(GL_STENCIL_FAIL));
    s.m_front.m_dpfail      = static_cast<StencilOp>(geti(GL_STENCIL_PASS_DEPTH_FAIL));
    s.m_front.m_dppass      = static_cast<StencilOp>(geti(GL_STENCIL_PASS_DEPTH_PASS));

    s.m_back.m_func        = static_cast<CompareFunc>(geti(GL_STENCIL_BACK_FUNC));
    s.m_back.m_ref         = geti(GL_STENCIL_BACK_REF);
    s.m_back.m_compareMask = static_cast<uint32_t>(geti(GL_STENCIL_BACK_VALUE_MASK));
    s.m_back.m_writeMask   = static_cast<uint32_t>(geti(GL_STENCIL_BACK_WRITEMASK));
    s.m_back.m_sfail       = static_cast<StencilOp>(geti(GL_STENCIL_BACK_FAIL));
    s.m_back.m_dpfail      = static_cast<StencilOp>(geti(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
    s.m_back.m_dppass      = static_cast<StencilOp>(geti(GL_STENCIL_BACK_PASS_DEPTH_PASS));
    return s;
}

// ---- 混合 ----
BlendState captureBlend() {
    BlendState s;
    s.m_blendEnabled  = isOn(GL_BLEND); // 注意：这是全局开关，不含 indexed(per-draw-buffer)
    s.m_srcRGB        = static_cast<BlendFactor>(geti(GL_BLEND_SRC_RGB));
    s.m_dstRGB        = static_cast<BlendFactor>(geti(GL_BLEND_DST_RGB));
    s.m_srcAlpha      = static_cast<BlendFactor>(geti(GL_BLEND_SRC_ALPHA));
    s.m_dstAlpha      = static_cast<BlendFactor>(geti(GL_BLEND_DST_ALPHA));
    s.m_rgbEquation   = static_cast<BlendEquation>(geti(GL_BLEND_EQUATION_RGB));
    s.m_alphaEquation = static_cast<BlendEquation>(geti(GL_BLEND_EQUATION_ALPHA));

    glGetFloatv(GL_BLEND_COLOR, s.m_blendColor.data());

    GLboolean cm[4] = {GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE};
    glGetBooleanv(GL_COLOR_WRITEMASK, cm);
    for (int i = 0; i < 4; ++i) s.m_colorMask[i] = (cm[i] == GL_TRUE);
    return s;
}

// ---- 像素存储 ----
PixelStorageState capturePixelStorage() {
    PixelStorageState s;

    PixelStoreParams& p = s.m_pack;
    p.m_swapBytes   = getb(GL_PACK_SWAP_BYTES);
    p.m_lsbFirst    = getb(GL_PACK_LSB_FIRST);
    p.m_rowLength   = geti(GL_PACK_ROW_LENGTH);
    p.m_imageHeight = geti(GL_PACK_IMAGE_HEIGHT);
    p.m_skipRows    = geti(GL_PACK_SKIP_ROWS);
    p.m_skipPixels  = geti(GL_PACK_SKIP_PIXELS);
    p.m_skipImages  = geti(GL_PACK_SKIP_IMAGES);
    p.m_alignment   = geti(GL_PACK_ALIGNMENT);

    PixelStoreParams& u = s.m_unpack;
    u.m_swapBytes   = getb(GL_UNPACK_SWAP_BYTES);
    u.m_lsbFirst    = getb(GL_UNPACK_LSB_FIRST);
    u.m_rowLength   = geti(GL_UNPACK_ROW_LENGTH);
    u.m_imageHeight = geti(GL_UNPACK_IMAGE_HEIGHT);
    u.m_skipRows    = geti(GL_UNPACK_SKIP_ROWS);
    u.m_skipPixels  = geti(GL_UNPACK_SKIP_PIXELS);
    u.m_skipImages  = geti(GL_UNPACK_SKIP_IMAGES);
    u.m_alignment   = geti(GL_UNPACK_ALIGNMENT);

    s.m_pixelPackBuffer   = static_cast<uint32_t>(geti(GL_PIXEL_PACK_BUFFER_BINDING));
    s.m_pixelUnpackBuffer = static_cast<uint32_t>(geti(GL_PIXEL_UNPACK_BUFFER_BINDING));
    return s;
}

} // namespace

GLStateSnapshot GLStateManager::CaptureCurrentState() {
    // 可选：先清掉已有的错误标志，便于事后用 glGetError 判断捕获本身是否干净。
    // while (glGetError() != GL_NO_ERROR) {}
    GLStateSnapshot snap;
    snap.m_resourceBindings = captureResourceBindings();
    snap.m_shader           = captureShader();
    snap.m_vertexInput      = captureVertexInput();
    snap.m_framebuffer      = captureFramebuffer();
    snap.m_rasterizer       = captureRasterizer();
    snap.m_depthStencil     = captureDepthStencil();
    snap.m_blend            = captureBlend();
    snap.m_pixelStorage     = capturePixelStorage();
    return snap;
}