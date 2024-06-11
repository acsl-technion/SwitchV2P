#include "include/timestamp-tag.h"

TimestampTag::TimestampTag () : m_creationTime (Simulator::Now ())
{
}

TypeId
TimestampTag::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("TimestampTag")
          .SetParent<Tag> ()
          .SetGroupName ("Sim")
          .AddConstructor<TimestampTag> ()
          .AddAttribute ("CreationTime", "The time at which the timestamp was created",
                         TimeValue (Time (0)), MakeTimeAccessor (&TimestampTag::m_creationTime),
                         MakeTimeChecker ());
  return tid;
}

TypeId
TimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
TimestampTag::GetSerializedSize (void) const
{
  return 8;
}

void
TimestampTag::Serialize (TagBuffer i) const
{
  i.WriteU64 (m_creationTime.GetTimeStep ());
}

void
TimestampTag::Deserialize (TagBuffer i)
{
  m_creationTime = TimeStep (i.ReadU64 ());
}

void
TimestampTag::Print (std::ostream &os) const
{
  os << "CreationTime=" << m_creationTime;
}

Time
TimestampTag::GetTxTime (void) const
{
  return m_creationTime;
};