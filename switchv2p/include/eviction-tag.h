#ifndef EVICTION_TAG_H
#define EVICTION_TAG_H

#include "ns3/network-module.h"

using namespace ns3;
using std::pair;

class EvictionTag : public Tag
{
public:
  EvictionTag ();

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  pair<uint32_t, uint32_t> GetEvicted (void) const;
  void SetEvicted (pair<uint32_t, uint32_t> v);

private:
  uint32_t m_key;
  uint32_t m_val;
};

#endif /* EVICTION_TAG_H */
