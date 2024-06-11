#ifndef SWITCH_APP_HELPER_H
#define SWITCH_APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "switch-app.h"
#include "sim-parameters.h"

class SwitchAppHelper
{
public:
  SwitchAppHelper (vector<Ipv4Address> &gwAddresses, enum SimulationParameters::Mode simMode)
      : m_gwAddresses (gwAddresses), m_simMode (simMode)
  {
    m_factory.SetTypeId (SwitchApp::GetTypeId ());
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
    Ptr<SwitchApp> app = m_factory.Create<SwitchApp> ();
    app->Setup (m_gwAddresses, m_simMode);
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
  vector<Ipv4Address> &m_gwAddresses;
  enum SimulationParameters::Mode m_simMode;
};

#endif /* SWITCH_APP_HELPER_H */
