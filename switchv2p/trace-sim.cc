#include "include/trace-sim.h"
#include "include/app-helper.h"
#include "include/switch-app.h"
#include "include/client-app-helper.h"
#include "include/gateway-app-helper.h"
#include "include/ip-utils.h"
#include "include/ilp-controller-app-helper.h"
#include "include/switch-app-helper.h"
#include "include/p4-switch-app-helper.h"
#include "ns3/delay-jitter-estimation.h"
#include "ns3/hops-tag.h"
#include <fstream>
#include <sstream>
#include <random>
#include <set>

using std::getline;
using std::ifstream;
using std::istringstream;
using std::stof;
using std::stoi;

NS_LOG_COMPONENT_DEFINE ("TraceSimulation");

TraceSimulation::TraceSimulation (string placementJsonPath, string traceCsvPath,
                                  SimulationParameters simulationParameters, string outputPath,
                                  MigrationParams migrationParams)
    : SimulationBase (simulationParameters,
                      ParsePlacement (placementJsonPath, simulationParameters), migrationParams),
      m_receivedPackets (0),
      m_sentPackets (0),
      m_gwPackets (0),
      m_lastGwPackets (0),
      m_generatedLearning (0),
      m_generatedInvalidation (0),
      m_droppedPackets (0),
      m_totalPacketLatency (0),
      m_totalPacketHops (0),
      m_startTime (Seconds (0)),
      m_stopTime (Seconds (0)),
      m_containerToFlows (ParseTrace (traceCsvPath)),
      m_outputPath (outputPath),
      m_migrationParams (migrationParams)
{
  CalculateDestinations ();
}

void
TraceSimulation::CalculateDestinations ()
{
  for (auto &[key, value] : m_containerToFlows)
    {
      std::sort (value.begin (), value.end (),
                 [] (const Flow &a, const Flow &b) -> bool { return a.ts < b.ts; });

      for (Flow f : value)
        m_destinations.insert (f.dst);
    }

  NS_LOG_DEBUG ("Set size = " << m_destinations.size ());
}

unordered_map<uint32_t, vector<Flow>>
TraceSimulation::ParseTrace (string traceCsvPath)
{
  unordered_map<uint32_t, vector<Flow>> containerToFlows;
  ifstream infile (traceCsvPath);
  if (!infile.is_open ())
    {
      throw std::system_error (errno, std::generic_category (), traceCsvPath);
    }
  infile.exceptions (ifstream::badbit);

  string line;
  while (getline (infile, line))
    {
      istringstream ss (line);
      vector<string> parsedLine;
      string token;
      while (getline (ss, token, ','))
        parsedLine.push_back (token);

      // Get services
      string umContainer = parsedLine[0];
      string dmContainer = parsedLine[1];
      uint64_t ts = stoull (parsedLine[2]);
      uint32_t flowId = stoi (parsedLine[3]);
      uint32_t size = stoi (parsedLine[4]);

      containerToFlows[m_containerToId.at (umContainer)].push_back (
          Flow (ts, m_containerToId.at (dmContainer), flowId, size));
      m_stopTime = Max (m_stopTime, NanoSeconds (ts));
    }

  // This needs to be increased for GwScaling with 4 GWs when packets are  lost. (otherwise follow-me due to app stopping)
  m_stopTime += Minutes (1);

  return containerToFlows;
}

