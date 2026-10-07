#pragma once

#include "HedgehogSettings/api/PhysicsSettings.hpp"

#include "yaml-cpp/yaml.h"

namespace HedgehogSettings
{
    // Reads engine_settings.yaml's physics section into settings, which start as the defaults or
    // as they were: a missing key keeps its value, a bad one keeps it with one warning.
    void ReadPhysicsSettings(const YAML::Node& section, PhysicsSettings& settings);

    // Writes the whole section as one value of the open map.
    void WritePhysicsSettings(YAML::Emitter& out, const PhysicsSettings& settings);
}
