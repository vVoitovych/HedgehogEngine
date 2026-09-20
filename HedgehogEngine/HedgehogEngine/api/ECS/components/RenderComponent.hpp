#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(RenderComponent)
    HH_PROP_NAMED(bool,        IsVisible, "Visible",  true,          None)
    HH_PROP_NAMED(std::string, Material,  "Material", std::string{}, None)

    // Index into HedgehogSettings::LayerSettings, not a mask: an object is in exactly one
    // layer, a camera carries a mask of them (RENDERING.md section 2). Scene files store this
    // index and never the layer's name, so renaming a layer cannot re-bucket anything.
    // Defaults to 0 ("Default"), which is what scene files written before layers existed load as.
    // Flagged None rather than given a custom widget: RenderComponent's inspector is hand-drawn
    // in EditorGui::DrawRenderComponent, so reflection here serves serialisation only.
    HH_PROP_NAMED(uint32_t,    Layer,     "Layer",    0u,            None)

    std::optional<uint64_t> MaterialIndex; // runtime-only
HH_END_COMPONENT(RenderComponent)
}
