#ifndef P4_CACHE_H
#define P4_CACHE_H

#include <map>
#include <deque>
#include <vector>
#include "ns3/network-module.h"
#include "ns3/core-module.h"

using ns3::CRC32Calculate;
using ns3::CreateObject;
using ns3::UniformRandomVariable;
using std::pair;
using std::vector;

template <typename K, typename V>
class P4Cache
{
public:
  size_t m_cacheSize;
  vector<pair<K, V>> m_array;
  vector<uint8_t> m_bits;
  K m_buffer[2];
  ns3::Ptr<UniformRandomVariable> m_random;

  P4Cache () : m_buffer{0, 0}, m_random (CreateObject<UniformRandomVariable> ())
  {
  }

  void
  Setup (int capacity, bool randomHash)
  {
    m_cacheSize = capacity;
    m_array.reserve (m_cacheSize);
    m_bits.reserve (m_cacheSize);
    for (size_t i = 0; i < m_cacheSize; ++i)
      {
        m_array[i].first = 0;
        m_array[i].second = 0;
        m_bits[i] = 0;
      }

    if (randomHash)
      {
        m_buffer[1] = m_random->GetInteger (0, (K) -1);
      }
  }

  bool
  Get (K key, V &value)
  {
    uint32_t idx = GetIndex (key);

    if (m_array[idx].first == key && m_array[idx].second != 0)
      {
        value = m_array[idx].second;
        m_bits[idx] = 1;
        return true;
      }
    m_bits[idx] = 0;
    return false;
  }

  uint8_t
  GetBit (K key)
  {
    uint32_t idx = GetIndex (key);
    return m_bits[idx];
  }

  bool
  Find (K key)
  {
    uint32_t idx = GetIndex (key);
    return m_array[idx].first == key && m_array[idx].second != 0;
  }

  bool
  Get (K key, V &value, uint8_t &bit)
  {
    uint32_t idx = GetIndex (key);

    if (m_array[idx].first == key && m_array[idx].second != 0)
      {
        value = m_array[idx].second;
        bit = m_bits[idx];
        m_bits[idx] = 1;
        return true;
      }

    return false;
  }

  bool
  PutIfNotEvict (K key, V value)
  {
    uint32_t idx = GetIndex (key);
    if (m_array[idx].first != 0 && m_array[idx].first != key)
      {
        return false;
      }
    m_array[idx].first = key;
    m_array[idx].second = value;
    return true;
  }

  void
  Put (K key, V value)
  {
    pair<K, V> evicted;
    Put (key, value, evicted);
  }

  bool
  Put (K key, V value, pair<K, V> &evicted)
  {
    uint32_t idx = GetIndex (key);
    bool eviction = false;
    if (m_array[idx].first != 0 && m_array[idx].first != key)
      {
        evicted = m_array[idx];
        eviction = true;
      }
    m_array[idx].first = key;
    m_array[idx].second = value;
    m_bits[idx] = 0;
    return eviction;
  }

  void
  Remove (K key)
  {
    uint32_t idx = GetIndex (key);
    NS_ASSERT (m_array[idx].first == key);
    m_array[idx].first = 0;
    m_array[idx].second = 0;
    m_bits[idx] = 0;
  }

private:
  uint32_t
  GetIndex (K key)
  {
    m_buffer[0] = key;
    return CRC32Calculate ((uint8_t *) m_buffer, 2 * sizeof (K)) % m_cacheSize;
  }
};

#endif /* P4_CACHE_H */