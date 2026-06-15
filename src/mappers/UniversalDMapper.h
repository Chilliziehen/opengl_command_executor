/**
 * @file UniversalDMapper.h
 * @author chenwenjun.liu@unity.cn
 * @brief Universal double mapping utilities for GL command processing
 */

#include <GLFW/glfw3.h>

#ifndef UNIVERSALDMAPPER_H
#define UNIVERSALDMAPPER_H
#include <stdexcept>
#include <unordered_map>


template <typename TKey, typename TVal = uint32_t> class UniversalDMapper {
private:
  std::unordered_map<TKey, TVal> m_mappingKeyToVal;
  std::unordered_map<TVal, TKey> m_mappingValToKey;

public:
  UniversalDMapper() = default;
  void addMapping(const TKey &key, const TVal &val) {
    m_mappingKeyToVal[key] = val;
    m_mappingValToKey[val] = key;
  }

  TVal getVal(const TKey &key) const {
    auto it = m_mappingKeyToVal.find(key);
    if (it != m_mappingKeyToVal.end()) {
      return it->second;
    }
    throw std::runtime_error("Key not found in mapping");
  }

  TKey getKey(const TVal &val) const {
    auto it = m_mappingValToKey.find(val);
    if (it != m_mappingValToKey.end()) {
      return it->second;
    }
    throw std::runtime_error("Value not found in mapping");
  }
};

#endif