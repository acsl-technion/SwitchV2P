#ifndef FLOW_INFO_H
#define FLOW_INFO_H

class FlowInfo
{
public:
  FlowInfo (uint32_t dstIp = 0, uint32_t gwIp = 0, uint32_t packetCount = 0)
      : dstIp (dstIp), gwIp (gwIp), packetCount(packetCount)
  {
  }
  uint32_t dstIp;
  uint32_t gwIp;
  uint32_t packetCount;
};

#endif /* FLOW_INFO_H */