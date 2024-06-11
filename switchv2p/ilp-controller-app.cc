#include "include/ilp-controller-app.h"
#include "include/switch-app.h"
#include "include/ip-utils.h"
#include "z3++.h"

using namespace z3;

NS_LOG_COMPONENT_DEFINE ("IlpControllerApp");
NS_OBJECT_ENSURE_REGISTERED (IlpControllerApp);

TypeId
IlpControllerApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("IlpControllerApp")
          .SetParent<Object> ()
          .SetGroupName ("Sim")
          .AddConstructor<IlpControllerApp> ()
          .AddAttribute ("Interval", "The time to wait between switch updates",
                         TimeValue (Seconds (1.0)),
                         MakeTimeAccessor (&IlpControllerApp::m_interval), MakeTimeChecker ())
          .AddAttribute ("GatewayCost", "The cost for gateway packet processing", IntegerValue (40),
                         MakeIntegerAccessor (&IlpControllerApp::m_gwCost),
                         MakeIntegerChecker<int32_t> ())
          .AddAttribute ("MemorySize", "The number of entries each switch can store",
                         IntegerValue (10), MakeIntegerAccessor (&IlpControllerApp::m_memorySize),
                         MakeIntegerChecker<int32_t> ());

  return tid;
}

IlpControllerApp::IlpControllerApp () : m_stop (false)
{
}

void
IlpControllerApp::Setup (ContainerGroups containerGroups, ApplicationContainer switchApps,
                         uint32_t leafCount, uint32_t spineCount, uint32_t coreCount,
                         uint32_t podWidth, unordered_map<uint32_t, uint32_t> *virtualToPhysical,
                         unordered_map<string, uint32_t> *containerToId,
                         vector<pair<uint32_t, uint32_t>> *gws, vector<Ipv4Address> *gwAddresses)
{
  m_containerGroups = containerGroups;
  m_switchApps = switchApps;
  m_leafCount = leafCount;
  m_spineCount = spineCount;
  m_coreCount = coreCount;
  m_podWidth = podWidth;
  m_virtualToPhysical = virtualToPhysical;
  m_containerToId = containerToId;
  m_gws = gws;
  m_gwAddresses = gwAddresses;

  for (uint32_t leaf = 0; leaf < containerGroups.size (); ++leaf)
    {
      for (uint32_t physicalMachine = 0; physicalMachine < containerGroups[leaf].size ();
           ++physicalMachine)
        {
          for (uint32_t container = 0; container < containerGroups[leaf][physicalMachine].size ();
               ++container)
            {
              m_containerToPhysicalLocation[(
                  *m_containerToId)[containerGroups[leaf][physicalMachine][container]]] =
                  std::make_pair (leaf, physicalMachine);
            }
        }
    }
  set_param ("parallel.enable", true);
  set_param ("parallel.threads.max", 5);
}

void
IlpControllerApp::StopApplication (void)
{
  m_stop = true;
  Simulator::Cancel (m_sendEvent);
}

uint32_t
IlpControllerApp::GetPacketCost (uint32_t srcContainerId, uint32_t dstContainerId, uint32_t steps,
                                 uint32_t gwLeaf, uint32_t gwPod)
{
  uint32_t srcLeaf = m_containerToPhysicalLocation[srcContainerId].first;
  uint32_t srcPod = srcLeaf / m_podWidth;
  uint32_t dstLeaf = m_containerToPhysicalLocation[dstContainerId].first;
  uint32_t dstPod = dstLeaf / m_podWidth;

  switch (steps)
    {
    case 1:
      if (srcLeaf == dstLeaf)
        {
          return 2;
        }

      if (srcPod == dstPod)
        {
          return 4;
        }

      return 6;
    case 2:
      if (srcPod == dstPod)
        {
          return 4;
        }

      return 6;

    case 3:
      if (m_coreCount == 0 && dstLeaf == gwLeaf)
        {
          return 4;
        }

      return 6;

    case 4:
      if (dstPod == gwPod)
        {
          return 6;
        }
      return 8;

    default:
      if (gwLeaf == dstLeaf)
        {
          return 6;
        }

      if (gwPod == dstPod)
        {
          return 8;
        }

      return 10;
    }
}

uint64_t
IlpControllerApp::GetFlowHash (uint32_t podId, uint32_t leafOffset, uint32_t id,
                               Ipv4Address gwAddress)
{
  m_hasher.clear ();
  std::ostringstream oss;
  oss << IpUtils::GetNodePhysicalAddress (podId, leafOffset, id) << gwAddress << (uint8_t) 17U
      << 667 << 667;

  std::string data = oss.str ();
  return m_hasher.GetHash64 (data);
}

