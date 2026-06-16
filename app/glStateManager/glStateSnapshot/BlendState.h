#ifndef BLEND_STATE_H
#define BLEND_STATE_H
#include <array>
#include <cstdint>

// 请参见 RasterizerState.h 顶部关于“enum 值 == GL 值”的约定说明。

// 混合因子 —— glBlendFuncSeparate 的四个参数取值。
// 这里列出常用集合；双源因子（GL_SRC1_COLOR 等）未具名，但因“enum 值 == GL 值”，
// 从 glGet 拿回的值经 static_cast 存取仍能精确往返。
enum class BlendFactor : uint32_t {
    Zero                  = 0x0000, // GL_ZERO
    One                   = 0x0001, // GL_ONE
    SrcColor              = 0x0300,
    OneMinusSrcColor      = 0x0301,
    SrcAlpha              = 0x0302,
    OneMinusSrcAlpha      = 0x0303,
    DstAlpha              = 0x0304,
    OneMinusDstAlpha      = 0x0305,
    DstColor              = 0x0306,
    OneMinusDstColor      = 0x0307,
    SrcAlphaSaturate      = 0x0308,
    ConstantColor         = 0x8001,
    OneMinusConstantColor = 0x8002,
    ConstantAlpha         = 0x8003,
    OneMinusConstantAlpha = 0x8004,
};

// 混合方程 —— glBlendEquationSeparate 的取值。
// GL_FUNC_ADD / GL_FUNC_SUBTRACT / GL_FUNC_REVERSE_SUBTRACT / GL_MIN / GL_MAX
enum class BlendEquation : uint32_t {
    Add             = 0x8006,
    Subtract        = 0x800A,
    ReverseSubtract = 0x800B,
    Min             = 0x8007,
    Max             = 0x8008,
};

/// @brief 输出合并阶段的混合状态快照（默认值对齐 GL 初始状态）
///
/// 重要：本结构记录的是“全局”混合状态。若你的渲染用到了按 draw buffer 分别
/// 设置的索引化混合（glEnablei(GL_BLEND, i)、glBlendFunci、glBlendEquationi、
/// glColorMaski），那这一份不够，需要把下面这些字段做成
/// std::array<…, MAX_DRAW_BUFFERS>（容量用 glGetIntegerv(GL_MAX_DRAW_BUFFERS)
/// 查询）。需要的话我可以给索引化版本。
struct BlendState {
    bool m_blendEnabled = false;                       // GL_BLEND

    // glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)
    BlendFactor m_srcRGB   = BlendFactor::One;          // 默认 GL_ONE
    BlendFactor m_dstRGB   = BlendFactor::Zero;         // 默认 GL_ZERO
    BlendFactor m_srcAlpha = BlendFactor::One;
    BlendFactor m_dstAlpha = BlendFactor::Zero;

    // glBlendEquationSeparate(rgb, alpha)
    BlendEquation m_rgbEquation   = BlendEquation::Add; // 默认 GL_FUNC_ADD
    BlendEquation m_alphaEquation = BlendEquation::Add;

    // glBlendColor，初值 (0,0,0,0)。仅当用到 Constant* 因子时才生效。
    std::array<float, 4> m_blendColor{0.0f, 0.0f, 0.0f, 0.0f};

    // glColorMask(r, g, b, a)，初值全 TRUE。严格说颜色写掩码属于输出合并阶段，
    // 放这里比放光栅化状态更贴合 GL 分组。
    std::array<bool, 4> m_colorMask{true, true, true, true};

    bool operator==(const BlendState&) const = default;
};

#endif // BLEND_STATE_H