#include "include/sim-base.h"
#include "include/ip-utils.h"
#include "ns3/virtual-net-device-module.h"

NS_LOG_COMPONENT_DEFINE ("Simulation");

SimulationBase::SimulationBase (SimulationParameters simParameters, ContainerGroups containerGroups,
                                MigrationParams migParams)
    : m_containerGroups (containerGroups),
      m_leafCount (m_containerGroups.size ()),
      m_spineCount (m_leafCount),
      m_coreCount (simParameters.NetworkTopology == SimulationParameters::CLOS
                       ? 0
                       : simParameters.NumOfCore),
      m_podWidth (simParameters.NetworkTopology == SimulationParameters::CLOS
                      ? m_leafCount
                      : simParameters.PodWidth),
      m_podCount (m_spineCount / m_podWidth),
      m_nodes (m_leafCount),
      m_swToSwDevice (m_spineCount, vector<NetDeviceContainer> (m_podWidth)),
      m_coreToSpineDevice (m_coreCount, vector<NetDeviceContainer> (m_podCount)),
      m_simParameters (simParameters)
{
  NS_LOG_DEBUG ("Pod width = " << m_podWidth);
  NS_ASSERT (m_simParameters.NumOfPorts >= 8);
  NS_ASSERT (m_podWidth <= m_simParameters.NumOfPorts / 2);
  NS_ASSERT (simParameters.NetworkTopology == SimulationParameters::FATTREE ||
             (simParameters.NetworkTopology == SimulationParameters::CLOS && m_leafCount <= 255));
  NS_ASSERT ((simParameters.NetworkTopology == SimulationParameters::FATTREE &&
              m_spineCount % m_podWidth == 0) ||
             simParameters.NetworkTopology == SimulationParameters::CLOS);
  NS_ASSERT (m_coreCount % m_podWidth == 0);
  NS_ASSERT (m_podCount <= m_simParameters.NumOfPorts);
  NS_ASSERT (m_podCount * 3 < 255);
  for (uint32_t i = 0; i < m_leafCount; ++i)
    {
      size_t count = m_containerGroups[i].size ();
      m_nodeToSwDevice.push_back (vector<NetDeviceContainer> (count));
      m_socketHelpers.push_back (vector<SocketHelper> (
          count,
          SocketHelper (m_virtualToPhysical, m_misdeliveryCount, m_lastMisdelivered,
                        simParameters.GatewayPerFlowLoadBalancing, simParameters, migParams)));
      m_onDemandCaches.push_back (vector<set<uint32_t>> (count));
      NS_ASSERT_MSG (count <= 255, "Leaf #" << i << " with " << count << " nodes");
    }

  // m_traceStream = m_asciiHelper.CreateFileStream ("trace.tr");
  AssignIds ();
}

void
SimulationBase::AssignIds ()
{
  uint32_t containerId = 0;
  for (size_t i = 0; i < m_containerGroups.size (); ++i)
    {
      for (size_t j = 0; j < m_containerGroups[i].size (); ++j)
        {
          for (size_t k = 0; k < m_containerGroups[i][j].size (); ++k)
            {
              if (m_containerGroups[i][j][k].find ("Gateway") != string::npos)
                {
                  NS_LOG_DEBUG ("Gateway ID = " << containerId);
                  m_gws.push_back (std::make_pair (i, j));
                }
              m_containerToId[m_containerGroups[i][j][k]] = containerId++;
            }
        }
    }
}

void
SimulationBase::Run ()
{
  Configure ();
  InitializeNodes ();
  InstallNetworkStack ();
  CreateTopology ();
  AssignPhysicalIps ();
  SetupTunnel ();
  SetupApplications ();
  StartSimulation ();
  StopSimulation ();
}

