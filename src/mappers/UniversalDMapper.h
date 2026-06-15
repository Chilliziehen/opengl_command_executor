/**
 * @file UniversalDMapper.h
 * @brief Bidirectional capture-id ↔ GL-handle mapper (global singleton)
 */
#ifndef UNIVERSAL_DMAPPER_H
#define UNIVERSAL_DMAPPER_H

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include "core/Singleton.h"

template <typename TKey, typename TVal = uint32_t>
class UniversalDMapper : public Singleton<UniversalDMapper<TKey, TVal>> {
    friend class Singleton<UniversalDMapper<TKey, TVal>>;

public:
    void addMapping(const TKey& key, const TVal& val) {
        m_mappingKeyToVal[key] = val;
        m_mappingValToKey[val] = key;
    }

    void removeMapping(const TKey& key) {
        auto it = m_mappingKeyToVal.find(key);
        if (it != m_mappingKeyToVal.end()) {
            m_mappingValToKey.erase(it->second);
            m_mappingKeyToVal.erase(it);
        }
    }

    TVal getVal(const TKey& key) const {
        auto it = m_mappingKeyToVal.find(key);
        if (it != m_mappingKeyToVal.end()) return it->second;
        throw std::runtime_error("Key not found in mapping");
    }

    TKey getKey(const TVal& val) const {
        auto it = m_mappingValToKey.find(val);
        if (it != m_mappingValToKey.end()) return it->second;
        throw std::runtime_error("Value not found in mapping");
    }

    bool containsKey(const TKey& key) const {
        return m_mappingKeyToVal.find(key) != m_mappingKeyToVal.end();
    }

    bool containsVal(const TVal& val) const {
        return m_mappingValToKey.find(val) != m_mappingValToKey.end();
    }

    /// Drop every mapping (does NOT free the underlying GL objects — callers are
    /// responsible for deleting those first).
    void clear() {
        m_mappingKeyToVal.clear();
        m_mappingValToKey.clear();
    }

private:
    UniversalDMapper() = default;
    std::unordered_map<TKey, TVal> m_mappingKeyToVal;
    std::unordered_map<TVal, TKey> m_mappingValToKey;
};

/// Per-resource-type mapper tags (each type gets its own singleton)
struct BufferMapperTag      {};
struct TextureMapperTag     {};
struct ShaderMapperTag      {};
struct ProgramMapperTag     {};
struct VertexArrayMapperTag {};
struct FramebufferMapperTag {};

/// Unique key: captures both resource type and capture ID in one uint64_t.
/// High 8 bits = resource kind, low 56 bits = capture ID.
/// This avoids collisions between e.g. texture #4 and shader #4.
enum class ResourceKind : uint8_t {
    Buffer      = 1,
    Texture     = 2,
    Shader      = 3,
    Program     = 4,
    VertexArray = 5,
    Framebuffer = 6
};

inline uint64_t encodeResourceKey(ResourceKind kind, uint32_t captureId) {
    return (static_cast<uint64_t>(static_cast<uint8_t>(kind)) << 56) | captureId;
}

/// The single global mapper. Keys use encodeResourceKey(type, id) to prevent collisions.
using ResourceMapper = UniversalDMapper<uint64_t>;

// ---- type-safe inline helpers (avoid changing 40+ call sites) ----
inline void     addMappedHandle(ResourceKind kind, uint32_t captureId, uint32_t glHandle)  { ResourceMapper::instance().addMapping(encodeResourceKey(kind, captureId), glHandle); }
inline uint32_t getMappedHandle(ResourceKind kind, uint32_t captureId)                     { return ResourceMapper::instance().getVal(encodeResourceKey(kind, captureId)); }
inline bool     hasMappedHandle(ResourceKind kind, uint32_t captureId)                     { return ResourceMapper::instance().containsKey(encodeResourceKey(kind, captureId)); }
inline void     removeMappedHandle(ResourceKind kind, uint32_t captureId)                  { ResourceMapper::instance().removeMapping(encodeResourceKey(kind, captureId)); }

/// Drop ALL capture-id → GL-handle mappings. Use when tearing down / reloading
/// a capture. Delete the GL objects (see ResourceAllocator::deleteAllResources)
/// before calling this, otherwise the handles leak.
inline void clearAllMappedHandles() { ResourceMapper::instance().clear(); }

/// Safe lookup: returns `fallback` (default 0) instead of throwing when the
/// capture id has no GL handle yet. Use this in command replay so a single
/// missing/dangling resource id can't take down the whole frame.
inline uint32_t getMappedHandleOr(ResourceKind kind, uint32_t captureId, uint32_t fallback = 0) {
    return hasMappedHandle(kind, captureId) ? getMappedHandle(kind, captureId) : fallback;
}

#endif
