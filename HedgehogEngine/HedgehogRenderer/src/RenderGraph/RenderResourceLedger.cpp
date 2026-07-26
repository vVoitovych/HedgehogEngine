#include "RenderResourceLedger.hpp"

#include "RHI/api/IRHITexture.hpp"

#include <cassert>

namespace Renderer
{
    RenderResourceLedger::LedgerEntry& RenderResourceLedger::getOrCreate(const std::string& scopedName)
    {
        return m_Entries[scopedName];
    }

    void RenderResourceLedger::Publish(const std::string& scopedName, RHI::IRHITexture* texture)
    {
        assert(texture && "Publishing a null texture to the resource ledger.");
        LedgerEntry& entry = getOrCreate(scopedName);
        entry.Texture = texture;
        ++entry.Generation;
    }

    RHI::IRHITexture* RenderResourceLedger::Resolve(const std::string& scopedName) const
    {
        const auto it = m_Entries.find(scopedName);
        return it != m_Entries.end() ? it->second.Texture : nullptr;
    }

    uint32_t RenderResourceLedger::GetGeneration(const std::string& scopedName) const
    {
        const auto it = m_Entries.find(scopedName);
        return it != m_Entries.end() ? it->second.Generation : 0;
    }

    RHI::ImageLayout RenderResourceLedger::GetLayout(const std::string& scopedName) const
    {
        const auto it = m_Entries.find(scopedName);
        assert(it != m_Entries.end() && "Reading the layout of an unpublished graph resource.");
        return it != m_Entries.end() ? it->second.CurrentLayout : RHI::ImageLayout::Undefined;
    }

    void RenderResourceLedger::SetLayout(const std::string& scopedName, RHI::ImageLayout layout)
    {
        getOrCreate(scopedName).CurrentLayout = layout;
    }

    void RenderResourceLedger::Erase(const std::string& scopePrefix)
    {
        const std::string prefix = scopePrefix + "/";
        for (auto it = m_Entries.begin(); it != m_Entries.end(); )
        {
            if (it->first.compare(0, prefix.size(), prefix) == 0)
                it = m_Entries.erase(it);
            else
                ++it;
        }
    }
}
