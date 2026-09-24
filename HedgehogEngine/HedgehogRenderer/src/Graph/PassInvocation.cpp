#include "HedgehogRenderer/Graph/PassInvocation.hpp"

#include <cassert>

namespace Renderer
{
    RGTexture PassInvocation::GetSlot(std::string_view slot) const
    {
        for (const auto& [name, texture] : m_Slots)
        {
            if (name == slot)
                return texture;
        }
        assert(false && "PassInvocation::GetSlot: slot was never bound; the pass type did not declare it.");
        return RGTexture{};
    }

    void PassInvocation::SetSlot(std::string_view slot, RGTexture texture)
    {
        for (auto& [name, bound] : m_Slots)
        {
            if (name == slot)
            {
                bound = texture;
                return;
            }
        }
        m_Slots.emplace_back(std::string(slot), texture);
    }

    std::optional<std::string_view> PassInvocation::GetParameter(std::string_view name) const
    {
        for (const auto& [key, value] : m_Parameters)
        {
            if (key == name)
                return value;
        }
        return std::nullopt;
    }

    void PassInvocation::SetParameter(std::string name, std::string value)
    {
        m_Parameters.emplace_back(std::move(name), std::move(value));
    }
}
