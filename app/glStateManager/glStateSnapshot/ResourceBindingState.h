#ifndef RESOURCE_BINDING_STATE_H
#define RESOURCE_BINDING_STATE_H
#include <array>
#include <cstddef>   // std::size_t
#include <cstdint>   // uint32_t / int64_t
#include <optional>

// ============================================================================
// 容量常量
// ----------------------------------------------------------------------------
// 这些是“按绑定点编号索引”的数组维度。务必让它们 >= 设备实际上报的上限，
// 否则按绑定点索引时会越界（UB）。理想做法：初始化时用 glGetIntegerv 查询
//   - GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
//   - GL_MAX_UNIFORM_BUFFER_BINDINGS
// 并在记录快照前对绑定点做范围检查 / 断言。
// 下面给的是较为保守、覆盖大多数现代桌面 GL 实现的默认值；如果你的目标设备
// 报告更高的值，请相应调大（或改用按设备能力分配的 std::vector）。
// ============================================================================
constexpr std::size_t MAX_TEXTURE_UNITS   = 96;
constexpr std::size_t MAX_UNIFORM_BUFFERS = 84;

// ============================================================================
// 纹理目标
// ============================================================================
enum class TextureTarget : std::size_t {
    Tex1D        = 0,
    Tex2D        = 1,
    Tex3D        = 2,
    Tex1DArray   = 3,
    Tex2DArray   = 4,
    TexRectangle = 5,
    TexBuffer    = 6,
    TexCube      = 7,
    TexCubeArray = 8,
    Tex2DMS      = 9,
    Tex2DMSArray = 10,
    Count        // 仅用于数组维度，不是合法目标
};

/// @brief GLenum -> TextureTarget。
/// @return 成功返回对应目标；遇到未知/不支持的 GLenum 返回 std::nullopt，
///         强制调用方处理失败分支，避免拿 Count 去索引数组造成越界。
[[nodiscard]] constexpr std::optional<TextureTarget>
GLenumToTextureTarget(uint32_t glTarget) noexcept {
    switch (glTarget) {
        case 0x0DE0: return TextureTarget::Tex1D;        // GL_TEXTURE_1D
        case 0x0DE1: return TextureTarget::Tex2D;        // GL_TEXTURE_2D
        case 0x806F: return TextureTarget::Tex3D;        // GL_TEXTURE_3D
        case 0x8C18: return TextureTarget::Tex1DArray;   // GL_TEXTURE_1D_ARRAY
        case 0x8C1A: return TextureTarget::Tex2DArray;   // GL_TEXTURE_2D_ARRAY
        case 0x84F5: return TextureTarget::TexRectangle; // GL_TEXTURE_RECTANGLE
        case 0x8C2A: return TextureTarget::TexBuffer;    // GL_TEXTURE_BUFFER
        case 0x8513: return TextureTarget::TexCube;      // GL_TEXTURE_CUBE_MAP
        case 0x9009: return TextureTarget::TexCubeArray; // GL_TEXTURE_CUBE_MAP_ARRAY
        case 0x9100: return TextureTarget::Tex2DMS;      // GL_TEXTURE_2D_MULTISAMPLE
        case 0x9102: return TextureTarget::Tex2DMSArray; // GL_TEXTURE_2D_MULTISAMPLE_ARRAY
        default:     return std::nullopt;
    }
}

/// @brief TextureTarget -> GLenum。用于从快照恢复时重新绑定。
/// @return 对应的 GL 枚举值；传入 Count 或越界值返回 0（GL_NONE）。
[[nodiscard]] constexpr uint32_t
TextureTargetToGLenum(TextureTarget target) noexcept {
    switch (target) {
        case TextureTarget::Tex1D:        return 0x0DE0;
        case TextureTarget::Tex2D:        return 0x0DE1;
        case TextureTarget::Tex3D:        return 0x806F;
        case TextureTarget::Tex1DArray:   return 0x8C18;
        case TextureTarget::Tex2DArray:   return 0x8C1A;
        case TextureTarget::TexRectangle: return 0x84F5;
        case TextureTarget::TexBuffer:    return 0x8C2A;
        case TextureTarget::TexCube:      return 0x8513;
        case TextureTarget::TexCubeArray: return 0x9009;
        case TextureTarget::Tex2DMS:      return 0x9100;
        case TextureTarget::Tex2DMSArray: return 0x9102;
        case TextureTarget::Count:
        default:                          return 0; // GL_NONE
    }
}

// ============================================================================
// 单个纹理单元（插槽）的状态
// ----------------------------------------------------------------------------
// 每个目标一个槽位。std::optional 用于区分：
//   nullopt -> 该目标未被记录 / 未知
//   value=0 -> 显式解绑（glBindTexture(target, 0)）
// ============================================================================
struct TextureUnitState {
    std::array<std::optional<uint32_t>,
               static_cast<std::size_t>(TextureTarget::Count)> m_textureBindings{};
    std::optional<uint32_t> m_samplerId{};

    bool operator==(const TextureUnitState&) const = default;

    /// @brief 便捷访问器：用 TextureTarget 索引绑定槽（自动转下标）。
    [[nodiscard]] std::optional<uint32_t>&
    binding(TextureTarget target) noexcept {
        return m_textureBindings[static_cast<std::size_t>(target)];
    }
    [[nodiscard]] const std::optional<uint32_t>&
    binding(TextureTarget target) const noexcept {
        return m_textureBindings[static_cast<std::size_t>(target)];
    }
};

// ============================================================================
// 统一缓冲区绑定状态
// ----------------------------------------------------------------------------
// offset / size 对应 GLintptr / GLsizeiptr。
// 约定（请在记录侧保持一致，否则 == 会把语义相同的绑定判为不等）：
//   - glBindBufferBase(target, idx, buf)            -> { buf, 0, 0 }
//   - glBindBufferRange(target, idx, buf, off, sz)  -> { buf, off, sz }
// 即 size == 0 表示“整个 buffer / 由 Base 绑定”，glBindBufferRange 本身不接受 0。
// ============================================================================
struct UniformBufferBinding {
    uint32_t m_bufferId = 0;
    int64_t  m_offset   = 0; // GLintptr
    int64_t  m_size     = 0; // GLsizeiptr，0 表示整个 buffer（见上方约定）

    bool operator==(const UniformBufferBinding&) const = default;
};

// ============================================================================
// 资源绑定状态快照
// ============================================================================
struct ResourceBindingState {
    std::array<TextureUnitState, MAX_TEXTURE_UNITS> m_textureUnits{};

    uint32_t m_activeTextureSlot = 0; // 当前 glActiveTexture 选中的单元下标

    std::array<std::optional<UniformBufferBinding>,
               MAX_UNIFORM_BUFFERS> m_uniformBufferBindings{};

    bool operator==(const ResourceBindingState&) const = default;
};

#endif // RESOURCE_BINDING_STATE_H