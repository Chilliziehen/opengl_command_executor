#ifndef FRAMEBUFFER_STATE_H
#define FRAMEBUFFER_STATE_H

// 不包含 GL / 窗口系统头文件。GL 句柄与 GLenum 均用 uint32_t 表达。
//
// 关于 draw buffer / read buffer 取值：这些是开放区间的 GLenum
// （GL_COLOR_ATTACHMENT0 + i 可一直递增），无法用具名 enum 穷举，
// 因此直接存原始 uint32_t（从 glGet 拿回什么就存什么，恢复时原样传回）。

#include <array>
#include <cstddef>
#include <cstdint>

// glDrawBuffers 一次能设置的最大数量。GL 规范保证的下限是 8；
// 实际请用 glGetIntegerv(GL_MAX_DRAW_BUFFERS) 查询并确保 >= 真实值。
constexpr std::size_t MAX_DRAW_BUFFERS = 8;

/// @brief 帧缓冲相关状态快照
///
/// 绑定部分（draw/read FBO id）是纯粹的上下文级状态。
/// draw buffers / read buffer 严格说属于“当前绑定的那个 FBO”的对象状态——
/// 绑定切换时会随之改变。这里一并记录，是为了能把“某一时刻看到的完整配置”
/// 复现出来；如果你的快照只关心绑定本身，可只用前两个字段。
struct FramebufferState {
    // glBindFramebuffer：0 表示默认帧缓冲（窗口）。
    // 用 GL_FRAMEBUFFER 绑定会同时改这两个；分别用
    // GL_DRAW_FRAMEBUFFER / GL_READ_FRAMEBUFFER 可使其不同。
    uint32_t m_drawFramebuffer = 0; // GL_DRAW_FRAMEBUFFER_BINDING
    uint32_t m_readFramebuffer = 0; // GL_READ_FRAMEBUFFER_BINDING

    // glDrawBuffers 配置：每个槽是一个 GLenum
    // （GL_NONE / GL_BACK / GL_COLOR_ATTACHMENT0+i ...）。
    // 默认 GL_NONE(0)；捕获时按 glGetIntegerv(GL_DRAW_BUFFER0 + i) 逐个填，
    // 并用 m_drawBufferCount 记录有效数量（恢复时传给 glDrawBuffers 的 n）。
    std::array<uint32_t, MAX_DRAW_BUFFERS> m_drawBuffers{}; // 全 0 = 全 GL_NONE
    uint32_t m_drawBufferCount = 0;

    // glReadBuffer：单个 GLenum（GL_BACK / GL_COLOR_ATTACHMENT0+i ...）。
    uint32_t m_readBuffer = 0x0405; // 默认帧缓冲的初值是 GL_BACK

    bool operator==(const FramebufferState&) const = default;
};

#endif // FRAMEBUFFER_STATE_H