void
SimulationBase::Configure ()
{
  if (m_simParameters.RandomRouting)
    {
      Config::SetDefault ("ns3::Ipv4GlobalRouting::EcmpMode",
                          EnumValue (Ipv4GlobalRouting::ECMP_RANDOM));
    }
  else
    {
      Config::SetDefault ("ns3::Ipv4GlobalRouting::EcmpMode",
                          EnumValue (Ipv4GlobalRouting::ECMP_PER_FLOW));
    }
  // LogComponentEnable ("Simulation", LogLevel (LOG_PREFIX_ALL | LOG_ALL));
  // LogComponentEnable("SocketHelper", LOG_LEVEL_ALL);
  // LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
  // LogComponentEnable ("Ipv4StaticRouting", LOG_LEVEL_ALL);

  // LogComponentEnable ("SwitchApp", LOG_LEVEL_ALL);
  // LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable ("VirtualNetDevice", LOG_LEVEL_ALL);
  // LogComponentEnable ("TrafficControlLayer", LOG_LEVEL_ALL);
  // LogComponentEnable ("UdpL4Protocol", LOG_LEVEL_ALL);

  // LogComponentEnable ("UdpSocketImpl", LOG_LEVEL_INFO);

  // Packet::EnablePrinting ();
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1024));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize",
                      UintegerValue (50 * 1024 * 1024));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize",
                      UintegerValue (50 * 1024 * 1024));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout",
                      TimeValue (MilliSeconds (1)));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto",
                      TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity",
                      TimeValue (NanoSeconds (10)));
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation",
                      TimeValue (MicroSeconds (53 * 2)));
  Config::SetDefault ("ns3::TcpSocket::ConnCount", UintegerValue (1024));
  Config::SetDefault ("ns3::TcpSocketBase::ReTxThreshold",
                      UintegerValue (2048)); // 300 produces similar results
  Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", StringValue ("32MiB"));
  m_nodeToSw.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  m_nodeToSw.SetChannelAttribute ("Delay", StringValue ("1us"));
  m_swToSw.SetDeviceAttribute ("DataRate", StringValue ("400Gbps"));
  m_swToSw.SetChannelAttribute ("Delay", StringValue ("1us"));
}
void
SimulationBase::InitializeNodes ()
{
  if (m_simParameters.NetworkTopology == SimulationParameters::FATTREE)
    {
      m_cores.Create (m_coreCount);
    }
  m_spines.Create (m_spineCount);
  m_leaves.Create (m_leafCount);
  for (uint32_t i = 0; i < m_leafCount; ++i)
    m_nodes[i].Create (m_containerGroups[i].size ());
}

void
SimulationBase::InstallNetworkStack ()
{
  InternetStackHelper stack;
  stack.SetIpv6StackInstall (false);
  stack.Install (m_cores);
  stack.Install (m_spines);
  stack.Install (m_leaves);
  for (uint32_t i = 0; i < m_leafCount; i++)
    stack.Install (m_nodes[i]);
}

void
SimulationBase::CreateTopology ()
{
  for (uint32_t i = 0; i < m_leafCount; i++)
    for (size_t j = 0; j < m_containerGroups[i].size (); j++)
      m_nodeToSwDevice[i][j] = m_nodeToSw.Install (m_nodes[i].Get (j), m_leaves.Get (i));

  if (m_simParameters.NetworkTopology == SimulationParameters::FATTREE)
    {
      for (uint32_t i = 0; i < m_spineCount; i++)
        for (uint32_t j = 0; (i / m_podWidth) * m_podWidth + j < m_leafCount && j < m_podWidth; j++)
          m_swToSwDevice[i][j] =
              m_swToSw.Install (m_spines.Get (i), m_leaves.Get ((i / m_podWidth) * m_podWidth + j));

      for (uint32_t i = 0; i < m_spineCount; i++)
        for (uint32_t j = 0; j < m_coreCount / m_podWidth; j++)
          {
            uint32_t podIdx = i / m_podWidth;
            uint32_t coreIdx = (i % m_podWidth) * (m_coreCount / m_podWidth) + j;
            m_coreToSpineDevice[coreIdx][podIdx] =
                m_swToSw.Install (m_cores.Get (coreIdx), m_spines.Get (i));
          }
    }
  else
    {
      for (uint32_t i = 0; i < m_spineCount; i++)
        for (uint32_t j = 0; j < m_leafCount; j++)
          m_swToSwDevice[i][j] = m_swToSw.Install (m_spines.Get (i), m_leaves.Get (j));
    }
}

