#ifndef DEPTH_STENCIL_STATE_H
#define DEPTH_STENCIL_STATE_H
#include <cstdint>

// 比较函数 —— 同时用于 glDepthFunc 和 glStencilFunc*
// GL_NEVER / GL_LESS / GL_EQUAL / GL_LEQUAL / GL_GREATER / GL_NOTEQUAL /
// GL_GEQUAL / GL_ALWAYS
enum class CompareFunc : uint32_t {
    Never    = 0x0200,
    Less     = 0x0201,
    Equal    = 0x0202,
    LEqual   = 0x0203,
    Greater  = 0x0204,
    NotEqual = 0x0205,
    GEqual   = 0x0206,
    Always   = 0x0207,
};

// 模板操作 —— glStencilOp* 的三个参数取值
// GL_KEEP / GL_ZERO / GL_REPLACE / GL_INCR / GL_DECR / GL_INVERT /
// GL_INCR_WRAP / GL_DECR_WRAP
enum class StencilOp : uint32_t {
    Keep     = 0x1E00,
    Zero     = 0x0000,
    Replace  = 0x1E01,
    Incr     = 0x1E02,
    Decr     = 0x1E03,
    Invert   = 0x150A,
    IncrWrap = 0x8507,
    DecrWrap = 0x8508,
};

/// @brief 单面（front 或 back）的模板状态。
/// GL 允许 front/back 独立设置（glStencilFuncSeparate / glStencilOpSeparate /
/// glStencilMaskSeparate），完整快照必须分开存。
struct StencilFaceState {
    // glStencilFuncSeparate(face, func, ref, mask)
    CompareFunc m_func        = CompareFunc::Always;
    int32_t     m_ref         = 0;
    uint32_t    m_compareMask = 0xFFFFFFFFu; // 上面 func 用的 mask，初值全 1

    // glStencilMaskSeparate(face, mask)
    uint32_t    m_writeMask   = 0xFFFFFFFFu; // 初值全 1

    // glStencilOpSeparate(face, sfail, dpfail, dppass)
    StencilOp   m_sfail       = StencilOp::Keep; // 模板测试失败
    StencilOp   m_dpfail      = StencilOp::Keep; // 模板过、深度失败
    StencilOp   m_dppass      = StencilOp::Keep; // 模板过、深度过（或无深度测试）

    bool operator==(const StencilFaceState&) const = default;
};

/// @brief 深度 + 模板状态快照（默认值对齐 GL 初始状态）
struct DepthStencilState {
    // ---- 深度 ----
    bool        m_depthTestEnabled  = false;            // GL_DEPTH_TEST
    CompareFunc m_depthFunc         = CompareFunc::Less; // glDepthFunc 默认 GL_LESS
    bool        m_depthWriteEnabled = true;             // glDepthMask，初值 TRUE
    float       m_depthRangeNear    = 0.0f;             // glDepthRange near
    float       m_depthRangeFar     = 1.0f;             // glDepthRange far

    // ---- 模板 ----
    bool             m_stencilTestEnabled = false;      // GL_STENCIL_TEST
    StencilFaceState m_front{};                         // GL_FRONT
    StencilFaceState m_back{};                          // GL_BACK

    bool operator==(const DepthStencilState&) const = default;
};

#endif // DEPTH_STENCIL_STATE_H