#include "HedgehogLuaDebug/api/DebugServer.hpp"

#include "ProtocolDispatcher.hpp"

namespace LuaDebug
{
    DebugServer::DebugServer(std::unique_ptr<ITransport> transport)
        : m_Transport(std::move(transport))
        , m_Dispatcher(std::make_unique<ProtocolDispatcher>())
    {
    }

    DebugServer::~DebugServer() { Stop(); }

    bool DebugServer::Start()
    {
        m_Dispatcher->Reset();
        return m_Transport->Start();
    }

    void DebugServer::Stop()
    {
        m_Transport->Stop();
        m_Dispatcher->Reset();
    }

    void DebugServer::Pump()
    {
        for (const std::string& body : m_Transport->Receive())
        {
            for (std::string& reply : m_Dispatcher->Handle(body))
                m_Transport->Send(std::move(reply));
            if (m_Dispatcher->WantsDisconnect())
            {
                m_Transport->Disconnect();
                m_Dispatcher->Reset();
                return;
            }
        }
        // The client went away without a disconnect request: the next one starts afresh.
        if (!m_Transport->IsConnected() && m_Dispatcher->IsAttached())
            m_Dispatcher->Reset();
    }

    bool DebugServer::IsAttached() const { return m_Dispatcher->IsAttached(); }

    bool DebugServer::IsConfigured() const { return m_Dispatcher->IsConfigured(); }

    ITransport& DebugServer::GetTransport() { return *m_Transport; }
}
