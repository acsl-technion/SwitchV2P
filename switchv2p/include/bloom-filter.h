#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>
#include "ns3/network-module.h"
#include "ns3/core-module.h"

using ns3::CRC32Calculate;
using std::vector;

template <typename K>
class BloomFilter
{
public:
  size_t m_size;
  vector<uint8_t> m_array;

  BloomFilter ()
  {
  }

  size_t
  GetSize ()
  {
    return m_size;
  }

  void
  Setup (int capacity)
  {
    m_size = capacity;
    m_array.reserve (m_size);
    for (size_t i = 0; i < m_size; ++i)
      {
        m_array[i] = 0;
      }
  }

  bool
  Get (K key)
  {
    uint32_t idx = GetIndex (key);
    return m_array[idx] == 1;
  }

  void
  Put (K key)
  {
    uint32_t idx = GetIndex (key);
    m_array[idx] = 1;
  }

private:
  uint32_t
  GetIndex (K key)
  {
    return key;
  }
};

#endif /* BLOOM_FILTER_H */