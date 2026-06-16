#ifndef GL_STATE_SNAPSHOT_H
#define GL_STATE_SNAPSHOT_H

#include <array>
#include <cstdint>

#include "ResourceBindingState.h"
#include "RasterizerState.h"
#include "DepthStencilState.h"
#include "BlendState.h"
#include "FramebufferState.h"
#include "PixelStorageState.h"
#include "VertexInputState.h"
#include "ShaderState.h"

/// @brief 完整 GL 状态快照
struct GLStateSnapshot {
    ResourceBindingState m_resourceBindings{}; // 纹理 / 采样器 / UBO 绑定
    ShaderState          m_shader{};           // 当前 program / program pipeline
    VertexInputState     m_vertexInput{};      // VAO 及其属性/绑定布局、EBO
    FramebufferState     m_framebuffer{};      // draw/read FBO、draw buffers、read buffer
    RasterizerState      m_rasterizer{};       // 剔除、多边形模式、裁剪、偏移等
    DepthStencilState    m_depthStencil{};     // 深度测试/写入 + 模板（前后面分开）
    BlendState           m_blend{};            // 混合因子/方程、blend color、color mask
    PixelStorageState    m_pixelStorage{};     // pack/unpack 参数 + PBO 绑定

    // 视口与清屏色：不属于上面任何子状态，但恢复时同样需要（否则单步预览改了
    // viewport / clearColor 后无法还原）。viewport = {x, y, width, height}。
    std::array<int32_t, 4> m_viewport{0, 0, 0, 0};   // glViewport
    std::array<float, 4>   m_clearColor{0, 0, 0, 0}; // glClearColor

    bool operator==(const GLStateSnapshot&) const = default;
};

#endif // GL_STATE_SNAPSHOT_H