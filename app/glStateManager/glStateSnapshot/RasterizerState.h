#ifndef RASTERIZER_STATE_H
#define RASTERIZER_STATE_H

// 不包含 GL / 窗口系统头文件，避免命名空间污染。GLenum 用 uint32_t 表达。
//
// 设计约定：下面所有 enum class 的“值”都直接等于对应的 GL 常量值。
// 因此恢复时无需映射表：
//     glCullFace(static_cast<GLenum>(state.m_cullMode));
// 捕获时同样直接：
//     state.m_cullMode = static_cast<CullMode>(queriedGLenum);
// 即使某个 GL token 未被列为具名枚举，从 glGet 拿回的真实值经 static_cast
// 存取也能精确往返（固定底层类型的 enum 接受其底层类型范围内的任意值）。

#include <array>
#include <cstdint>

// glCullFace 的面 —— GL_FRONT / GL_BACK / GL_FRONT_AND_BACK
enum class CullMode : uint32_t {
    Front        = 0x0404,
    Back         = 0x0405,
    FrontAndBack = 0x0408,
};

// glFrontFace 的绕向 —— GL_CW / GL_CCW
enum class FrontFace : uint32_t {
    CW  = 0x0900,
    CCW = 0x0901,
};

// glPolygonMode 的填充模式 —— GL_POINT / GL_LINE / GL_FILL
// 注意：核心剖面下 glPolygonMode 的 face 参数只接受 GL_FRONT_AND_BACK，
// 所以这里只需记一个全局模式。
enum class PolygonMode : uint32_t {
    Point = 0x1B00,
    Line  = 0x1B01,
    Fill  = 0x1B02,
};

/// @brief 光栅化阶段状态快照（默认值对齐 GL 初始状态）
struct RasterizerState {
    // 面剔除
    bool        m_cullFaceEnabled = false;          // GL_CULL_FACE
    CullMode    m_cullMode        = CullMode::Back;  // glCullFace 默认 GL_BACK
    FrontFace   m_frontFace       = FrontFace::CCW;  // glFrontFace 默认 GL_CCW

    // 多边形模式
    PolygonMode m_polygonMode     = PolygonMode::Fill;

    // 裁剪测试。scissorBox = {x, y, width, height}（GL 初值是窗口尺寸，
    // 这里给 0，建议捕获时用 glGetIntegerv(GL_SCISSOR_BOX) 填真实值）。
    bool                      m_scissorTestEnabled = false; // GL_SCISSOR_TEST
    std::array<int32_t, 4>    m_scissorBox{0, 0, 0, 0};

    // 多边形偏移（仅 fill 模式最常用；如需 line/point 偏移可另加开关）
    bool  m_polygonOffsetFillEnabled = false;       // GL_POLYGON_OFFSET_FILL
    float m_polygonOffsetFactor      = 0.0f;        // glPolygonOffset factor
    float m_polygonOffsetUnits       = 0.0f;        // glPolygonOffset units

    // 其它常见光栅化开关
    float m_lineWidth               = 1.0f;         // glLineWidth，GL 初值 1.0
    bool  m_multisampleEnabled      = true;         // GL_MULTISAMPLE，初值 TRUE
    bool  m_depthClampEnabled       = false;        // GL_DEPTH_CLAMP
    bool  m_rasterizerDiscardEnabled = false;       // GL_RASTERIZER_DISCARD

    // 注：float 成员用 == 比较是有意为之 —— 快照存的是捕获时的精确位，
    // 做差异检测时按位相等即“未变化”。
    bool operator==(const RasterizerState&) const = default;
};

#endif // RASTERIZER_STATE_H