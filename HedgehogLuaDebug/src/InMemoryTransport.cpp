#include "HedgehogLuaDebug/api/Transport.hpp"

#include <utility>

namespace LuaDebug
{
    bool InMemoryTransport::Start()
    {
        std::lock_guard lock(m_Mutex);
        m_Connected = true;
        return true;
    }

    void InMemoryTransport::Stop() { Disconnect(); }

    std::vector<std::string> InMemoryTransport::Receive()
    {
        std::lock_guard          lock(m_Mutex);
        std::vector<std::string> messages(m_Incoming.begin(), m_Incoming.end());
        m_Incoming.clear();
        return messages;
    }

    void InMemoryTransport::Send(std::string body)
    {
        std::lock_guard lock(m_Mutex);
        if (m_Connected)
            m_Outgoing.push_back(std::move(body));
    }

    bool InMemoryTransport::IsConnected() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Connected;
    }

    void InMemoryTransport::Disconnect()
    {
        std::lock_guard lock(m_Mutex);
        m_Connected = false;
        m_Incoming.clear();
    }

    void InMemoryTransport::PushFromClient(std::string body)
    {
        std::lock_guard lock(m_Mutex);
        m_Incoming.push_back(std::move(body));
    }

    std::vector<std::string> InMemoryTransport::TakeSent()
    {
        std::lock_guard lock(m_Mutex);
        return std::exchange(m_Outgoing, {});
    }
}
