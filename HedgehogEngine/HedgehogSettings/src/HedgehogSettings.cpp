#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "PhysicsSettingsYaml.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace HedgehogSettings
{
    namespace
    {
        std::string ToText(double value)
        {
            std::ostringstream text;
            text << value;
            return text.str();
        }

        // A number of shadowmap: in [0, max], set through set. One that is not a (whole, when whole
        // is set) finite number keeps its value, and one out of range is clamped, each with one
        // warning naming the key.
        template<typename SetFn>
        void ReadShadowValue(const YAML::Node& shadowmap, const char* key, double max, bool whole, SetFn set)
        {
            const YAML::Node node = shadowmap[key];
            if (!node)
                return;

            double value = 0.0;
            if (!node.IsScalar() || !YAML::convert<double>::decode(node, value) || !std::isfinite(value)
                || (whole && value != std::floor(value)))
            {
                LOGWARNING("[Settings] shadowmap." + std::string(key) + ": '" + YAML::Dump(node) + "' is not "
                           + (whole ? "a whole number" : "a number") + "; it keeps its value.");
                return;
            }
            if (value < 0.0 || value > max)
            {
                LOGWARNING("[Settings] shadowmap." + std::string(key) + ": " + ToText(value) + " is outside 0 to "
                           + ToText(max) + "; it is clamped.");
                value = std::clamp(value, 0.0, max);
            }
            set(value);
        }

        // The forward pass's shadow sampling values, which ShadowmapSettings documents.
        void ReadShadowSampling(const YAML::Node& shadowmap, ShadowmapSettings& shadow)
        {
            ReadShadowValue(shadowmap, "depth_bias", ShadowmapSettings::MAX_DEPTH_BIAS, false,
                            [&](double v) { shadow.SetDepthBias(static_cast<float>(v)); });
            ReadShadowValue(shadowmap, "slope_bias", ShadowmapSettings::MAX_SLOPE_BIAS, false,
                            [&](double v) { shadow.SetSlopeBias(static_cast<float>(v)); });
            ReadShadowValue(shadowmap, "normal_offset", ShadowmapSettings::MAX_NORMAL_OFFSET, false,
                            [&](double v) { shadow.SetNormalOffset(static_cast<float>(v)); });
            ReadShadowValue(shadowmap, "pcf_radius", ShadowmapSettings::MAX_PCF_RADIUS, true,
                            [&](double v) { shadow.SetPcfRadius(static_cast<uint32_t>(v)); });
            ReadShadowValue(shadowmap, "cascade_blend", ShadowmapSettings::MAX_CASCADE_BLEND, false,
                            [&](double v) { shadow.SetCascadeBlend(static_cast<float>(v)); });
        }
    }

    Settings::Settings()
    {
        m_ShadowmapSettings = std::make_unique<ShadowmapSettings>();
        m_LayerSettings     = std::make_unique<LayerSettings>();
        m_ProjectSettings   = std::make_unique<ProjectSettings>();
    }

    Settings::~Settings()
    {
    }

    std::unique_ptr<ShadowmapSettings>& Settings::GetShadowmapSettings()
    {
        return m_ShadowmapSettings;
    }

    const std::unique_ptr<ShadowmapSettings>& Settings::GetShadowmapSettings() const
    {
        return m_ShadowmapSettings;
    }

    std::unique_ptr<LayerSettings>& Settings::GetLayerSettings()
    {
        return m_LayerSettings;
    }

    const std::unique_ptr<LayerSettings>& Settings::GetLayerSettings() const
    {
        return m_LayerSettings;
    }

    ProjectSettings& Settings::GetProjectSettings()
    {
        return *m_ProjectSettings;
    }

    const ProjectSettings& Settings::GetProjectSettings() const
    {
        return *m_ProjectSettings;
    }

    LuaDebuggerSettings& Settings::GetLuaDebuggerSettings() { return m_LuaDebuggerSettings; }

    const LuaDebuggerSettings& Settings::GetLuaDebuggerSettings() const { return m_LuaDebuggerSettings; }

    PhysicsSettings& Settings::GetPhysicsSettings() { return m_PhysicsSettings; }

    const PhysicsSettings& Settings::GetPhysicsSettings() const { return m_PhysicsSettings; }

    bool Settings::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            return false;
        }

        try
        {
            const YAML::Node root = YAML::Load(*text);

            if (const YAML::Node shadowmap = root["shadowmap"])
            {
                if (const YAML::Node n = shadowmap["size"])
                {
                    m_ShadowmapSettings->SetShadowmapSize(n.as<uint32_t>());
                }
                if (const YAML::Node n = shadowmap["cascades_count"])
                {
                    m_ShadowmapSettings->SetCascadesCount(n.as<uint32_t>());
                }
                if (const YAML::Node n = shadowmap["cascade_split_lambda"])
                {
                    m_ShadowmapSettings->SetCascadeSplitLambda(n.as<float>());
                }
                if (const YAML::Node n = shadowmap["caster_mask"])
                {
                    m_ShadowmapSettings->SetShadowCasterMask(n.as<uint32_t>());
                }

                // Farthest split first. Each setter clamps against its neighbours, so restoring
                // 60/70/80 in ascending order would clamp split1 against the *default* split2
                // (25) and silently collapse the cascade layout.
                if (const YAML::Node n = shadowmap["split3"])
                {
                    m_ShadowmapSettings->SetSplit3(n.as<float>());
                }
                if (const YAML::Node n = shadowmap["split2"])
                {
                    m_ShadowmapSettings->SetSplit2(n.as<float>());
                }
                if (const YAML::Node n = shadowmap["split1"])
                {
                    m_ShadowmapSettings->SetSplit1(n.as<float>());
                }
                ReadShadowSampling(shadowmap, *m_ShadowmapSettings);
            }

            // A "rendering" section from before the render graph became the only path
            // (use_render_graph) is ignored, and dropped on the next Save.

            if (const YAML::Node layers = root["layers"])
            {
                for (const auto& entry : layers)
                {
                    const uint32_t layer = entry.first.as<uint32_t>();
                    if (layer < LayerSettings::LAYER_COUNT)
                    {
                        m_LayerSettings->SetLayerName(layer, entry.second.as<std::string>());
                    }
                }
            }

            // A "game" section (data_version) from before project settings is ignored and dropped
            // on the next Save: the game data version lives in Project.yaml.

            if (const YAML::Node debugger = root["lua_debugger"])
            {
                if (const YAML::Node n = debugger["enabled"])
                    m_LuaDebuggerSettings.Enabled = n.as<bool>();
                if (const YAML::Node n = debugger["port"])
                {
                    const int port = n.as<int>();
                    if (port >= 1 && port <= 65535)
                        m_LuaDebuggerSettings.Port = static_cast<uint16_t>(port);
                    else
                        LOGWARNING("Settings::Load: lua_debugger.port ", port, " is not a port; using ",
                                   LuaDebuggerSettings::DEFAULT_PORT, ".");
                }
            }

            if (const YAML::Node physics = root["physics"])
                ReadPhysicsSettings(physics, m_PhysicsSettings);
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("Settings::Load: malformed engine settings, keeping defaults (path: ", virtualPath,
                     ", error: ", e.what(), ")");
            return false;
        }

        // Dirty flags are left exactly as the setters left them. At startup the caller clears
        // them (no GPU resources exist yet); a reload at runtime wants them set so the renderer
        // resizes to the values just read.
        return true;
    }

    bool Settings::Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "shadowmap" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "size"                 << YAML::Value << m_ShadowmapSettings->GetShadowmapSize();
        out << YAML::Key << "cascades_count"       << YAML::Value << m_ShadowmapSettings->GetCascadesCount();
        out << YAML::Key << "cascade_split_lambda" << YAML::Value << m_ShadowmapSettings->GetCascadeSplitLambda();
        out << YAML::Key << "split1"               << YAML::Value << m_ShadowmapSettings->GetSplit1();
        out << YAML::Key << "split2"               << YAML::Value << m_ShadowmapSettings->GetSplit2();
        out << YAML::Key << "split3"               << YAML::Value << m_ShadowmapSettings->GetSplit3();
        out << YAML::Key << "caster_mask"          << YAML::Value << m_ShadowmapSettings->GetShadowCasterMask();
        out << YAML::Key << "depth_bias"           << YAML::Value << m_ShadowmapSettings->GetDepthBias();
        out << YAML::Key << "slope_bias"           << YAML::Value << m_ShadowmapSettings->GetSlopeBias();
        out << YAML::Key << "normal_offset"        << YAML::Value << m_ShadowmapSettings->GetNormalOffset();
        out << YAML::Key << "pcf_radius"           << YAML::Value << m_ShadowmapSettings->GetPcfRadius();
        out << YAML::Key << "cascade_blend"        << YAML::Value << m_ShadowmapSettings->GetCascadeBlend();
        out << YAML::EndMap;

        // Keyed by index, and only named slots are written: the index is the identity that
        // scenes store, the name is just a label for it.
        out << YAML::Key << "layers" << YAML::Value << YAML::BeginMap;
        for (uint32_t layer = 0; layer < LayerSettings::LAYER_COUNT; ++layer)
        {
            const std::string& name = m_LayerSettings->GetLayerName(layer);
            if (!name.empty())
            {
                out << YAML::Key << layer << YAML::Value << name;
            }
        }
        out << YAML::EndMap;

        out << YAML::Key << "lua_debugger" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "enabled" << YAML::Value << m_LuaDebuggerSettings.Enabled;
        out << YAML::Key << "port"    << YAML::Value << m_LuaDebuggerSettings.Port;
        out << YAML::EndMap;

        out << YAML::Key << "physics" << YAML::Value;
        WritePhysicsSettings(out, m_PhysicsSettings);

        out << YAML::EndMap;

        if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
        {
            LOGERROR("Settings::Save: failed to write engine settings (path: ", virtualPath, ")");
            return false;
        }
        return true;
    }

    bool Settings::IsDirty() const
    {
        return m_ShadowmapSettings->IsDirty();
    }

    void Settings::CleanDirtyState()
    {
        m_ShadowmapSettings->CleanDirtyState();
    }
}
