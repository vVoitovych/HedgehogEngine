#pragma once

#include "RGTypes.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Renderer
{
    // Everything a registered pass builder receives about one pass instance (RENDERING.md section
    // 6): its name, the graph texture bound to each of its slots, and its parameter strings.
    //
    // GraphInstantiator fills one from an asset, but a C++ caller builds it exactly the same way
    // and calls the same builder function. That is the "data may express nothing C++ cannot" rule
    // made structural: the loader has no other way into a pass.
    class PassInvocation
    {
    public:
        explicit PassInvocation(std::string name) : m_Name(std::move(name)) {}

        const std::string& GetName() const { return m_Name; }

        // The handle currently bound to slot. A builder that writes the slot stores the new
        // version back with SetSlot, so later passes and output binding see the write.
        [[nodiscard]] RGTexture GetSlot(std::string_view slot) const;
        void                    SetSlot(std::string_view slot, RGTexture texture);

        // The raw parameter string, or std::nullopt if the pass instance did not set it. The
        // instantiator has already checked every value against its declared kind.
        [[nodiscard]] std::optional<std::string_view> GetParameter(std::string_view name) const;
        void                                         SetParameter(std::string name, std::string value);

    private:
        std::string                                      m_Name;
        std::vector<std::pair<std::string, RGTexture>>   m_Slots;
        std::vector<std::pair<std::string, std::string>> m_Parameters;
    };
}
