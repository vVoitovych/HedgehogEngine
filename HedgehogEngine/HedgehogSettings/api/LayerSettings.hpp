#pragma once

#include "HedgehogSettingsApi.hpp"

#include <cstdint>
#include <string>

namespace HedgehogSettings
{
    // Layer names for the 32 render layers (RENDERING.md section 2).
    //
    // A renderable object belongs to exactly one layer; a camera carries a mask of layers.
    // Scenes serialise the layer *index* only — the names here are purely a display
    // affordance, so renaming a layer relabels it and can never re-bucket objects.
    class LayerSettings
    {
    public:
        static constexpr uint32_t LAYER_COUNT = 32;

        HEDGEHOG_SETTINGS_API LayerSettings();

        ~LayerSettings() = default;
        LayerSettings(const LayerSettings&) = delete;
        LayerSettings(LayerSettings&&) = delete;
        LayerSettings& operator=(const LayerSettings&) = delete;
        LayerSettings& operator=(LayerSettings&&) = delete;

        // The author-assigned name, empty for a slot that has never been named.
        HEDGEHOG_SETTINGS_API const std::string& GetLayerName(uint32_t layer) const;

        // What the UI shows: the assigned name, or "Layer <n>" when unnamed. Never empty, so
        // every slot stays selectable in a combo.
        HEDGEHOG_SETTINGS_API const std::string& GetLayerDisplayName(uint32_t layer) const;

        // Out-of-range indices are ignored, as is blanking layer 0 — every scene file without an
        // explicit layer deserialises to 0, so that slot always keeps a name.
        HEDGEHOG_SETTINGS_API void SetLayerName(uint32_t layer, const std::string& name);

        HEDGEHOG_SETTINGS_API bool IsDirty() const;
        HEDGEHOG_SETTINGS_API void CleanDirtyState();

    private:
        void RefreshDisplayName(uint32_t layer);

        std::string m_Names[LAYER_COUNT];
        std::string m_DisplayNames[LAYER_COUNT];
        bool        m_IsDirty = false;
    };
}
