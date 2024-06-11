#ifndef MIGRATION_PARAMS_H
#define MIGRATION_PARAMS_H

#include <cstdint>

class MigrationParams
{
public:
  MigrationParams (bool migration, uint32_t containerId, uint32_t dstLeaf, uint32_t dstHost,
                   uint64_t ts)
      : migration (migration),
        containerId (containerId),
        dstLeaf (dstLeaf),
        dstHost (dstHost),
        ts (ts)
  {
  }
  bool migration;
  uint32_t containerId, dstLeaf, dstHost;
  uint64_t ts;
};

#endif /* MIGRATION_PARAMS_H */