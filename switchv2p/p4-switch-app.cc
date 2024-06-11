#include "include/p4-switch-app.h"

#include "include/eviction-tag.h"
#include "include/hit-tag.h"
#include "include/invalidation-tag.h"
#include "include/ip-utils.h"

#include "ns3/internet-module.h"

#include <random>

NS_LOG_COMPONENT_DEFINE ("P4SwitchApp");
NS_OBJECT_ENSURE_REGISTERED (P4SwitchApp);

const uint16_t P4SwitchApp::SWITCH_PORT = 333;

/**
 * Register this type.
 * \return The TypeId.
 */
TypeId
P4SwitchApp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("P4SwitchApp")
          .SetParent<Application> ()
          .SetGroupName ("Sim")
          .AddConstructor<P4SwitchApp> ()
          .AddAttribute ("MemorySize", "The number of entries each switch can store",
                         IntegerValue (10), MakeIntegerAccessor (&P4SwitchApp::m_memorySize),
                         MakeIntegerChecker<int32_t> ())
          .AddAttribute ("TTL", "The default TTL value", IntegerValue (64),
                         MakeIntegerAccessor (&P4SwitchApp::m_defaultTtl),
                         MakeIntegerChecker<uint32_t> ())
          .AddAttribute ("RandomHashFunction", "Random hash function", BooleanValue (false),
                         MakeBooleanAccessor (&P4SwitchApp::m_randomHash), MakeBooleanChecker ())
          .AddAttribute ("SourceLearning", "Enable source learning at leaf switches",
                         BooleanValue (false), MakeBooleanAccessor (&P4SwitchApp::m_sourceLearning),
                         MakeBooleanChecker ())
          .AddAttribute ("AccessBit", "Enable the use of access bits in spine switches",
                         BooleanValue (true), MakeBooleanAccessor (&P4SwitchApp::m_accessBit),
                         MakeBooleanChecker ())
          .AddAttribute ("BloomFilter", "Enable the use of bloom filter for invalidations",
                         BooleanValue (false),
                         MakeBooleanAccessor (&P4SwitchApp::m_bloomFilterEnabled),
                         MakeBooleanChecker ())
          .AddAttribute ("BloomFilterSize", "The number of entries in the bloom filter",
                         IntegerValue (80), MakeIntegerAccessor (&P4SwitchApp::m_bloomFilterSize),
                         MakeIntegerChecker<int32_t> ())
          .AddAttribute ("GenerateInvalidations", "Enable the generation of invalidation packets",
                         BooleanValue (false),
                         MakeBooleanAccessor (&P4SwitchApp::m_generateInvalidation),
                         MakeBooleanChecker ())
          .AddAttribute ("GenerateProbability", "Probablity of packet generation",
                         DoubleValue (0.1), MakeDoubleAccessor (&P4SwitchApp::m_generateProb),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("PcieDataRate", "The default data rate for the pcie link",
                         DataRateValue (DataRate ("20Gbps")),
                         MakeDataRateAccessor (&P4SwitchApp::m_bps), MakeDataRateChecker ())
          .AddAttribute ("ControlPlaneLatency", "The latency of the control plane",
                         TimeValue (NanoSeconds (8500)),
                         MakeTimeAccessor (&P4SwitchApp::m_bluebirdDelay), MakeTimeChecker ())
          .AddAttribute (
              "BluebirdProgrammingLatency", "The latency of the programming the data plane cache",
              TimeValue (MilliSeconds (2)),
              MakeTimeAccessor (&P4SwitchApp::m_bluebirdProgrammingDelay), MakeTimeChecker ())
          .AddAttribute (
              "BluebirdQueueSize", "The max queue size", QueueSizeValue (QueueSize ("1MiB")),
              MakeQueueSizeAccessor (&P4SwitchApp::m_bluebirdQueueSize), MakeQueueSizeChecker ())
          .AddTraceSource ("GeneratedLearning", "A packet has been generated for learning",
                           MakeTraceSourceAccessor (&P4SwitchApp::m_learningTrace),
                           "ns3::Packet::AddressTracedCallback")
          .AddTraceSource ("GeneratedInvalidation", "A packet has been generated for invalidation",
                           MakeTraceSourceAccessor (&P4SwitchApp::m_invalidationTrace),
                           "ns3::Packet::AddressTracedCallback")
          .AddTraceSource ("ProcessedPackets", "A packet has been processed by the switch",
                           MakeTraceSourceAccessor (&P4SwitchApp::m_processedPackets),
                           "ns3::Packet::SwitchIdTracedCallback")
          .AddTraceSource ("CacheHit", "A packet hit the cache on the switch",
                           MakeTraceSourceAccessor (&P4SwitchApp::m_cacheHit),
                           "ns3::Packet::SwitchIdTracedCallback");

  return tid;
}