void
IlpControllerApp::QuerySwitches ()
{
  if (m_stop)
    {
      return;
    }

  NS_LOG_FUNCTION (this);

  m_spinesTrafficMatrix.clear ();
  m_coresTrafficMatrix.clear ();
  m_leavesTrafficMatrix.clear ();
  uint32_t switchCount = m_switchApps.GetN ();

  for (uint32_t switchIdx = 0; switchIdx < switchCount; ++switchIdx)
    {
      Ptr<SwitchApp> switchApp = m_switchApps.Get (switchIdx)->GetObject<SwitchApp> ();
      TrafficMatrix tm = switchApp->GetTrafficMatrix ();
      if (switchIdx < m_leafCount)
        {
          m_leavesTrafficMatrix.push_back (tm);
        }
      else if (switchIdx < m_leafCount + m_spineCount)
        {
          m_spinesTrafficMatrix.push_back (tm);
        }
      else
        {
          m_coresTrafficMatrix.push_back (tm);
        }
    }

  std::set<uint32_t> activeDestinations;
  for (uint32_t leaf = 0; leaf < m_leafCount; ++leaf)
    {
      TrafficMatrix tm = m_leavesTrafficMatrix[leaf];

      // Iterate over the traffic matrix
      for (auto &srcDestMapPair : tm)
        {
          for (auto &destCountPair : srcDestMapPair.second)
            {
              activeDestinations.insert (destCountPair.second.dstIp);
            }
        }
    }

  context ctx;
  optimize opt (ctx);
  size_t containerCount = m_containerToPhysicalLocation.size ();
  expr gatewayCost = ctx.int_val (m_gwCost);
  vector<unordered_map<uint32_t, expr>> switchContainerExpressions (switchCount);

  for (uint32_t i = 0; i < switchCount; ++i)
    {
      expr_vector vec (ctx);
      for (size_t j = 0; j < containerCount; ++j)
        {
          if (activeDestinations.count (j))
            {
              std::string exprName = "m" + std::to_string (i) + "v" + std::to_string (j);
              expr exp = ctx.bool_const (exprName.c_str ());
              switchContainerExpressions[i].emplace (j, exp);
              vec.push_back (exp);
            }
        }

      if (vec.size () > 0)
        {
          expr sum = ite (vec[0], ctx.int_val (1), ctx.int_val (0));
          for (size_t j = 1; j < vec.size (); ++j)
            {
              sum = sum + ite (vec[j], ctx.int_val (1), ctx.int_val (0));
            }
          opt.add (sum <= m_memorySize);
        }
    }

  // Foreach leaf
  expr cost = ctx.int_val (0);
  for (uint32_t leaf = 0; leaf < m_leafCount; ++leaf)
    {
      TrafficMatrix tm = m_leavesTrafficMatrix[leaf];

      // Iterate over the traffic matrix
      for (auto &srcDestMapPair : tm)
        {
          // Skip sources that are not directly attached to the current leaf
          uint32_t srcContainerId = srcDestMapPair.first;
          if (m_containerToPhysicalLocation[srcContainerId].first != leaf)
            {
              NS_LOG_LOGIC (srcContainerId << " is not directly attached to LEAF " << leaf);
              continue;
            }

          NS_LOG_LOGIC (srcContainerId << " IS directly attached to LEAF " << leaf);

          for (auto &destCountPair : srcDestMapPair.second)
            {
              uint32_t dstContainerId = destCountPair.second.dstIp;
              uint32_t gwIdx = std::find (m_gwAddresses->begin (), m_gwAddresses->end (),
                                          Ipv4Address (destCountPair.second.gwIp)) -
                               m_gwAddresses->begin ();
              uint32_t gwLeaf = m_gws->at (gwIdx).first;
              uint32_t gwPod = gwLeaf / m_podWidth;
              expr weight = ctx.int_val (destCountPair.second.packetCount);

              if (leaf == gwLeaf)
                {
                  // The switch is directly attached to the Gateway
                  cost = cost +
                         weight * ite (switchContainerExpressions[leaf].at (dstContainerId),
                                       ctx.int_val (GetPacketCost (srcContainerId, dstContainerId,
                                                                   1, gwLeaf, gwPod)),
                                       gatewayCost);
                }
              else
                {
                  uint32_t srcPod = leaf / m_podWidth;
                  uint64_t flowHash =
                      GetFlowHash (leaf / m_podWidth, leaf % m_podWidth,
                                   m_containerToPhysicalLocation[srcContainerId].second,
                                   m_gwAddresses->at (gwIdx));
                  uint32_t spineId = srcPod * m_podWidth + flowHash % m_podWidth;

                  if (srcPod == gwPod)
                    {
                      // The switch is in the same pod as the Gateway
                      uint32_t dstLeafId = m_containerToPhysicalLocation[dstContainerId].first;

                      // Exact cost
                      // cost =
                      //     cost +
                      //     weight *
                      //         ite (switchContainerExpressions[leaf].at (dstContainerId),
                      //              ctx.int_val (GetPacketCost (srcContainerId, dstContainerId, 1,
                      //                                          gwLeaf, gwPod)),
                      //              ite (switchContainerExpressions[spineId + m_leafCount].at (
                      //                       dstContainerId),
                      //                   ctx.int_val (GetPacketCost (srcContainerId, dstContainerId,
                      //                                               2, gwLeaf, gwPod)),
                      //                   ite (switchContainerExpressions[m_leafCount - 1].at (
                      //                            dstContainerId),
                      //                        ctx.int_val (GetPacketCost (
                      //                            srcContainerId, dstContainerId, 3, gwLeaf, gwPod)),
                      //                        gatewayCost)));
                      // Relaxed cost
                      cost =
                          cost +
                          weight * ite (switchContainerExpressions[leaf].at (dstContainerId) ||
                                            switchContainerExpressions[spineId + m_leafCount].at (
                                                dstContainerId) ||
                                            switchContainerExpressions[gwLeaf].at (dstContainerId),
                                        ctx.int_val (8), gatewayCost);
                      NS_LOG_LOGIC ("The switch is NOT connected to the GW" << spineId << dstLeafId
                                                                            << weight);
                    }
                  else
                    {
                      uint32_t coreId = (spineId % m_podWidth) * (m_coreCount / m_podWidth) +
                                        flowHash % (m_coreCount / m_podWidth);

                      uint32_t secondSpineId =
                          (coreId / (m_coreCount / m_podWidth)) + m_podWidth * gwPod;

                      // Exact cost
                      // cost =
                      //     cost +
                      //     weight *
                      //         ite (switchContainerExpressions[leaf].at (dstContainerId),
                      //              ctx.int_val (GetPacketCost (srcContainerId, dstContainerId, 1,
                      //                                          gwLeaf, gwPod)),
                      //              ite (switchContainerExpressions[spineId + m_leafCount].at (
                      //                       dstContainerId),
                      //                   ctx.int_val (GetPacketCost (srcContainerId, dstContainerId,
                      //                                               2, gwLeaf, gwPod)),
                      //                   ite (switchContainerExpressions[coreId + m_leafCount +
                      //                                                   m_spineCount]
                      //                            .at (dstContainerId),
                      //                        ctx.int_val (GetPacketCost (
                      //                            srcContainerId, dstContainerId, 3, gwLeaf, gwPod)),
                      //                        ite (switchContainerExpressions[secondSpineId +
                      //                                                        m_leafCount]
                      //                                 .at (dstContainerId),
                      //                             ctx.int_val (GetPacketCost (srcContainerId,
                      //                                                         dstContainerId, 4,
                      //                                                         gwLeaf, gwPod)),
                      //                             ite (switchContainerExpressions[m_leafCount - 1]
                      //                                      .at (dstContainerId),
                      //                                  ctx.int_val (GetPacketCost (
                      //                                      srcContainerId, dstContainerId, 5,
                      //                                      gwLeaf, gwPod)),
                      //                                  gatewayCost)))));
                      // Relaxed cost
                      cost =
                          cost +
                          weight *
                              ite (switchContainerExpressions[leaf].at (dstContainerId) ||
                                       switchContainerExpressions[spineId + m_leafCount].at (
                                           dstContainerId) ||
                                       switchContainerExpressions[coreId + m_leafCount +
                                                                  m_spineCount]
                                           .at (dstContainerId) ||
                                       switchContainerExpressions[secondSpineId + m_leafCount].at (
                                           dstContainerId) ||
                                       switchContainerExpressions[gwLeaf].at (dstContainerId),
                                   ctx.int_val (8), gatewayCost);
                    }
                }
            }
        }
    }

  optimize::handle h = opt.minimize (cost);
  opt.check ();
  uint64_t costValue = opt.upper (h).as_int64 ();
  NS_LOG_LOGIC ("COST = " << costValue);
  model m = opt.get_model ();
  for (uint32_t i = 0; i < switchCount; ++i)
    {
      vector<pair<uint32_t, uint32_t>> items;
      for (size_t j = 0; j < containerCount; ++j)
        {
          if (activeDestinations.count (j) &&
              eq (m.eval (switchContainerExpressions[i].at (j)), ctx.bool_val (true)))
            {
              items.push_back (std::make_pair (IpUtils::GetContainerVirtualAddress (j).Get (),
                                               (*m_virtualToPhysical)[j]));
            }
        }

      m_switchApps.Get (i)->GetObject<SwitchApp> ()->BulkInsertToCache (items);
    }

  m_sendEvent = Simulator::Schedule (m_interval, &IlpControllerApp::QuerySwitches, this);
}

void
IlpControllerApp::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::Schedule (Seconds (0.), &IlpControllerApp::QuerySwitches, this);
}
