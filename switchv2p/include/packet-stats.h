#ifndef PACKET_STATS_H
#define PACKET_STATS_H

class PacketStats
{
public:
  PacketStats (uint64_t txTime = 0, uint64_t rxTime = 0) : txTime (txTime), rxTime (rxTime)
  {
  }

  uint64_t txTime;
  uint64_t rxTime;
};

class FlowStats
{
public:
  FlowStats (PacketStats first = PacketStats (), PacketStats last = PacketStats (),
             uint64_t count = 0, uint64_t disorder = 0)
      : first (first), last (last), count (count), disorder (disorder)
  {
  }
  PacketStats first;
  PacketStats last;
  uint64_t count, disorder;
};

#endif /* PACKET_STATS_H */