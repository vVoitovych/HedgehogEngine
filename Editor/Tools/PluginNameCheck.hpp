#pragma once

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    // Why name cannot be added to the project's plugin list (not a plugin name, or listed already,
    // ignoring case), or empty when it can. ImGui-free, so EditorTest checks it.
    [[nodiscard]] std::string CheckNewPluginName(std::string_view name, const std::vector<HedgehogSettings::PluginEntry>& listed);
}
