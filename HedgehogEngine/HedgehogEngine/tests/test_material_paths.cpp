#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <string>
#include <vector>

using namespace HedgehogEngine;

TEST_CASE("Material paths - one key however a path is spelt, case kept")
{
    CHECK(NormalizeMaterialPath("Materials/test1.material") == "Materials/test1.material");
    CHECK(NormalizeMaterialPath("Materials\\test1.material") == "Materials/test1.material");
    CHECK(NormalizeMaterialPath("assets://Materials/test1.material") == "Materials/test1.material");
    CHECK(NormalizeMaterialPath("assets://Materials\\test1.material") == "Materials/test1.material");
    CHECK(NormalizeMaterialPath("Models\\DamagedHelmet//./DamagedHelmet_Material_MR.material")
          == "Models/DamagedHelmet/DamagedHelmet_Material_MR.material");
    CHECK(NormalizeMaterialPath("Materials/Test1.material") == "Materials/Test1.material");
    CHECK(NormalizeMaterialPath("").empty());
}

TEST_CASE("Material paths - three spellings of one material load it once, the components keep their text")
{
    EngineContext context;
    ECS::ECS&     ecs          = context.GetECS();
    RenderSystem& renderSystem = *context.GetRenderSystem();
    const size_t  listed       = renderSystem.GetMaterials().size();

    const std::vector<std::string> spellings = { "Materials\\test1.material", "Materials/test1.material",
                                                 "assets://Materials/./test1.material" };
    std::vector<ECS::Entity> entities;
    for (const std::string& spelling : spellings)
    {
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        RenderComponent   render;
        render.Material = spelling;
        ecs.AddComponent(entity, render);
        renderSystem.Update(ecs, entity);
        entities.push_back(entity);
    }

    const auto& first = ecs.GetComponent<RenderComponent>(entities[0]);
    REQUIRE(first.MaterialIndex.has_value());
    for (size_t i = 0; i < entities.size(); ++i)
    {
        CAPTURE(i);
        const auto& render = ecs.GetComponent<RenderComponent>(entities[i]);
        CHECK(render.MaterialIndex == first.MaterialIndex);
        CHECK(render.Material == spellings[i]); // saved as written
    }
    REQUIRE(renderSystem.GetMaterials().size() == listed + 1);
    CHECK(renderSystem.GetMaterials()[*first.MaterialIndex] == "Materials/test1.material");

    // The container loads it once, from its normalized path.
    context.GetResourceCatalog().Update(renderSystem, *context.GetMeshSystem());
    const MaterialContainer& materials = context.GetResourceCatalog().GetMaterialContainer();
    CHECK(materials.GetMaterialCount() == listed + 1);
    CHECK(materials.GetMaterialDataByIndex(*first.MaterialIndex).path == "Materials/test1.material");

    // Re-spelling a component's material keeps its index; another material gets a new one.
    auto& second    = ecs.GetComponent<RenderComponent>(entities[1]);
    second.Material = "assets://Materials\\test1.material";
    renderSystem.Update(ecs, entities[1]);
    CHECK(second.MaterialIndex == first.MaterialIndex);
    second.Material = "Materials/test3.material";
    renderSystem.Update(ecs, entities[1]);
    CHECK(second.MaterialIndex != first.MaterialIndex);
    CHECK(renderSystem.GetMaterials().size() == listed + 2);
}

TEST_CASE("Texture paths - the editor's texture list holds each file once, however materials spell it")
{
    TextureContainer textures;
    textures.RegisterTexturePath("Models\\DamagedHelmet\\Default_albedo.jpg");
    textures.RegisterTexturePath("Models/DamagedHelmet/Default_albedo.jpg");
    textures.RegisterTexturePath("assets://Models/DamagedHelmet/./Default_albedo.jpg");
    textures.RegisterTexturePath("engine://Content\\Textures\\Default\\cells.png");
    textures.RegisterTexturePath("engine://Content/Textures/Default/cells.png");
    textures.RegisterTexturePath(""); // no map: not a texture

    CHECK(textures.GetTexturePathes()
          == std::vector<std::string>{ "Models/DamagedHelmet/Default_albedo.jpg", "engine://Content/Textures/Default/cells.png" });
    CHECK(textures.GetTextureIndex("Models\\DamagedHelmet\\Default_albedo.jpg") == 0);
    CHECK(textures.GetTextureIndex("assets://Models/DamagedHelmet/Default_albedo.jpg") == 0);
    CHECK(textures.GetTextureIndex("engine://Content/Textures/Default/cells.png") == 1);
    CHECK(textures.GetTextureIndex("Textures/missing.png") == 2); // not listed: the count
}
