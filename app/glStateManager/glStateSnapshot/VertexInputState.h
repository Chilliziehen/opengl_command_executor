#ifndef VERTEX_INPUT_STATE_H
#define VERTEX_INPUT_STATE_H

// 本文件把 VAO 状态完整序列化（用于存盘 / 对象可能被重建的恢复场景）。
// 现代 core profile 下 VAO 由三段构成，分别对应三组 GL 调用：
//   1) per-attribute 格式：glVertexAttribFormat / *IFormat / *LFormat
//   2) attribute -> binding 映射：glVertexAttribBinding
//   3) per-binding 的 buffer 绑定：glBindVertexBuffer + glVertexBindingDivisor
// 旧式 glVertexAttribPointer 在内部也会被拆成这三段，故本结构同样能覆盖。
// EBO（element array buffer）绑定属于 VAO 内部状态，一并记录。

#include <array>
#include <cstdint>
#include <optional>

// 规范下限均为 16；后续可用glGetIntegerv 查询并确保 >= 真实值：
constexpr std::size_t MAX_VERTEX_ATTRIBUTES         = 16;
constexpr std::size_t MAX_VERTEX_ATTRIB_BINDINGS = 16;

/// @brief 属性格式的“种类”，决定恢复时调用三个格式函数中的哪一个。
/// 这不是 GL 枚举，而是从下面两个查询标志推导出的分派标签：
///   GL_VERTEX_ATTRIB_ARRAY_INTEGER == TRUE  -> Int  (glVertexAttribIFormat)
///   GL_VERTEX_ATTRIB_ARRAY_LONG    == TRUE  -> Long (glVertexAttribLFormat)
///   两者皆 FALSE                            -> Float(glVertexAttribFormat)
enum class AttribFormatKind : uint8_t {
    Float = 0, // 浮点 / 定点归一化，normalized 字段在此种类下才有意义
    Int   = 1, // 整数属性，原样传入着色器（ivec 等）
    Long  = 2, // 64 位（double / dvec）
};

/// @brief 单个顶点属性的格式与映射（对应 glVertexAttribFormat 系列）
struct VertexAttributeState {
    bool             m_enabled       = false;                  // glEnableVertexAttribArray
    AttribFormatKind m_formatKind    = AttribFormatKind::Float;
    int32_t          m_size          = 4;                      // 1..4，或 GL_BGRA(0x80E1)
    uint32_t         m_type          = 0x1406;                 // GLenum，默认 GL_FLOAT
    bool             m_normalized    = false;                  // 仅 Float 种类有意义
    uint32_t         m_relativeOffset = 0;                     // glVertexAttribFormat 的 relativeoffset
    uint32_t         m_bindingIndex   = 0;                     // glVertexAttribBinding，见下方说明

    bool operator==(const VertexAttributeState&) const = default;
};

/// @brief 单个顶点 buffer 绑定点的状态（对应 glBindVertexBuffer）
struct VertexBufferBinding {
    uint32_t m_buffer  = 0;  // 绑定的 VBO，0 表示无；GL_VERTEX_BINDING_BUFFER
    int64_t  m_offset  = 0;  // GLintptr，glBindVertexBuffer 的 offset
    int32_t  m_stride  = 0;  // GLsizei，glBindVertexBuffer 的 stride
    uint32_t m_divisor = 0;  // glVertexBindingDivisor，实例化步进；0 = 每顶点

    bool operator==(const VertexBufferBinding&) const = default;
};

/// @brief 顶点输入状态快照（完整序列化版）
///
/// 注意 m_attribs[i].m_bindingIndex 的默认值：GL 初始 VAO 中，属性 i 默认映射到
/// 绑定点 i（即 bindingIndex == 属性下标）。这个“随下标变化”的默认值无法用统一的
/// 聚合初始化表达，所以这里一律给 0；捕获时务必用
/// glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_BINDING, ...) 读真实值填回，
/// 否则恢复后属性会全部从绑定点 0 取数据。
struct VertexInputState {
    std::optional<uint32_t> m_vaoId; // 当前绑定的 VAO；nullopt = 未记录，0 = 默认 VAO

    std::array<VertexAttributeState,  MAX_VERTEX_ATTRIBUTES>         m_attributes{};
    std::array<VertexBufferBinding, MAX_VERTEX_ATTRIB_BINDINGS> m_bindings{};

    // EBO 绑定，属于 VAO 内部状态；GL_ELEMENT_ARRAY_BUFFER_BINDING。
    // nullopt = 未记录，0 = 无 EBO。
    std::optional<uint32_t> m_elementBuffer;

    bool operator==(const VertexInputState&) const = default;
};

#endif // VERTEX_INPUT_STATE_H