#ifndef GATEWAY_APP_HELPER_H
#define GATEWAY_APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "gateway-app.h"

class GatewayAppHelper
{
public:
  GatewayAppHelper (unordered_map<uint32_t, uint32_t> &virtualToPhysical)
      : m_virtualToPhysical (virtualToPhysical)
  {
    m_factory.SetTypeId (GatewayApp::GetTypeId ());
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
    Ptr<GatewayApp> app = m_factory.Create<GatewayApp> ();
    app->Setup (&m_virtualToPhysical);
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
  unordered_map<uint32_t, uint32_t> &m_virtualToPhysical;
};

#endif /* GATEWAY_APP_HELPER_H */
