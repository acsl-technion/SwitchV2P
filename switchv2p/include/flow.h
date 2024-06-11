#ifndef FLOW_H
#define FLOW_H

#include <cstdint>

class Flow
{
public:
  Flow (uint64_t ts, uint32_t dst, uint32_t flowId, uint32_t size)
      : ts (ts), dst (dst), flowId (flowId), size (size)
  {
  }

  uint64_t ts;
  uint32_t dst;
  uint32_t flowId;
  uint32_t size;
};

#endif /* FLOW_H */
