#include "ViewRegistry.hpp"

#include "RenderView.hpp"

#include <algorithm>

namespace Renderer
{
    ViewRegistry::Handle ViewRegistry::CreateView(std::string name, RenderResourceLedger& ledger)
    {
        const Handle handle = m_NextHandle++;
        m_Views[handle]     = std::make_unique<RenderView>(std::move(name), ledger);
        m_Order.push_back(handle);
        return handle;
    }

    void ViewRegistry::DestroyView(Handle handle)
    {
        const auto it = m_Views.find(handle);
        if (it == m_Views.end())
            return;

        m_Views.erase(it);
        m_Order.erase(std::remove(m_Order.begin(), m_Order.end(), handle), m_Order.end());
    }

    std::optional<ViewRegistry::Handle> ViewRegistry::FindByName(std::string_view name) const
    {
        for (Handle handle : m_Order)
        {
            const auto it = m_Views.find(handle);
            if (it != m_Views.end() && it->second->GetName() == name)
                return handle;
        }
        return std::nullopt;
    }

    RenderView* ViewRegistry::Get(Handle handle)
    {
        const auto it = m_Views.find(handle);
        return it != m_Views.end() ? it->second.get() : nullptr;
    }

    const RenderView* ViewRegistry::Get(Handle handle) const
    {
        const auto it = m_Views.find(handle);
        return it != m_Views.end() ? it->second.get() : nullptr;
    }

    std::vector<std::string> ViewRegistry::GetAllViewOutputNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Order.size());
        for (Handle handle : m_Order)
            names.push_back(m_Views.at(handle)->OutputColorName());
        return names;
    }
}
