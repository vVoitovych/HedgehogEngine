#include "HedgehogLuaDebug/api/DebugServer.hpp"

#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"

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
        m_Pending.clear();
        m_Transport->Stop();
        m_Dispatcher->Reset();
    }

    void DebugServer::Pump()
    {
        for (std::string& body : m_Transport->Receive())
            m_Pending.push_back(std::move(body));
        while (!m_Pending.empty())
        {
            const std::string body       = std::move(m_Pending.front());
            const bool        wasStopped = m_Engine && m_Engine->IsStopped();
            m_Pending.pop_front();
            for (std::string& reply : m_Dispatcher->Handle(body, m_Engine))
                m_Transport->Send(std::move(reply));
            if (m_Dispatcher->WantsDisconnect())
            {
                m_Transport->Disconnect();
                m_Dispatcher->Reset();
                m_Pending.clear();
                if (m_Engine)
                    m_Engine->ClearSession();
                return;
            }
            if (wasStopped && !m_Engine->IsStopped())
                return;
        }
        // The client went away without a disconnect request: the next one starts afresh.
        if (!m_Transport->IsConnected() && m_Dispatcher->IsAttached())
        {
            m_Pending.clear();
            m_Dispatcher->Reset();
            if (m_Engine)
                m_Engine->ClearSession();
        }
    }

    bool DebugServer::IsAttached() const { return m_Dispatcher->IsAttached(); }

    bool DebugServer::IsConfigured() const { return m_Dispatcher->IsConfigured(); }

    ITransport& DebugServer::GetTransport() { return *m_Transport; }

    void DebugServer::SetEngine(LuaDebugEngine* engine) { m_Engine = engine; }

    void DebugServer::SendEvent(const std::string& event, const std::string& bodyJson)
    {
        m_Transport->Send(m_Dispatcher->MakeEventMessage(event, bodyJson));
    }
}
