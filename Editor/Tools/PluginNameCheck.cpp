#include "PluginNameCheck.hpp"

#include <algorithm>
#include <cctype>

namespace Editor
{
    std::string CheckNewPluginName(std::string_view name, const std::vector<HedgehogSettings::PluginEntry>& listed)
    {
        if (name.empty())
            return "Type the plugin's DLL name, without .dll.";
        if (!HedgehogSettings::ProjectSettings::IsValidPluginName(name))
            return "1 to 64 of A-Z, a-z, 0-9, '_' and '-': the DLL's name without .dll.";

        const auto sameName = [name](const HedgehogSettings::PluginEntry& entry)
        {
            return std::equal(entry.Name.begin(), entry.Name.end(), name.begin(), name.end(),
                              [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
        };
        if (std::any_of(listed.begin(), listed.end(), sameName))
            return "The project lists it already.";
        return {};
    }
}
