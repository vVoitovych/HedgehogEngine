#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogSettings/api/RenderingSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>

namespace HedgehogSettings
{
    Settings::Settings()
    {
        m_ShadowmapSettings = std::make_unique<ShadowmapSettings>();
        m_LayerSettings     = std::make_unique<LayerSettings>();
        m_RenderingSettings = std::make_unique<RenderingSettings>();
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

    RenderingSettings& Settings::GetRenderingSettings()
    {
        return *m_RenderingSettings;
    }

    const RenderingSettings& Settings::GetRenderingSettings() const
    {
        return *m_RenderingSettings;
    }

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
            }

            if (const YAML::Node rendering = root["rendering"])
            {
                if (const YAML::Node n = rendering["use_render_graph"])
                {
                    m_RenderingSettings->SetUseRenderGraph(n.as<bool>());
                }
            }

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
        out << YAML::EndMap;

        out << YAML::Key << "rendering" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "use_render_graph" << YAML::Value << m_RenderingSettings->GetUseRenderGraph();
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
