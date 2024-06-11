#include "include/invalidation-tag.h"

InvalidationTag::InvalidationTag ()
{
}

TypeId
InvalidationTag::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("InvalidationTag")
          .SetParent<Tag> ()
          .SetGroupName ("Sim")
          .AddConstructor<InvalidationTag> ()
          .AddAttribute ("Freqeuncy", "The time at which the timestamp was created",
                         UintegerValue (0), MakeUintegerAccessor (&InvalidationTag::m_key),
                         MakeUintegerChecker<uint32_t> ());
  return tid;
}

TypeId
InvalidationTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
InvalidationTag::GetSerializedSize (void) const
{
  return 4 * 2;
}

void
InvalidationTag::Serialize (TagBuffer i) const
{
  uint32_t val[2] = {m_key, m_val};
  i.Write ((uint8_t *) val, 2 * sizeof (uint32_t));
}

void
InvalidationTag::Deserialize (TagBuffer i)
{
  uint32_t val[2] = {0};
  i.Read ((uint8_t *) val, 2 * sizeof (uint32_t));
  m_key = val[0];
  m_val = val[1];
}

void
InvalidationTag::Print (std::ostream &os) const
{
  os << "Evicted key=" << m_key << ", evicted val=" << m_val;
}

pair<uint32_t, uint32_t>
InvalidationTag::GetInvalidation (void) const
{
  return std::make_pair (m_key, m_val);
}

void
InvalidationTag::SetInvalidation (pair<uint32_t, uint32_t> val)
{
  m_key = val.first;
  m_val = val.second;
}
