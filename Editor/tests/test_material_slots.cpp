#include "doctest/doctest/doctest.h"

#include "Panels/MaterialSlots.hpp"

#include <set>
#include <string>

using HedgehogEngine::MaterialTextureSlot;

TEST_CASE("Material slots - every map slot once, in the inspector's order, with distinct ids")
{
    const auto slots = Editor::GetMaterialSlots();
    REQUIRE(slots.size() == HedgehogEngine::MATERIAL_TEXTURE_SLOT_COUNT);
    CHECK(slots[0].Slot == MaterialTextureSlot::BaseColor);
    CHECK(slots[1].Slot == MaterialTextureSlot::MetallicRoughness);
    CHECK(slots[2].Slot == MaterialTextureSlot::Normal);
    CHECK(slots[3].Slot == MaterialTextureSlot::Occlusion);
    CHECK(slots[4].Slot == MaterialTextureSlot::Emissive);

    std::set<MaterialTextureSlot> seen;
    std::set<std::string>         ids;
    std::set<std::string>         labels;
    for (const Editor::MaterialSlotInfo& slot : slots)
    {
        seen.insert(slot.Slot);
        ids.insert(slot.Id);
        labels.insert(slot.Label);
        CHECK(std::string(slot.Id).starts_with("##"));
    }
    CHECK(seen.size() == slots.size());
    CHECK(ids.size() == slots.size());
    CHECK(labels.size() == slots.size());
}

TEST_CASE("Material slots - texture paths as a material names them, and glTF files recognised")
{
    CHECK(Editor::ToMaterialTexturePath("assets://Textures/a.png") == "Textures/a.png");
    CHECK(Editor::ToMaterialTexturePath("assets://Models\\Helmet\\albedo.jpg") == "Models/Helmet/albedo.jpg");
    CHECK_FALSE(Editor::ToMaterialTexturePath("engine://Content/Textures/Default/cells.png").has_value());
    CHECK_FALSE(Editor::ToMaterialTexturePath("Textures/a.png").has_value());

    CHECK(Editor::IsGltfPath("assets://Models/Helmet/DamagedHelmet.gltf"));
    CHECK(Editor::IsGltfPath("car.GLB"));
    CHECK_FALSE(Editor::IsGltfPath("assets://Models/viking_room.obj"));
    CHECK_FALSE(Editor::IsGltfPath("gltf"));
}
