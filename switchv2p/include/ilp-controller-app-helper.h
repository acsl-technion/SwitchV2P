#ifndef ILP_CONTROLLER_APP_HELPER_H
#define ILP_CONTROLLER_APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ilp-controller-app.h"

class IlpControllerAppHelper
{
public:
  IlpControllerAppHelper (ContainerGroups containerGroups, ApplicationContainer switchApps,
                          uint32_t leafCount, uint32_t spineCount, uint32_t coreCount,
                          uint32_t podWidth, unordered_map<uint32_t, uint32_t> &virtualToPhysical,
                          unordered_map<string, uint32_t> &containerToId,
                          vector<pair<uint32_t, uint32_t>> &gws, vector<Ipv4Address> &gwAddresses)
      : m_containerGroups (containerGroups),
        m_switchApps (switchApps),
        m_leafCount (leafCount),
        m_spineCount (spineCount),
        m_coreCount (coreCount),
        m_podWidth (podWidth),
        m_virtualToPhysical (virtualToPhysical),
        m_containerToId (containerToId),
        m_gws (gws),
        m_gwAddresses (gwAddresses)
  {
    m_factory.SetTypeId (IlpControllerApp::GetTypeId ());
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
    Ptr<IlpControllerApp> app = m_factory.Create<IlpControllerApp> ();
    app->Setup (m_containerGroups, m_switchApps, m_leafCount, m_spineCount, m_coreCount, m_podWidth,
                &m_virtualToPhysical, &m_containerToId, &m_gws, &m_gwAddresses);
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
  ContainerGroups m_containerGroups;
  ApplicationContainer m_switchApps;
  uint32_t m_leafCount, m_spineCount, m_coreCount, m_podWidth;
  unordered_map<uint32_t, uint32_t> &m_virtualToPhysical;
  unordered_map<string, uint32_t> &m_containerToId;
  vector<pair<uint32_t, uint32_t>> &m_gws;
  vector<Ipv4Address> &m_gwAddresses;
};

#endif /* ILP_CONTROLLER_APP_HELPER_H */
