#include "include/hit-tag.h"

HitTag::HitTag ()
{
}

TypeId
HitTag::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("HitTag").SetParent<Tag> ().SetGroupName ("Sim").AddConstructor<HitTag> ();
  return tid;
}

TypeId
HitTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
HitTag::GetSerializedSize (void) const
{
  return 2 * sizeof (uint32_t);
}

void
HitTag::Serialize (TagBuffer i) const
{
  uint32_t buff[2] = {m_address.Get (), m_switchId};
  i.Write ((uint8_t *) buff, GetSerializedSize ());
}

void
HitTag::Deserialize (TagBuffer i)
{
  uint32_t buff[2] = {0, 0};
  i.Read ((uint8_t *) buff, GetSerializedSize ());
  m_address = Ipv4Address (buff[0]);
  m_switchId = buff[1];
}

void
HitTag::Print (std::ostream &os) const
{
  os << "Hit tag";
}

void
HitTag::SetAddress (Ipv4Address addr)
{
  m_address = addr;
}

Ipv4Address
HitTag::GetAddress (void)
{
  return m_address;
}

void
HitTag::SetId (uint32_t id)
{
  m_switchId = id;
}

uint32_t
HitTag::GetId (void)
{
  return m_switchId;
}