void
TraceSimulation::Configure ()
{
  SimulationBase::Configure ();
  LogComponentEnable ("TraceSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("IlpControllerApp", LOG_LEVEL_DEBUG);
  LogComponentEnable ("P4SwitchApp", LogLevel (LOG_PREFIX_ALL | LOG_ALL));
}

template <typename T>
ptree
TraceSimulation::CreatePtree (unordered_map<uint32_t, T> &map)
{
  ptree mapPtree;
  for (auto &entry : map)
    {
      mapPtree.put (std::to_string (entry.first), std::to_string (entry.second));
    }

  return mapPtree;
}

void
TraceSimulation::StopSimulation ()
{
  SimulationBase::StopSimulation ();
  NS_LOG_INFO ("Total sent packets =  " << m_sentPackets);
  NS_LOG_INFO ("Total receieved packets =  " << m_receivedPackets);
  NS_LOG_INFO ("Total gw packets =  " << m_gwPackets);
  ptree json;
  json.put ("total_tx_packets", m_sentPackets);
  json.put ("total_rx_packets", m_receivedPackets);
  json.put ("total_dropped_packets", m_droppedPackets);
  json.put ("total_gw_packets", m_gwPackets);
  json.put ("total_learning_packets", m_generatedLearning);
  json.put ("total_invalidation_packets", m_generatedInvalidation);
  json.put ("total_misdelivered_packets", m_misdeliveryCount);
  json.put ("last_misdelivered_packet", m_lastMisdelivered.As (Time::US));

  json.add_child ("switch_to_processed_packets", CreatePtree (m_switchToProcessedPackets));
  json.add_child ("switch_to_processed_bytes", CreatePtree (m_switchToProcessedBytes));
  json.add_child ("switch_to_hits", CreatePtree (m_switchToCacheHits));
  json.add_child ("switch_to_first_hits", CreatePtree (m_switchToFirstCacheHits));

  uint64_t totalFct = 0, totalFirstPacketLatency = 0;
  for (pair<uint32_t, FlowStats> fsPair : m_flowStats)
    {
      FlowStats fs = fsPair.second;
      totalFirstPacketLatency += fs.first.rxTime - fs.first.txTime;
      totalFct += fs.last.rxTime - fs.first.txTime;
    }

  json.put ("avg_fpl",
            std::to_string (totalFirstPacketLatency / static_cast<double> (m_flowStats.size ())));
  json.put ("avg_fct", std::to_string (totalFct / static_cast<double> (m_flowStats.size ())));
  json.put ("avg_packet_latency",
            std::to_string (m_totalPacketLatency / static_cast<double> (m_receivedPackets)));
  json.put ("avg_packet_hops",
            std::to_string (m_totalPacketHops / static_cast<double> (m_receivedPackets)));

  std::ofstream outputFile (m_outputPath);
  write_json (outputFile, json);
}

ContainerGroups
TraceSimulation::ParsePlacement (string placementJsonPath,
                                 SimulationParameters simulationParameters)
{
  ptree json;
  read_json (placementJsonPath, json);
  vector<vector<string>> parsedRepresentation;
  for (auto &[k, v] : json)
    {
      vector<string> containers;
      for (auto &[k, v] : boost::make_iterator_range (v.equal_range ("")))
        {
          containers.push_back (v.get_value<string> ());
        }
      parsedRepresentation.push_back (containers);
    }

  size_t nodesPerLeaf = simulationParameters.NumOfPorts / 2;
  size_t gwLeavesIdx = 0;
  size_t nodesIdx = 0;
  size_t totalNodes = parsedRepresentation.size () + simulationParameters.GatewayLeaves.size ();
  size_t totalLeaves = parsedRepresentation.size () / nodesPerLeaf;
  NS_LOG_INFO ("Total nodes = " << totalNodes);
  ContainerGroups placement;
  for (size_t leaf = 0; leaf < totalLeaves; ++leaf)
    {
      NS_LOG_LOGIC ("Deploying leaf #" << leaf << ". Current Idx = " << nodesIdx);
      uint32_t gwCount = 0;
      if (gwLeavesIdx < simulationParameters.GatewayLeaves.size () &&
          simulationParameters.GatewayLeaves[gwLeavesIdx] == leaf)
        {
          uint32_t gwIdx = gwLeavesIdx;
          while (simulationParameters.GatewayLeaves[gwIdx++] == leaf)
            {
            }
          gwCount = gwIdx - gwLeavesIdx - 1;
          NS_LOG_LOGIC ("Deploying " << gwCount << " GWs!");
        }
      size_t serversInLeaf = (nodesIdx + nodesPerLeaf) >= parsedRepresentation.size ()
                                 ? (parsedRepresentation.size () - nodesIdx)
                                 : nodesPerLeaf;

      NS_LOG_LOGIC ("Deploying " << serversInLeaf << " servers");

      vector<vector<string>> nodes;
      for (size_t j = 0; j < serversInLeaf; ++j)
        {
          nodes.push_back (parsedRepresentation[nodesIdx++]);
        }

      for (uint32_t j = 0; j < gwCount; ++j)
        {
          nodes.push_back (vector<string>{"Gateway" + std::to_string (gwLeavesIdx++)});
        }

      placement.push_back (nodes);
    }

  NS_LOG_DEBUG ("Total leaves = " << placement.size ());
  NS_ASSERT (gwLeavesIdx + nodesIdx == totalNodes);
  return placement;
}

void
TraceSimulation::SinkRx (Ptr<const Packet> p, const Address &a, const Time &txTime,
                         const Time &rxTime, const uint32_t &flowId)
{
  NS_LOG_LOGIC ("Received packet from address " << InetSocketAddress::ConvertFrom (a).GetIpv4 ()
                                                << " with delay " << (rxTime - txTime));
  if (m_flowStats.count (flowId) == 0)
    {
      NS_LOG_ERROR ("SinkRX for a flow w/o a ClientTx() event");
      return;
    }
  m_receivedPackets++;
  uint64_t txTimeNs = txTime.ToInteger (Time::NS);
  uint64_t rxTimeNs = rxTime.ToInteger (Time::NS);
  m_totalPacketLatency += rxTimeNs - txTimeNs;

  HopsTag tag;
  if (p->PeekPacketTag (tag))
    {
      m_totalPacketHops += tag.GetHops ();
    }

  PacketStats first = m_flowStats[flowId].first;
  first.txTime = std::min (first.txTime, txTimeNs);
  first.rxTime = std::min (first.rxTime, rxTimeNs);
  PacketStats last = m_flowStats[flowId].last;
  last.txTime = std::max (last.txTime, txTimeNs);
  last.rxTime = std::max (last.rxTime, rxTimeNs);
  uint64_t count = m_flowStats[flowId].count;
  uint64_t disorder = m_flowStats[flowId].disorder;
  m_flowStats[flowId] = FlowStats (first, last, count + 1, disorder);
}

void
TraceSimulation::GatewayRx (Ptr<const Packet>, const Address &)
{
  m_gwPackets++;
}

void
TraceSimulation::RecordDropIp (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload,
                               Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifIndex)
{
  m_droppedPackets++;
}

void
TraceSimulation::RecordDropQueue (Ptr<const Packet> ipPayload)
{
  m_droppedPackets++;
}

void
TraceSimulation::RecordDropQueueDisc (Ptr<const QueueDiscItem> item)
{
  m_droppedPackets++;
}

void
TraceSimulation::RecordCongState (const TcpSocketState::TcpCongState_t oldState,
                                  const TcpSocketState::TcpCongState_t newState,
                                  const uint32_t &flowId)
{
  if (newState == TcpSocketState::TcpCongState_t::CA_DISORDER && m_flowStats.count (flowId) > 0 &&
      flowId != 0)
    {
      m_flowStats[flowId].disorder++;
    }
}

void
TraceSimulation::ClientTx (Ptr<const Packet> packet, const uint32_t &flowId)
{
  m_sentPackets++;
  if (flowId == 0)
    {
      return;
    }

  if (m_flowStats.count (flowId) == 0)
    {
      m_flowStats[flowId] = FlowStats (
          PacketStats (Simulator::Now ().ToInteger (Time::NS), m_stopTime.ToInteger (Time::NS)),
          PacketStats (Simulator::Now ().ToInteger (Time::NS),
                       Simulator::Now ().ToInteger (Time::NS)),
          0);
    }
}

void
TraceSimulation::GeneratedLearning (Ptr<const Packet>)
{
  m_generatedLearning++;
}

void
TraceSimulation::GeneratedInvalidation (Ptr<const Packet>)
{
  m_generatedInvalidation++;
}

void
TraceSimulation::ProcessedPacket (Ptr<const Packet> packet, uint32_t switchId)
{
  m_switchToProcessedPackets[switchId]++;
  m_switchToProcessedBytes[switchId] += packet->GetSize ();
}

void
TraceSimulation::CacheHit (Ptr<const Packet> packet, uint32_t switchId)
{
  m_switchToCacheHits[switchId]++;
  FlowIdTag flowIdTag;
  DelayJitterEstimationTimestampTag tag;
  if (packet->PeekPacketTag (tag) && packet->PeekPacketTag (flowIdTag))
    {
      uint32_t flowId = flowIdTag.GetFlowId ();
      if (m_flowStats.count (flowId) > 0 &&
          m_flowStats[flowId].first.txTime ==
              static_cast<uint64_t> (tag.GetTxTime ().ToInteger (Time::NS)))
        {
          m_switchToFirstCacheHits[switchId]++;
        }
    }
}

void
TraceSimulation::Migration ()
{
  NS_LOG_DEBUG (Simulator::Now () << "MIGRATION. Original src leaf: " << m_migrationSrcLeaf
                                  << ", host = " << m_migrationSrcHost);
  // Remove address and container from source
  Ptr<Node> node = m_nodes[m_migrationSrcLeaf].Get (m_migrationSrcHost);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  uint32_t iface = ipv4->GetNInterfaces () - 1;
  ipv4->RemoveAddress (iface, IpUtils::GetContainerVirtualAddress (m_migrationParams.containerId));
  // Add a dummy address
  ipv4->AddAddress (iface, Ipv4InterfaceAddress (
                               IpUtils::GetContainerVirtualAddress (m_containerToId.size () + 1),
                               IpUtils::GetClassBMask ()));

  // Install the app on the new location
  node = m_nodes[m_migrationParams.dstLeaf].Get (m_migrationParams.dstHost);
  ipv4 = node->GetObject<Ipv4> ();
  iface = ipv4->GetNInterfaces () - 1;
  ipv4->AddAddress (iface, Ipv4InterfaceAddress (
                               IpUtils::GetContainerVirtualAddress (m_migrationParams.containerId),
                               IpUtils::GetClassBMask ()));

  // Update mapping
  Time delay = MicroSeconds (0);
  if (m_simParameters.SimMode == SimulationParameters::Mode::OnDemand)
    {
      delay = MicroSeconds (1000);
    }
  Simulator::Schedule (delay, &TraceSimulation::UpdateMappings, this);
}

void
TraceSimulation::UpdateMappings ()
{
  m_virtualToPhysical[IpUtils::GetContainerVirtualAddress (m_migrationParams.containerId).Get ()] =
      IpUtils::GetNodePhysicalAddress (m_migrationParams.dstLeaf / m_podWidth,
                                       m_migrationParams.dstLeaf % m_podWidth,
                                       m_migrationParams.dstHost)
          .Get ();
}

void
TraceSimulation::SetupApplications ()
{
  for (size_t leaf = 0; leaf < m_containerGroups.size (); ++leaf)
    {
      for (size_t node = 0; node < m_containerGroups[leaf].size (); ++node)
        {
          if (m_migrationParams.migration && node == m_migrationParams.dstHost &&
              leaf == m_migrationParams.dstLeaf)
            {
              PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                           InetSocketAddress (IpUtils::GetContainerVirtualAddress (
                                                                  m_migrationParams.containerId),
                                                              ClientApp::CLIENT_PORT));
              ApplicationContainer serverApps = sinkHelper.Install (
                  m_nodes[m_migrationParams.dstLeaf].Get (m_migrationParams.dstHost));
              serverApps.Get (0)->TraceConnectWithoutContext (
                  "RxWithDelay", MakeCallback (&TraceSimulation::SinkRx, this));
              serverApps.Start (MicroSeconds (m_migrationParams.ts));
              serverApps.Stop (m_stopTime);
            }

          for (size_t container = 0; container < m_containerGroups[leaf][node].size (); ++container)
            {
              if (m_containerGroups[leaf][node][container].find ("Gateway") != string::npos)
                continue;
              uint32_t containerId = m_containerToId[m_containerGroups[leaf][node][container]];
              if (m_destinations.count (containerId))
                {
                  PacketSinkHelper sinkHelper (
                      (m_migrationParams.migration || m_simParameters.UdpMode)
                          ? "ns3::UdpSocketFactory"
                          : "ns3::TcpSocketFactory",
                      InetSocketAddress (IpUtils::GetContainerVirtualAddress (containerId),
                                         ClientApp::CLIENT_PORT));
                  ApplicationContainer serverApps = sinkHelper.Install (m_nodes[leaf].Get (node));
                  serverApps.Get (0)->TraceConnectWithoutContext (
                      "RxWithDelay", MakeCallback (&TraceSimulation::SinkRx, this));
                  serverApps.Get (0)->TraceConnectWithoutContext (
                      "Tx", MakeCallback (&TraceSimulation::ClientTx, this));
                  serverApps.Get (0)->TraceConnectWithoutContext (
                      "CongState", MakeCallback (&TraceSimulation::RecordCongState, this));
                  serverApps.Get (0)->SetAttribute (
                      "Source", UintegerValue (IpUtils::GetNodePhysicalAddress (
                                                   leaf / m_podWidth, leaf % m_podWidth, node)
                                                   .Get ()));
                  serverApps.Start (m_startTime);

                  if (m_migrationParams.migration && containerId == m_migrationParams.containerId)
                    {
                      NS_LOG_DEBUG ("Container ID = " << containerId << ". Leaf = " << leaf);

                      m_migrationSrcLeaf = leaf;
                      m_migrationSrcHost = node;
                      serverApps.Stop (MicroSeconds (m_migrationParams.ts));
                    }
                  else
                    {
                      serverApps.Stop (m_stopTime);
                    }
                }

              if (m_containerToFlows.count (containerId))
                {
                  ClientAppHelper helper (
                      m_containerToFlows[containerId],
                      IpUtils::GetContainerVirtualAddress (containerId),
                      IpUtils::GetNodePhysicalAddress (leaf / m_podWidth, leaf % m_podWidth, node),
                      m_migrationParams.migration || m_simParameters.UdpMode);
                  ApplicationContainer clientApps = helper.Install (m_nodes[leaf].Get (node));
                  clientApps.Get (0)->TraceConnectWithoutContext (
                      "Tx", MakeCallback (&TraceSimulation::ClientTx, this));
                  clientApps.Get (0)->TraceConnectWithoutContext (
                      "CongState", MakeCallback (&TraceSimulation::RecordCongState, this));
                  clientApps.Get (0)->TraceConnectWithoutContext (
                      "RxWithDelay", MakeCallback (&TraceSimulation::SinkRx, this));
                  clientApps.Start (m_startTime);
                  clientApps.Stop (m_stopTime);
                }
            }
        }
    }

  set<uint32_t> gatewayLeaves;
  std::transform (m_gws.begin (), m_gws.end (), inserter (gatewayLeaves, gatewayLeaves.end ()),
                  [] (const pair<uint32_t, uint32_t> &gwPair) { return gwPair.first; });

  if (m_simParameters.SimMode == SimulationParameters::Mode::SwitchV2P ||
      m_simParameters.SimMode == SimulationParameters::Mode::LocalLearning)
    {
      m_switchApps = ApplicationContainer ();
      set<uint32_t> gatewayPods;
      std::transform (
          m_gws.begin (), m_gws.end (), inserter (gatewayPods, gatewayPods.end ()),
          [this] (const pair<uint32_t, uint32_t> &gwPair) { return gwPair.first / m_podWidth; });
      for (uint32_t leaf = 0; leaf < m_leafCount; ++leaf)
        {
          enum P4SwitchApp::SwitchType switchType = gatewayLeaves.count (leaf)
                                                        ? P4SwitchApp::SwitchType::GW_LEAF
                                                        : P4SwitchApp::SwitchType::LEAF;

          P4SwitchAppHelper switchHelper (
              m_gwAddresses,
              IpUtils::GetLeafAddress (m_podCount, leaf / m_podWidth, 0, leaf % m_podWidth),
              switchType, m_simParameters.SimMode, m_podCount, m_virtualToPhysical);

          m_switchApps.Add (switchHelper.Install (m_leaves.Get (leaf)));
        }

      for (uint32_t spine = 0; spine < m_spineCount; ++spine)
        {
          enum P4SwitchApp::SwitchType switchType = gatewayPods.count (spine / m_podWidth)
                                                        ? P4SwitchApp::SwitchType::GW_SPINE
                                                        : P4SwitchApp::SwitchType::SPINE;

          P4SwitchAppHelper switchHelper (
              m_gwAddresses,
              IpUtils::GetSpineFromLeafAddress (m_podCount, spine / m_podWidth, spine % m_podWidth,
                                                0),
              switchType, m_simParameters.SimMode, m_podCount, m_virtualToPhysical);
          m_switchApps.Add (switchHelper.Install (m_spines.Get (spine)));
        }

      for (uint32_t core = 0; core < m_coreCount; ++core)
        {
          P4SwitchAppHelper switchHelper (m_gwAddresses, IpUtils::GetCoreAddress (0, core),
                                          P4SwitchApp::SwitchType::CORE, m_simParameters.SimMode,
                                          m_podCount, m_virtualToPhysical);
          m_switchApps.Add (switchHelper.Install (m_cores.Get (core)));
        }

      for (uint32_t i = 0; i < m_switchApps.GetN (); ++i)
        {
          m_switchApps.Get (i)->TraceConnectWithoutContext (
              "GeneratedLearning", MakeCallback (&TraceSimulation::GeneratedLearning, this));
          m_switchApps.Get (i)->TraceConnectWithoutContext (
              "GeneratedInvalidation",
              MakeCallback (&TraceSimulation::GeneratedInvalidation, this));
        }
    }
  else if (m_simParameters.SimMode == SimulationParameters::Mode::GwCache)
    {
      m_switchApps = ApplicationContainer ();

      for (uint32_t leaf : gatewayLeaves)
        {
          P4SwitchAppHelper switchHelper (
              m_gwAddresses,
              IpUtils::GetLeafAddress (m_podCount, leaf / m_podWidth, 0, leaf % m_podWidth),
              P4SwitchApp::SwitchType::GW_LEAF, SimulationParameters::Mode::LocalLearning,
              m_podCount, m_virtualToPhysical);
          m_switchApps.Add (switchHelper.Install (m_leaves.Get (leaf)));
        }

      SwitchAppHelper switchHelper (m_gwAddresses, SimulationParameters::Mode::NoCache);
      for (uint32_t i = 0; i < m_leafCount; ++i)
        {
          if (gatewayLeaves.count (i))
            {
              continue;
            }
          m_switchApps.Add (switchHelper.Install (m_leaves.Get (i)));
        }

      m_switchApps.Add (switchHelper.Install (m_spines));
      m_switchApps.Add (switchHelper.Install (m_cores));
    }
  else if (m_simParameters.SimMode == SimulationParameters::Mode::Bluebird)
    {
      m_switchApps = ApplicationContainer ();

      for (uint32_t leaf = 0; leaf < m_leafCount; ++leaf)
        {
          P4SwitchAppHelper switchHelper (
              m_gwAddresses,
              IpUtils::GetLeafAddress (m_podCount, leaf / m_podWidth, 0, leaf % m_podWidth),
              P4SwitchApp::SwitchType::LEAF, m_simParameters.SimMode, m_podCount,
              m_virtualToPhysical);

          m_switchApps.Add (switchHelper.Install (m_leaves.Get (leaf)));
        }

      SwitchAppHelper switchHelper (m_gwAddresses, SimulationParameters::Mode::NoCache);
      m_switchApps.Add (switchHelper.Install (m_spines));
      m_switchApps.Add (switchHelper.Install (m_cores));
    }
  else
    {
      SwitchAppHelper switchHelper (m_gwAddresses, m_simParameters.SimMode);
      m_switchApps = switchHelper.Install (NodeContainer (m_leaves, m_spines, m_cores));
    }

  for (uint32_t i = 0; i < m_switchApps.GetN (); ++i)
    {
      m_switchApps.Get (i)->TraceConnectWithoutContext (
          "ProcessedPackets", MakeCallback (&TraceSimulation::ProcessedPacket, this));
      m_switchApps.Get (i)->TraceConnectWithoutContext (
          "CacheHit", MakeCallback (&TraceSimulation::CacheHit, this));
    }
  m_switchApps.Start (m_startTime);
  m_switchApps.Stop (m_stopTime);

  GatewayAppHelper gwHelper (m_virtualToPhysical);
  NodeContainer gws;
  for (size_t i = 0; i < m_gws.size (); ++i)
    {
      gws.Add (m_nodes[m_gws[i].first].Get (m_gws[i].second));
    }
  ApplicationContainer gwApps = gwHelper.Install (gws);
  for (size_t i = 0; i < m_gws.size (); ++i)
    {
      gwApps.Get (i)->TraceConnectWithoutContext ("Rx",
                                                  MakeCallback (&TraceSimulation::GatewayRx, this));
    }
  gwApps.Start (m_startTime);
  gwApps.Stop (m_stopTime);

  if (m_simParameters.SimMode == SimulationParameters::Mode::Controller)
    {
      IlpControllerAppHelper controllerHelper (
          m_containerGroups, m_switchApps, m_leafCount, m_spineCount, m_coreCount, m_podWidth,
          m_virtualToPhysical, m_containerToId, m_gws, m_gwAddresses);
      ApplicationContainer controllerApps = controllerHelper.Install (gws.Get (0));
      controllerApps.Start (m_startTime);
      controllerApps.Stop (m_stopTime);
    }

  if (m_migrationParams.migration)
    {
      Simulator::Schedule (MicroSeconds (m_migrationParams.ts), &TraceSimulation::Migration, this);
    }

  Config::ConnectWithoutContext ("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/*/Drop",
                                 MakeCallback (&TraceSimulation::RecordDropQueueDisc, this));

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/TxQueue/Drop",
                                 MakeCallback (&TraceSimulation::RecordDropQueue, this));

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTxDrop",
                                 MakeCallback (&TraceSimulation::RecordDropQueue, this));

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop",
                                 MakeCallback (&TraceSimulation::RecordDropQueue, this));

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyTxDrop",
                                 MakeCallback (&TraceSimulation::RecordDropQueue, this));

  Config::ConnectWithoutContext ("/NodeList/*/$ns3::Ipv4L3Protocol/Drop",
                                 MakeCallback (&TraceSimulation::RecordDropIp, this));
}