void
SimulationBase::AssignPhysicalIps ()
{
  for (uint32_t i = 0; i < m_leafCount; i++)
    {
      for (size_t j = 0; j < m_containerGroups[i].size (); j++)
        {
          m_address.SetBase (
              IpUtils::GetNodeBasePhysicalAddress (i / m_podWidth, i % m_podWidth, j),
              IpUtils::GetClassCMask ());
          m_address.Assign (m_nodeToSwDevice[i][j]);
        }
    }
  for (uint32_t i = 0; i < m_spineCount; i++)
    {
      for (uint32_t j = 0; (i / m_podWidth) * m_podWidth + j < m_leafCount && j < m_podWidth; j++)
        {
          m_address.SetBase (
              IpUtils::GetSwitchBaseAddress (m_podCount, i / m_podWidth, i % m_podWidth, j),
              IpUtils::GetClassCMask ());
          m_address.Assign (m_swToSwDevice[i][j]);
        }
      for (uint32_t j = 0; j < m_coreCount / m_podWidth; j++)
        {
          uint32_t podIdx = i / m_podWidth;
          uint32_t coreIdx = (i % m_podWidth) * (m_coreCount / m_podWidth) + j;

          m_address.SetBase (IpUtils::GetCoreSwitchBaseAddress (podIdx, coreIdx),
                             IpUtils::GetClassCMask ());
          m_address.Assign (m_coreToSpineDevice[coreIdx][podIdx]);
        }
    }
  // This is super slow. Instead, we set the routing manually.
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Node to leaf routing
  for (uint32_t i = 0; i < m_leafCount; ++i)
    {
      for (size_t j = 0; j < m_containerGroups[i].size (); ++j)
        {

          Ptr<Node> n = m_nodes[i].Get (j);
          Ptr<GlobalRouter> gRouter = n->GetObject<GlobalRouter> ();
          Ptr<Ipv4GlobalRouting> routing = gRouter->GetRoutingProtocol ();
          routing->AddNetworkRouteTo (Ipv4Address::GetAny (), Ipv4Mask::GetZero (),
                                      Ipv4Address::GetAny (), 1);
        }
    }

  // Leaf to spine, leaf to leaf, leaf to core routing
  for (uint32_t i = 0; i < m_leafCount; ++i)
    {
      Ptr<Node> n = m_leaves.Get (i);
      Ptr<GlobalRouter> gRouter = n->GetObject<GlobalRouter> ();
      Ptr<Ipv4GlobalRouting> routing = gRouter->GetRoutingProtocol ();
      uint32_t currentLeafPodId = i / m_podWidth;
      uint32_t currentLeafInternalId = i % m_podWidth;
      for (uint32_t j = 0; j < m_leafCount; ++j)
        {
          uint32_t otherLeafPodId = j / m_podWidth;
          uint32_t otherLeafInternalId = j % m_podWidth;

          if (otherLeafInternalId == currentLeafInternalId && otherLeafPodId == currentLeafPodId)
            {
              continue;
            }

          for (uint32_t k = 0; k < m_podWidth; ++k)
            {
              // Leaf to all nodes in the topology, can get there through any spine
              routing->AddNetworkRouteTo (
                  IpUtils::GetNodePhysicalAddress (otherLeafPodId, otherLeafInternalId, 0),
                  IpUtils::GetClassBMask (), Ipv4Address::GetAny (),
                  m_containerGroups[i].size () + 1 + k);

              // Leaf to all other leaves, can get there through any spine
              routing->AddNetworkRouteTo (
                  IpUtils::GetLeafAddress (m_podCount, otherLeafPodId, 0, otherLeafInternalId),
                  IpUtils::GetClassCMask (), Ipv4Address::GetAny (),
                  m_containerGroups[i].size () + 1 + k);
            }
        }

      for (uint32_t j = 0; j < m_spineCount; ++j)
        {
          uint32_t spinePodId = j / m_podWidth;
          uint32_t spineOffset = j % m_podWidth;

          if (spinePodId == currentLeafPodId)
            {
              // Leaf to spines in the same pod, one path only
              routing->AddNetworkRouteTo (
                  IpUtils::GetSpineFromLeafAddress (m_podCount, spinePodId, spineOffset, 0),
                  IpUtils::GetClassCMask (), Ipv4Address::GetAny (),
                  m_containerGroups[i].size () + 1 + spineOffset);
            }
          else
            {
              for (uint32_t k = 0; k < m_podWidth; ++k)
                {
                  // Leaf to spines in other pods, can get through any spine
                  routing->AddNetworkRouteTo (
                      IpUtils::GetSpineFromLeafAddress (m_podCount, spinePodId, spineOffset, 0),
                      IpUtils::GetClassCMask (), Ipv4Address::GetAny (),
                      m_containerGroups[i].size () + 1 + k);
                }
            }
        }

      for (uint32_t j = 0; j < m_coreCount; ++j)
        {
          // Leaf to core, only through a specific core switch
          uint32_t spineOffset = j / (m_coreCount / m_podWidth);
          routing->AddNetworkRouteTo (IpUtils::GetCoreAddress (0, j), IpUtils::GetClassCMask (),
                                      Ipv4Address::GetAny (),
                                      m_containerGroups[i].size () + 1 + spineOffset);
        }
    }

  // Spine to leaf, core and spines routing
  for (uint32_t k = 0; k < m_spineCount; ++k)
    {
      Ptr<Node> n = m_spines.Get (k);
      Ptr<GlobalRouter> gRouter = n->GetObject<GlobalRouter> ();
      Ptr<Ipv4GlobalRouting> routing = gRouter->GetRoutingProtocol ();
      uint32_t spinePodId = k / m_podWidth;
      uint32_t spineInternalId = k % m_podWidth;

      for (uint32_t i = 0; i < m_leafCount; ++i)
        {
          uint32_t leafPodId = i / m_podWidth;
          uint32_t leafInternalId = i % m_podWidth;

          if (spinePodId == leafPodId)
            {
              // Spine to all nodes connected to that leaf
              routing->AddNetworkRouteTo (
                  IpUtils::GetNodePhysicalAddress (leafPodId, leafInternalId, 0),
                  IpUtils::GetClassBMask (), Ipv4Address::GetAny (), leafInternalId + 1);
              // Spine to a leaf in the same pod
              routing->AddNetworkRouteTo (
                  IpUtils::GetLeafAddress (m_podCount, leafPodId, 0, leafInternalId),
                  IpUtils::GetClassCMask (), Ipv4Address::GetAny (), leafInternalId + 1);
            }
          else
            {
              for (uint32_t j = 0; j < m_coreCount / m_podWidth; j++)
                {
                  // Spine to nodes and leaves in other pods, can get there through any core switch
                  routing->AddNetworkRouteTo (
                      IpUtils::GetNodePhysicalAddress (leafPodId, leafInternalId, 0),
                      IpUtils::GetClassBMask (), Ipv4Address::GetAny (), m_podWidth + 1 + j);
                  routing->AddNetworkRouteTo (
                      IpUtils::GetLeafAddress (m_podCount, leafPodId, 0, leafInternalId),
                      IpUtils::GetClassCMask (), Ipv4Address::GetAny (), m_podWidth + 1 + j);
                }
            }
        }

      // Spine to other spines
      for (uint32_t i = 0; i < m_spineCount; ++i)
        {
          uint32_t otherSpinePodId = i / m_podWidth;
          uint32_t otherSpineInternalId = i % m_podWidth;

          if (otherSpinePodId == spinePodId && otherSpineInternalId == spineInternalId)
            {
              continue;
            }

          if (spinePodId == otherSpinePodId)
            {
              // Spine to a spine in the same pod
              for (uint32_t j = 0; j < m_podWidth; ++j)
                {
                  routing->AddNetworkRouteTo (
                      IpUtils::GetSpineFromLeafAddress (m_podCount, otherSpinePodId,
                                                        otherSpineInternalId, 0),
                      IpUtils::GetClassCMask (), Ipv4Address::GetAny (), 1 + j);
                }
            }
          else
            {
              for (uint32_t j = 0; j < m_coreCount / m_podWidth; j++)
                {
                  // Spine to spines in other pods, can get there through any core switch
                  routing->AddNetworkRouteTo (
                      IpUtils::GetSpineFromLeafAddress (m_podCount, otherSpinePodId,
                                                        otherSpineInternalId, 0),
                      IpUtils::GetClassCMask (), Ipv4Address::GetAny (), m_podWidth + 1 + j);
                }
            }
        }

      for (uint32_t i = 0; i < m_coreCount / m_podWidth; ++i)
        {
          // Spine to connected cores
          uint32_t coreId = (k % m_podWidth) * (m_coreCount / m_podWidth) + i;
          routing->AddNetworkRouteTo (IpUtils::GetCoreAddress (0, coreId),
                                      IpUtils::GetClassCMask (), Ipv4Address::GetAny (),
                                      m_podWidth + 1 + i);
        }
    }

  // Core to spine routing
  for (uint32_t k = 0; k < m_coreCount; ++k)
    {
      Ptr<Node> n = m_cores.Get (k);
      Ptr<GlobalRouter> gRouter = n->GetObject<GlobalRouter> ();
      Ptr<Ipv4GlobalRouting> routing = gRouter->GetRoutingProtocol ();
      for (uint32_t i = 0; i < m_podCount; ++i)
        {
          for (uint32_t j = 0; j < m_podWidth; ++j)
            {
              routing->AddNetworkRouteTo (IpUtils::GetNodePhysicalAddress (i, j, 0),
                                          IpUtils::GetClassBMask (), Ipv4Address::GetAny (), i + 1);

              routing->AddNetworkRouteTo (IpUtils::GetSpineFromLeafAddress (m_podCount, i, j, 0),
                                          IpUtils::GetClassCMask (), Ipv4Address::GetAny (), i + 1);
            }
        }

      for (uint32_t leaf = 0; leaf < m_leafCount; ++leaf)
        {
          uint32_t leafInternalId = leaf % m_podWidth;
          uint32_t leafPodId = leaf / m_podWidth;
          // Core to leaf, only one path
          routing->AddNetworkRouteTo (
              IpUtils::GetLeafAddress (m_podCount, leafPodId, 0, leafInternalId),
              IpUtils::GetClassCMask (), Ipv4Address::GetAny (), 1 + leafPodId);
        }
    }
}

