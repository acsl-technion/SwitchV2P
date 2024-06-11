#ifndef ILP_CONTROLLER_APP_H
#define ILP_CONTROLLER_APP_H

#include "ns3/applications-module.h"
#include "sim-base.h"
#include "ns3/core-module.h"
#include "switch-app.h"

class IlpControllerApp : public Application
{
public:
  IlpControllerApp ();
  void Setup (ContainerGroups containerGroups, ApplicationContainer switchApps, uint32_t leafCount,
              uint32_t spineCount, uint32_t coreCount, uint32_t podWidth,
              unordered_map<uint32_t, uint32_t> *virtualToPhysical,
              unordered_map<string, uint32_t> *containerToId, vector<pair<uint32_t, uint32_t>> *gws,
              vector<Ipv4Address> *gwAddresses);
  void QuerySwitches ();
  static TypeId GetTypeId (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  uint32_t GetPacketCost (uint32_t srcContainerId, uint32_t dstContainerId, uint32_t steps,
                          uint32_t gwLeaf, uint32_t gwPod);
  uint64_t GetFlowHash (uint32_t podId, uint32_t leafOffset, uint32_t id, Ipv4Address gwAddress);

  ContainerGroups m_containerGroups;
  ApplicationContainer m_switchApps;
  uint32_t m_leafCount, m_spineCount, m_coreCount, m_podWidth;
  EventId m_sendEvent;
  bool m_stop;
  unordered_map<uint32_t, uint32_t> *m_virtualToPhysical;
  unordered_map<string, uint32_t> *m_containerToId;
  unordered_map<uint32_t, pair<uint32_t, uint32_t>> m_containerToPhysicalLocation;
  vector<pair<uint32_t, uint32_t>> *m_gws;
  vector<Ipv4Address> *m_gwAddresses;
  vector<TrafficMatrix> m_leavesTrafficMatrix, m_spinesTrafficMatrix, m_coresTrafficMatrix;
  Time m_interval;
  int m_gwCost;
  int m_memorySize;
  Hasher m_hasher;
};

#endif /* ILP_CONTROLLER_APP_H */