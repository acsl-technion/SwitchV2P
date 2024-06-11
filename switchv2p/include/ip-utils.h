#ifndef IP_UTILS_H
#define IP_UTILS_H

#include "ns3/network-module.h"

using namespace ns3;

class IpUtils
{
public:
  static Ipv4Mask GetClassAMask ();
  static Ipv4Mask GetClassCMask ();
  static Ipv4Mask GetClassBMask ();
  static Ipv4Address GetNodeBasePhysicalAddress (uint32_t podId, uint32_t leafOffset, uint32_t id);
  static Ipv4Address GetNodePhysicalAddress (uint32_t podId, uint32_t leafOffset, uint32_t id);
  static Ipv4Address GetContainerVirtualAddress (uint32_t id);
  static Ipv4Address GetSwitchBaseAddress (uint32_t podCount, uint32_t podId, uint32_t spineOffset,
                                           uint32_t leafOffset);
  static Ipv4Address GetLeafAddress (uint32_t podCount, uint32_t podId, uint32_t spineOffset,
                                     uint32_t leafOffset);

  static Ipv4Address GetSpineFromLeafAddress (uint32_t podCount, uint32_t podId,
                                              uint32_t spineOffset, uint32_t leafOffset);
  static Ipv4Address GetCoreSwitchBaseAddress (uint32_t spinePodId, uint32_t core);
  static Ipv4Address GetCoreAddress (uint32_t spinePodId, uint32_t core);
  static Ipv4Address GetSpineFromCoreAddress (uint32_t spinePodId, uint32_t core);
};

#endif /* IP_UTILS_H */