#ifndef HIT_TAG_H
#define HIT_TAG_H

#include "ns3/network-module.h"
#include <stack>

using namespace ns3;
using std::stack;

class HitTag : public Tag
{
public:
  HitTag ();

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

  void SetAddress (Ipv4Address addr);
  Ipv4Address GetAddress (void);

  void SetId (uint32_t id);
  uint32_t GetId (void);
private:
  Ipv4Address m_address;
  uint32_t m_switchId;
};

#endif /* HIT_TAG_H */
