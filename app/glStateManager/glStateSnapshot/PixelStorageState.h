#ifndef PIXEL_STORAGE_STATE_H
#define PIXEL_STORAGE_STATE_H

// 不包含 GL / 窗口系统头文件。GL 句柄用 uint32_t 表达。
// 这里全部是 glPixelStorei / glPixelStoref 设置的客户端像素打包/解包参数，
// 直接用对应的标量类型存取即可，无需枚举映射。

#include <cstdint>

/// @brief 一组 pixel store 参数（pack 与 unpack 字段完全对称，共用此结构）。
/// 默认值对齐 GL 初始状态：alignment = 4，其余为 0 / false。
struct PixelStoreParams {
    bool    m_swapBytes   = false; // *_SWAP_BYTES
    bool    m_lsbFirst    = false; // *_LSB_FIRST
    int32_t m_rowLength   = 0;     // *_ROW_LENGTH
    int32_t m_imageHeight = 0;     // *_IMAGE_HEIGHT（仅 3D 纹理相关）
    int32_t m_skipRows    = 0;     // *_SKIP_ROWS
    int32_t m_skipPixels  = 0;     // *_SKIP_PIXELS
    int32_t m_skipImages  = 0;     // *_SKIP_IMAGES（仅 3D 纹理相关）
    int32_t m_alignment   = 4;     // *_ALIGNMENT，初值 4

    bool operator==(const PixelStoreParams&) const = default;
};

/// @brief 像素存储状态快照
///
/// pack 影响“从 GL 读出”的操作（glReadPixels / glGetTexImage 等）；
/// unpack 影响“写入 GL”的操作（glTexImage* / glTexSubImage* 等）。
struct PixelStorageState {
    PixelStoreParams m_pack{};   // GL_PACK_*
    PixelStoreParams m_unpack{}; // GL_UNPACK_*

    // 像素缓冲对象（PBO）绑定。它们直接决定上面那些像素传输操作是走客户端
    // 内存还是 PBO，因此和像素存储状态强相关。0 表示未绑定 PBO。
    // 说明：这俩本质是 buffer 绑定，若你更愿意把它们归到 ResourceBindingState
    // 里也合理——放哪都行，关键是别漏掉。
    uint32_t m_pixelPackBuffer   = 0; // GL_PIXEL_PACK_BUFFER_BINDING
    uint32_t m_pixelUnpackBuffer = 0; // GL_PIXEL_UNPACK_BUFFER_BINDING

    bool operator==(const PixelStorageState&) const = default;
};

#endif // PIXEL_STORAGE_STATE_H