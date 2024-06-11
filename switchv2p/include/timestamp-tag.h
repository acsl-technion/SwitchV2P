#ifndef TIMESTAMP_TAG_H
#define TIMESTAMP_TAG_H

#include "ns3/network-module.h"

using namespace ns3;

class TimestampTag : public Tag
{
public:
  TimestampTag ();

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

  /**
   * \brief Get the Transmission time stored in the tag
   * \return the transmission time
   */
  Time GetTxTime (void) const;

private:
  Time m_creationTime; //!< The time stored in the tag
};

#endif /* TIMESTAMP_TAG_H */