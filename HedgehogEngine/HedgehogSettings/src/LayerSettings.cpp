#include "HedgehogSettings/api/LayerSettings.hpp"

#include <cassert>

namespace HedgehogSettings
{
    namespace
    {
        const std::string s_EmptyName;
        const std::string s_InvalidDisplayName = "<invalid layer>";
    }

    LayerSettings::LayerSettings()
    {
        m_Names[0]            = "Default";
        m_Names[EDITOR_LAYER] = "Editor";
        for (uint32_t layer = 0; layer < LAYER_COUNT; ++layer)
        {
            RefreshDisplayName(layer);
        }
    }

    const std::string& LayerSettings::GetLayerName(uint32_t layer) const
    {
        assert(layer < LAYER_COUNT && "Layer index out of range.");
        if (layer >= LAYER_COUNT)
        {
            return s_EmptyName;
        }
        return m_Names[layer];
    }

    const std::string& LayerSettings::GetLayerDisplayName(uint32_t layer) const
    {
        assert(layer < LAYER_COUNT && "Layer index out of range.");
        if (layer >= LAYER_COUNT)
        {
            return s_InvalidDisplayName;
        }
        return m_DisplayNames[layer];
    }

    void LayerSettings::SetLayerName(uint32_t layer, const std::string& name)
    {
        assert(layer < LAYER_COUNT && "Layer index out of range.");
        if (layer >= LAYER_COUNT)
        {
            return;
        }
        if (layer == 0 && name.empty())
        {
            return;
        }
        if (m_Names[layer] == name)
        {
            return;
        }

        m_Names[layer] = name;
        RefreshDisplayName(layer);
        m_IsDirty = true;
    }

    bool LayerSettings::IsDirty() const
    {
        return m_IsDirty;
    }

    void LayerSettings::CleanDirtyState()
    {
        m_IsDirty = false;
    }

    void LayerSettings::RefreshDisplayName(uint32_t layer)
    {
        if (m_Names[layer].empty())
        {
            m_DisplayNames[layer] = "Layer " + std::to_string(layer);
        }
        else
        {
            m_DisplayNames[layer] = m_Names[layer];
        }
    }
}