P4SwitchApp::P4SwitchApp () : m_bluebirdBusy (false)
{
}

void
P4SwitchApp::Setup (vector<Ipv4Address> &gwAddresses, Ipv4Address switchAddress,
                    enum SwitchType switchType, enum SimulationParameters::Mode simMode,
                    uint32_t podCount, unordered_map<uint32_t, uint32_t> *virtualToPhysical)
{
  std::transform (gwAddresses.begin (), gwAddresses.end (),
                  inserter (m_gwAddresses, m_gwAddresses.end ()),
                  [] (const Ipv4Address &ipv4) { return ipv4.Get (); });
  m_switchAddress = switchAddress;
  m_switchType = switchType;
  m_simMode = simMode;
  m_cache.Setup (m_memorySize, m_randomHash);
  m_bluebirdCache.SetCapacity (m_memorySize);
  ObjectFactory queueFactory;
  queueFactory.SetTypeId ("ns3::DropTailQueue<Packet>");
  queueFactory.Set ("MaxSize", QueueSizeValue (m_bluebirdQueueSize));
  m_bluebirdQueue = queueFactory.Create<Queue<Packet>> ();
  m_random = CreateObject<UniformRandomVariable> ();
  m_podCount = podCount;
  m_virtualToPhysical = virtualToPhysical;

  if (m_bloomFilterEnabled)
    {
      m_bloomFilter.Setup (m_bloomFilterSize);
    }
}

void
P4SwitchApp::StartApplication (void)
{
  Ptr<Ipv4L3Protocol> ipv4Proto = m_node->GetObject<Ipv4L3Protocol> ();
  if (ipv4Proto != 0)
    {
      NS_LOG_INFO ("Ipv4 packet interceptor added");
      ipv4Proto->AddPacketInterceptor (MakeCallback (&P4SwitchApp::ReceivePacket, this),
                                       UdpL4Protocol::PROT_NUMBER);
    }
  else
    {
      NS_LOG_INFO ("No Ipv4 with packet intercept facility");
    }

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      m_socket->Bind (InetSocketAddress (m_switchAddress, SWITCH_PORT));
    }

  if (m_simMode == SimulationParameters::Mode::Bluebird)
    {
      for (uint32_t i = 1; i < m_node->GetNDevices (); ++i)
        {
          TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
          Ptr<Socket> s = Socket::CreateSocket (GetNode (), tid);
          s->SetAttribute ("Protocol", UintegerValue (UdpL4Protocol::PROT_NUMBER));
          s->SetAttribute ("IpHeaderInclude", BooleanValue (true));
          s->SetRecvCallback (MakeCallback (&P4SwitchApp::ReceiveRawPacket, this));
          s->BindToNetDevice (GetNode ()->GetDevice (i));
          m_bluebirdSockets.push_back (s);
        }
    }

  NS_LOG_DEBUG ("Starting " << m_switchType << " switch " << m_switchAddress);
}

void
P4SwitchApp::ReceiveRawPacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      Ipv4Header ipHeader;
      packet->RemoveHeader (ipHeader);
      m_packetToSocket[packet->GetUid ()] = socket;
      bool forward = ReceivePacket (packet, ipHeader);
      if (forward)
        {
          m_node->GetObject<Ipv4L3Protocol> ()->ReceiveInternal (packet, ipHeader,
                                                                 socket->GetBoundNetDevice ());
          m_packetToSocket.erase (packet->GetUid ());
        }
    }
}
void
P4SwitchApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_socket = 0;
    }

  for (Ptr<Socket> s : m_bluebirdSockets)
    {
      if (s)
        {
          s->Close ();
          s->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
          s = 0;
        }
    }
}

