#ifndef SHADER_STATE_H
#define SHADER_STATE_H

// 这里记录的是“着色器相关的上下文级绑定”，靠重新绑定已存在的对象来恢复。
// uniform 的值属于 program 对象内部状态：只要恢复时该 program 对象仍存在，
// 重新 glUseProgram 即可带回其全部 uniform，无需在此序列化。
// 若需要在对象可能被删除/重建的场景下恢复，则要另行序列化每个 uniform 的
// location 与值（以及 program binary / 源码），那是独立且大得多的工作。

#include <cstdint>
#include <optional>

/// @brief 着色器 / 程序绑定状态快照
struct ShaderState {
    // glUseProgram 的当前程序；GL_CURRENT_PROGRAM。
    // nullopt = 未记录，0 = 显式解绑（无程序，或改用 program pipeline）。
    std::optional<uint32_t> m_programId;

    // glBindProgramPipeline 的当前管线；GL_PROGRAM_PIPELINE_BINDING。
    // 仅在使用分离式着色器（separable program）时有意义。
    // nullopt = 未记录，0 = 未绑定管线。
    std::optional<uint32_t> m_programPipelineId;

    bool operator==(const ShaderState&) const = default;
};

#endif // SHADER_STATE_H