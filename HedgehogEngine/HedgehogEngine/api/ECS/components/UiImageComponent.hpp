#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <string>

namespace HedgehogEngine
{
    // Fills its element's rect with a texture under assets:// tinted by Color (RGBA), or with Color
    // alone when Texture is empty.
HH_BEGIN_COMPONENT(UiImageComponent)
    HH_PROP_NAMED(std::string, Texture, "Texture", std::string{},                     AssetRef)
    HH_PROP_NAMED(HM::Vector4, Color,   "Color",   HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f), IsColor)
HH_END_COMPONENT(UiImageComponent)
}