bool
P4SwitchApp::HandleProtocolPacket (Ptr<Packet> packet, Ipv4Header &ipHeader)
{
  InvalidationTag tag;
  if (packet->PeekPacketTag (tag))
    {
      // Invalidation
      pair<uint32_t, uint32_t> invalidated = tag.GetInvalidation ();
      uint32_t cachedVal = 0;
      if (m_cache.Get (invalidated.first, cachedVal))
        {
          if (cachedVal == invalidated.second)
            {
              m_cache.Remove (invalidated.first);
            }
        }
    }

  if (ipHeader.GetDestination () == m_switchAddress)
    {
      EvictionTag tag;
      if (packet->PeekPacketTag (tag))
        {
          pair<uint32_t, uint32_t> learn = tag.GetEvicted ();
          m_cache.Put (learn.first, learn.second);
        }
    }
  else
    {
      m_socket->SendTo (packet, 0, InetSocketAddress (ipHeader.GetDestination (), SWITCH_PORT));
    }

  return false;
}

bool
P4SwitchApp::LocalP4CacheLogic (uint32_t virtualDestinationIp, uint32_t physicalDestinationIp,
                                Ptr<Packet> packet, Ipv4Header &ipHeader)
{
  if (m_gwAddresses.count (physicalDestinationIp))
    {
      uint32_t cachedAddr = 0;
      if (m_cache.Get (virtualDestinationIp, cachedAddr))
        {
          ipHeader.SetDestination (Ipv4Address (cachedAddr));
        }
    }
  else
    {
      m_cache.Put (virtualDestinationIp, physicalDestinationIp);
    }

  return true;
}

void
P4SwitchApp::BluebirdProcessPacket (Ptr<Packet> packet)
{
  Ipv4Header outterHeader; // Physical IPs
  packet->RemoveHeader (outterHeader);
  UdpHeader udpHeader;
  packet->RemoveHeader (udpHeader);
  Ipv4Header innerHeader; // Virtual IPs
  packet->RemoveHeader (innerHeader);
  uint32_t virtualDestinationIp = innerHeader.GetDestination ().Get ();
  uint32_t physicalDestinationIp = m_virtualToPhysical->at (virtualDestinationIp);
  outterHeader.SetDestination (Ipv4Address (physicalDestinationIp));
  packet->AddHeader (innerHeader);
  packet->AddHeader (udpHeader);
  m_node->GetObject<Ipv4L3Protocol> ()->ReceiveInternal (packet, outterHeader,
                                                         m_node->GetDevice (0));
  m_packetToSocket.erase (packet->GetUid ());
  Simulator::Schedule (m_bluebirdProgrammingDelay, &P4SwitchApp::PopulateBluebirdCache, this,
                       virtualDestinationIp);
}

void
P4SwitchApp::PopulateBluebirdCache (uint32_t virtualIp)
{
  m_bluebirdCache.Put (virtualIp, m_virtualToPhysical->at (virtualIp));
}

void
P4SwitchApp::BluebirdTxStart (Ptr<Packet> packet)
{
  m_bluebirdBusy = true;
  Time txTime = m_bps.CalculateBytesTxTime (packet->GetSize ());
  Simulator::Schedule (txTime + m_bluebirdDelay, &P4SwitchApp::BluebirdProcessPacket, this, packet);
  Simulator::Schedule (txTime + NanoSeconds (250), &P4SwitchApp::BluebirdTxComplete, this);
}

void
P4SwitchApp::BluebirdTxComplete ()
{
  m_bluebirdBusy = false;
  Ptr<Packet> p = m_bluebirdQueue->Dequeue ();
  if (p == 0)
    {
      return;
    }

  BluebirdTxStart (p);
}

bool
P4SwitchApp::BluebirdLogic (uint32_t virtualDestinationIp, uint32_t physicalDestinationIp,
                            Ptr<Packet> packet, Ipv4Header &ipHeader)
{
  if (m_gwAddresses.count (physicalDestinationIp))
    {
      uint32_t cachedAddr = 0;
      if (m_bluebirdCache.Get (virtualDestinationIp, cachedAddr))
        {
          ipHeader.SetDestination (Ipv4Address (cachedAddr));
          return true;
        }
      else
        {
          packet->AddHeader (ipHeader);
          if (m_bluebirdQueue->Enqueue (packet))
            {
              if (!m_bluebirdBusy)
                {
                  BluebirdTxStart (m_bluebirdQueue->Dequeue ());
                }
            }
          else
            {
              NS_LOG_DEBUG ("Bluebird packet drop");
              m_packetToSocket.erase (packet->GetUid ());
            }

          return false;
        }
    }
  return true;
}

