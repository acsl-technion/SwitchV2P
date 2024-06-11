#include "include/ip-utils.h"
#include <string>

using std::string;

Ipv4Mask
IpUtils::GetClassCMask ()
{
  return Ipv4Mask ("255.255.255.0");
}

Ipv4Mask
IpUtils::GetClassBMask ()
{
  return Ipv4Mask ("255.255.0.0");
}

Ipv4Mask
IpUtils::GetClassAMask ()
{
  return Ipv4Mask ("255.0.0.0");
}

Ipv4Address
IpUtils::GetNodeBasePhysicalAddress (uint32_t podId, uint32_t leafOffset, uint32_t id)
{
  string ipBaseStr = std::to_string (podId + 1) + "." + std::to_string (leafOffset) + "." +
                     std::to_string (id) + ".0";
  return Ipv4Address (ipBaseStr.c_str ());
}

Ipv4Address
IpUtils::GetNodePhysicalAddress (uint32_t podId, uint32_t leafOffset, uint32_t id)
{
  return Ipv4Address (GetNodeBasePhysicalAddress (podId, leafOffset, id).Get () + 1);
}

Ipv4Address
IpUtils::GetContainerVirtualAddress (uint32_t id)
{
  return Ipv4Address (id);
}

Ipv4Address
IpUtils::GetSwitchBaseAddress (uint32_t podCount, uint32_t podId, uint32_t spineOffset,
                               uint32_t leafOffset)
{
  string ipBaseStr = std::to_string (podCount + podId + 1) + "." + std::to_string (spineOffset) +
                     "." + std::to_string (leafOffset) + ".0";
  return Ipv4Address (ipBaseStr.c_str ());
}

Ipv4Address
IpUtils::GetLeafAddress (uint32_t podCount, uint32_t podId, uint32_t spineOffset,
                         uint32_t leafOffset)
{

  return Ipv4Address (GetSwitchBaseAddress (podCount, podId, spineOffset, leafOffset).Get () + 2);
}

Ipv4Address
IpUtils::GetSpineFromLeafAddress (uint32_t podCount, uint32_t podId, uint32_t spineOffset,
                                  uint32_t leafOffset)
{
  return Ipv4Address (GetSwitchBaseAddress (2 * podCount, podId, spineOffset, leafOffset).Get () +
                      1);
}

Ipv4Address
IpUtils::GetCoreSwitchBaseAddress (uint32_t spinePodId, uint32_t core)
{
  string ipBaseStr = "255." + std::to_string (core) + "." + std::to_string (spinePodId) + ".0";
  return Ipv4Address (ipBaseStr.c_str ());
}

Ipv4Address
IpUtils::GetCoreAddress (uint32_t spinePodId, uint32_t core)
{
  return Ipv4Address (GetCoreSwitchBaseAddress (spinePodId, core).Get () + 1);
}

Ipv4Address
IpUtils::GetSpineFromCoreAddress (uint32_t spinePodId, uint32_t core)
{
  return Ipv4Address (GetCoreSwitchBaseAddress (spinePodId, core).Get () + 2);
}