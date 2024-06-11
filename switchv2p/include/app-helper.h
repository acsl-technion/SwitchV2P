#ifndef APP_HELPER_H
#define APP_HELPER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

template <typename T>
class AppHelper
{
public:
  AppHelper ()
  {
    m_factory.SetTypeId (T::GetTypeId ());
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

  void
  SetAttribute (std::string name, const AttributeValue &value)
  {
    m_factory.Set (name, value);
  }

private:
  Ptr<Application>
  InstallPriv (Ptr<Node> node) const
  {
    Ptr<Application> app = m_factory.Create<T> ();
    node->AddApplication (app);

    return app;
  }
  ObjectFactory m_factory;
};

#endif /* APP_HELPER_H */