void
P4SwitchApp::GeneratePacketToLeaf (pair<uint32_t, uint32_t> data, Ipv4Address hostAddress)
{
  uint32_t physicalSource = hostAddress.Get ();
  uint32_t podId = (physicalSource >> 24) - 1;
  uint32_t leafOffset = (physicalSource >> 16) & 0x000000ff;
  Ipv4Address dstAddress = IpUtils::GetLeafAddress (m_podCount, podId, 0, leafOffset);
  GeneratePacketToAddress (data, dstAddress, true);
}

void
P4SwitchApp::GeneratePacketToAddress (pair<uint32_t, uint32_t> data, Ipv4Address dstAddress,
                                      bool learning)
{
  Ptr<Packet> generatedPacket = Create<Packet> (64);
  if (learning)
    {
      EvictionTag tag;
      tag.SetEvicted (data);
      generatedPacket->AddPacketTag (tag);
    }
  else
    {
      InvalidationTag tag;
      tag.SetInvalidation (data);
      generatedPacket->AddPacketTag (tag);
    }

  if (dstAddress != m_switchAddress)
    {
      m_socket->SendTo (generatedPacket, 0, InetSocketAddress (dstAddress, SWITCH_PORT));
    }
}

bool
P4SwitchApp::ReceivePacket (Ptr<Packet> packet, Ipv4Header &ipHeader)
{
  UdpHeader udpHeader;
  packet->RemoveHeader (udpHeader);

  if (udpHeader.GetDestinationPort () == SWITCH_PORT)
    {
      return HandleProtocolPacket (packet, ipHeader);
    }

  HopsTag hopsTag;
  if (packet->RemovePacketTag (hopsTag))
    {
      hopsTag.IncHops ();
      packet->AddPacketTag (hopsTag);
    }

  NS_ASSERT (udpHeader.GetDestinationPort () == 667);
  m_processedPackets (packet, GetNode ()->GetId ());
  Ipv4Header innerHeader;
  packet->RemoveHeader (innerHeader);

  // packet is not a copy!
  packet->AddHeader (innerHeader);
  packet->AddHeader (udpHeader);

  uint32_t virtualDestinationIp = innerHeader.GetDestination ().Get ();
  uint32_t physicalDestinationIp = ipHeader.GetDestination ().Get ();

  if (m_simMode == SimulationParameters::Mode::LocalLearning)
    {
      return LocalP4CacheLogic (virtualDestinationIp, physicalDestinationIp, packet, ipHeader);
    }
  else if (m_simMode == SimulationParameters::Mode::Bluebird)
    {
      return BluebirdLogic (virtualDestinationIp, physicalDestinationIp, packet, ipHeader);
    }

  InvalidationTag invalidationTag;
  if (packet->PeekPacketTag (invalidationTag))
    {
      pair<uint32_t, uint32_t> invalidated = invalidationTag.GetInvalidation ();
      uint32_t cachedVal = 0;
      if (m_cache.Get (invalidated.first, cachedVal))
        {
          if (cachedVal == invalidated.second)
            {
              m_cache.Remove (invalidated.first);
            }
          else
            {
              ipHeader.SetDestination (Ipv4Address (cachedVal));
            }
        }

      if (m_switchType == LEAF)
        {
          HitTag hitTag;
          if (packet->RemovePacketTag (hitTag))
            {
              bool generate = m_generateInvalidation;
              if (m_bloomFilterEnabled)
                {
                  if (m_bloomFilter.Get (hitTag.GetId ()) == 1)
                    {
                      generate = false;
                    }
                }

              if (generate)
                {
                  m_invalidationTrace (packet);
                  GeneratePacketToAddress (
                      std::make_pair (virtualDestinationIp, invalidated.second),
                      hitTag.GetAddress (), false);
                }

              if (m_bloomFilterEnabled)
                {
                  m_bloomFilter.Put (hitTag.GetId ());
                }
            }
        }

      return true;
    }

  EvictionTag tag;

  // Promote entries from SPINE to CORE
  if (m_switchType == SPINE && m_gwAddresses.count (physicalDestinationIp))
    {
      uint32_t cachedVal = 0;
      if (m_cache.Get (virtualDestinationIp, cachedVal))
        {
          packet->RemovePacketTag (tag);
          tag.SetEvicted (std::make_pair (virtualDestinationIp, cachedVal));
          packet->AddPacketTag (tag);
          return true;
        }
    }

  if (m_switchType == LEAF)
    {
      if (m_sourceLearning && ipHeader.GetTtl () < m_defaultTtl - 1)
        {
          m_cache.Put (innerHeader.GetSource ().Get (), ipHeader.GetSource ().Get ());
        }
      else if (m_gwAddresses.count (physicalDestinationIp) == 0)
        {
          m_cache.PutIfNotEvict (virtualDestinationIp, physicalDestinationIp);
        }
    }
  else
    {
      pair<uint32_t, uint32_t> learn = std::make_pair (virtualDestinationIp, physicalDestinationIp);
      bool foundTag = false;
      if (packet->RemovePacketTag (tag))
        {
          foundTag = true;
          learn = tag.GetEvicted ();
          if (learn.first == 0 && learn.second == 0)
            {
              // Remove obsolete entry -> migration
              if (m_cache.Find (virtualDestinationIp))
                {
                  m_cache.Remove (virtualDestinationIp);
                }
              packet->AddPacketTag (tag);

              return true;
            }
        }

      if (foundTag || m_gwAddresses.count (physicalDestinationIp) == 0)
        {
          pair<uint32_t, uint32_t> evicted;
          if (m_switchType == CORE)
            {
              if (foundTag)
                {
                  uint8_t bit = m_cache.GetBit (learn.first);
                  bool inCache = m_cache.Find (learn.first);

                  if (learn.first == virtualDestinationIp && bit == 0 && !inCache)
                    {
                      if (m_cache.Put (learn.first, learn.second, evicted))
                        {
                          tag.SetEvicted (evicted);
                          packet->AddPacketTag (tag);
                        }
                    }
                  else
                    {
                      packet->AddPacketTag (tag);
                    }
                }
              else
                {
                  m_cache.PutIfNotEvict (learn.first, learn.second);
                }
            }
          else
            {
              if ((m_switchType == SPINE || m_switchType == GW_SPINE) && m_accessBit)
                {
                  uint8_t bit = m_cache.GetBit (learn.first);
                  bool inCache = m_cache.Find (learn.first);

                  if (bit == 1 && !inCache)
                    {
                      if (foundTag)
                        {
                          packet->AddPacketTag (tag);
                        }
                    }
                  else if (bit == 0 || inCache)
                    {
                      bool cacheEvict = m_cache.Put (learn.first, learn.second, evicted);
                      if (cacheEvict)
                        {
                          tag.SetEvicted (evicted);
                          packet->AddPacketTag (tag);
                        }
                    }
                }
              else
                {
                  if (m_switchType == GW_LEAF && m_random->GetValue (0.0, 1.0) <= m_generateProb)
                    {
                      m_learningTrace (packet);
                      SourceTag sourceTag;
                      packet->PeekPacketTag (sourceTag);
                      NS_LOG_INFO ("Generating a packet to: " << sourceTag.GetSource ());
                      GeneratePacketToLeaf (std::make_pair (learn.first, learn.second),
                                            sourceTag.GetSource ());
                    }

                  bool cacheEvict = m_cache.Put (learn.first, learn.second, evicted);
                  if (cacheEvict)
                    {
                      tag.SetEvicted (evicted);
                      packet->AddPacketTag (tag);
                    }
                }
            }
        }
    }
  if (m_gwAddresses.count (physicalDestinationIp))
    {
      uint32_t cached_addr = 0;
      if (m_cache.Get (virtualDestinationIp, cached_addr))
        {
          HitTag tag;
          tag.SetAddress (m_switchAddress);
          tag.SetId (GetNode ()->GetId ());
          packet->AddPacketTag (tag);
          ipHeader.SetDestination (Ipv4Address (cached_addr));
          m_cacheHit (packet, GetNode ()->GetId ());
          return true;
        }
    }

  return true;
}
