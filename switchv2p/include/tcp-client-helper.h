#ifndef TCP_CLIENT_APP_HELPER_H
#define TCP_CLIENT_APP_HELPER_H

#include "tcp-client-app.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"

class TcpClientAppHelper
{
  public:
    TcpClientAppHelper(InetSocketAddress remoteAddress,
                       InetSocketAddress localAddress,
                       Ipv4Address sourceAddress,
                       uint32_t maxBytes,
                       uint32_t flowId)
        : m_remoteAddress(remoteAddress),
          m_localAddress(localAddress),
          m_sourceAddress(sourceAddress),
          m_maxBytes(maxBytes),
          m_flowId(flowId)
    {
        m_factory.SetTypeId(TcpClientApp::GetTypeId());
    }

    ApplicationContainer Install(Ptr<Node> node) const
    {
        return ApplicationContainer(InstallPriv(node));
    }

    ApplicationContainer Install(NodeContainer c) const
    {
        ApplicationContainer apps;
        for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
        {
            apps.Add(InstallPriv(*i));
        }

        return apps;
    }

  private:
    Ptr<Application> InstallPriv(Ptr<Node> node) const
    {
        Ptr<TcpClientApp> app = m_factory.Create<TcpClientApp>();
        app->Setup(m_remoteAddress, m_localAddress, m_sourceAddress, m_maxBytes, m_flowId);
        node->AddApplication(app);

        return app;
    }

    ObjectFactory m_factory;
    InetSocketAddress m_remoteAddress, m_localAddress;
    Ipv4Address m_sourceAddress;
    uint32_t m_maxBytes, m_flowId;
};

#endif /* TCP_CLIENT_APP_HELPER_H */