void
SimulationBase::SetupTunnel ()
{
  for (size_t i = 0; i < m_gws.size (); ++i)
    {
      m_gwAddresses.push_back (IpUtils::GetNodePhysicalAddress (
          m_gws[i].first / m_podWidth, m_gws[i].first % m_podWidth, m_gws[i].second));
    }
  for (uint32_t i = 0; i < m_leafCount; ++i)
    for (size_t j = 0; j < m_containerGroups[i].size (); ++j)
      {

        Ptr<Socket> socket = Socket::CreateSocket (m_nodes[i].Get (j),
                                                   TypeId::LookupByName ("ns3::UdpSocketFactory"));
        socket->Bind (
            InetSocketAddress (IpUtils::GetNodePhysicalAddress (i / m_podWidth, i % m_podWidth, j),
                               SocketHelper::PORT_NUMBER));
        socket->SetRecvCallback (MakeCallback (&SocketHelper::SocketRecv, &m_socketHelpers[i][j]));
        Ptr<VirtualNetDevice> vDev = CreateObject<VirtualNetDevice> ();
        Ptr<Node> node = m_nodes[i].Get (j);
        vDev->SetSendCallback (MakeCallback (&SocketHelper::VirtualSend, &m_socketHelpers[i][j]));
        node->AddDevice (vDev);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
        uint32_t iface = ipv4->AddInterface (vDev);

        for (size_t k = 0; k < m_containerGroups[i][j].size (); ++k)
          {
            uint32_t containerId = m_containerToId[m_containerGroups[i][j][k]];

            ipv4->AddAddress (
                iface, Ipv4InterfaceAddress (IpUtils::GetContainerVirtualAddress (containerId),
                                             IpUtils::GetClassAMask ()));

            m_virtualToPhysical[IpUtils::GetContainerVirtualAddress (containerId).Get ()] =
                IpUtils::GetNodePhysicalAddress (i / m_podWidth, i % m_podWidth, j).Get ();
          }
        ipv4->SetUp (iface);

        m_socketHelpers[i][j].m_node = node;
        m_socketHelpers[i][j].m_socket = socket;
        m_socketHelpers[i][j].m_vDev = vDev;
        m_socketHelpers[i][j].m_gatewayRange = m_containerToId.size () / m_gws.size ();
        m_socketHelpers[i][j].m_gatewayAddresses = m_gwAddresses;
        m_socketHelpers[i][j].m_physicalAddress =
            IpUtils::GetNodePhysicalAddress (i / m_podWidth, i % m_podWidth, j);
        m_socketHelpers[i][j].m_onDemandCache = &m_onDemandCaches[i][j];
      }
}

void
SimulationBase::StartSimulation ()
{
  // m_swToSw.EnableAsciiAll (m_traceStream);
  Simulator::Run ();
}

void
SimulationBase::StopSimulation ()
{
  Simulator::Destroy ();
  NS_LOG_INFO ("Finished simulation");
}
