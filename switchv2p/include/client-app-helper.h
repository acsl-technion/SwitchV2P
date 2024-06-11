#ifndef CLIENT_APP_HELPER_H
#define CLIENT_APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "client-app.h"

class ClientAppHelper
{
public:
  ClientAppHelper (vector<Flow> &flows, Ipv4Address address, Ipv4Address physicalAddress,
                   bool udpMode)
      : m_flows (flows),
        m_address (address),
        m_physicalAddress (physicalAddress),
        m_udpMode (udpMode)
  {
    m_factory.SetTypeId (ClientApp::GetTypeId ());
  }

  ApplicationContainer
  Install (Ptr<Node> node) const
  {
    return ApplicationContainer (InstallPriv (node));
  }

  ApplicationContainer
  Install (NodeContainer c) const
  {
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
      {
        apps.Add (InstallPriv (*i));
      }

    return apps;
  }

private:
  Ptr<Application>
  InstallPriv (Ptr<Node> node) const
  {
    Ptr<ClientApp> app = m_factory.Create<ClientApp> ();
    app->Setup (&m_flows, m_address, m_physicalAddress, m_udpMode);
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
  vector<Flow> &m_flows;
  Ipv4Address m_address, m_physicalAddress;
  bool m_udpMode;
};

#endif /* CLIENT_APP_HELPER_H */
