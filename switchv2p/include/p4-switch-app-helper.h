#ifndef P4_SWITCH_APP_HELPER_H
#define P4_SWITCH_APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "p4-switch-app.h"

class P4SwitchAppHelper
{
public:
  P4SwitchAppHelper (vector<Ipv4Address> &gwAddresses, Ipv4Address switchAddress,
                     enum P4SwitchApp::SwitchType switchType,
                     enum SimulationParameters::Mode simMode, uint32_t podCount,
                     unordered_map<uint32_t, uint32_t> &virtualToPhysical)
      : m_gwAddresses (gwAddresses),
        m_switchAddress (switchAddress),
        m_switchType (switchType),
        m_simMode (simMode),
        m_podCount (podCount),
        m_virtualToPhysical (virtualToPhysical)
  {
    m_factory.SetTypeId (P4SwitchApp::GetTypeId ());
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
    Ptr<P4SwitchApp> app = m_factory.Create<P4SwitchApp> ();
    app->Setup (m_gwAddresses, m_switchAddress, m_switchType, m_simMode, m_podCount,
                &m_virtualToPhysical);
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
  vector<Ipv4Address> &m_gwAddresses;
  Ipv4Address m_switchAddress;
  enum P4SwitchApp::SwitchType m_switchType;
  enum SimulationParameters::Mode m_simMode;
  uint32_t m_podCount;
  unordered_map<uint32_t, uint32_t> &m_virtualToPhysical;
};

#endif /* P4_SWITCH_APP_HELPER_H */
