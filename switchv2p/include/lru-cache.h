#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <map>
#include <deque>
#include <vector>

using std::deque;
using std::map;
using std::pair;
using std::vector;

template <typename K, typename V>
class LRUCache
{
public:
  size_t m_cacheSize;
  map<K, V> m_map;
  deque<K> m_dq;

  void
  SetCapacity (int capacity)
  {
    m_cacheSize = capacity;
  }

  bool
  Get (K key, V &value)
  {
    if (m_map.find (key) == m_map.end ())
      {
        return false;
      }

    typename deque<K>::iterator it = m_dq.begin ();
    while (*it != key)
      {
        it++;
      }

    m_dq.erase (it);
    m_dq.push_front (key);

    value = m_map[key];
    return true;
  }

  bool
  Find (K key)
  {
    return m_map.find (key) != m_map.end ();
  }

  bool
  Put (K key, V value)
  {
    pair<K, V> p;
    return Put (key, value, p);
  }

  size_t
  GetSize ()
  {
    return m_dq.size ();
  }

  K
  GetEvictionCandidate ()
  {
    return m_dq.back ();
  }

  void
  Remove (K key)
  {
    typename deque<K>::iterator it = m_dq.begin ();
    while (*it != key)
      {
        it++;
      }

    m_dq.erase (it);
    m_map.erase (key);
  }

  vector<K>
  GetKeys ()
  {
    vector<K> vints;
    for (auto const &imap : m_map)
      vints.push_back (imap.first);
    return vints;
  }

  bool
  Put (K key, V value, pair<K, V> &removed)
  {
    bool evict = false;

    if (m_map.find (key) == m_map.end ())
      {
        if (m_cacheSize == m_dq.size ())
          {
            K last = m_dq.back ();
            m_dq.pop_back ();
            removed.first = last;
            removed.second = m_map.at (last);
            m_map.erase (last);
            evict = true;
          }
      }
    else
      {
        typename deque<K>::iterator it = m_dq.begin ();
        while (*it != key)
          {
            it++;
          }

        m_dq.erase (it);
        m_map.erase (key);
      }

    m_dq.push_front (key);
    m_map[key] = value;
    return evict;
  }
};

#endif /* LRU_CACHE_H */