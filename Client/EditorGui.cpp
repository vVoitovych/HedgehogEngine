#include "EditorGui.hpp"

#include "HedgehogEngine/HedgehogContext/Context/Context.hpp"
#include "HedgehogEngine/HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"

#include "HedgehogEngine/HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"

#include "imgui.h"

#include <stdexcept>

namespace HedgehogClient
{
    void EditorGui::Draw(Context::Context& context)
    {
        DrawGui(context);
    }

    void EditorGui::ShowSettingsWindow()
    {
        m_SettingsWindowShow = true;
    }

    // ─── Internal ────────────────────────────────────────────────────────────

    void EditorGui::DrawGui(Context::Context& context)
    {
        DrawInspector(context);
        DrawSceneInspector(context);
        DrawMainMenu(context);
        DrawSettingsWindow(context);
        ImGui::ShowDemoWindow();  // TODO: remove
    }

    // ─── Inspector ───────────────────────────────────────────────────────────

    void EditorGui::DrawInspector(Context::Context& context)
    {
        const float sizeX = 300.0f;
        const float sizeY = static_cast<float>(ImGui::GetIO().DisplaySize.y);

        ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, 20.0f),
            ImGuiCond_Always, ImVec2(1.0f, 0.0f));

        ImGui::Begin("Inspector", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        if (m_SelectedObject.has_value())
        {
            DrawTitle(context);
            DrawTransform(context);
            DrawLight(context);
            DrawMesh(context);
            DrawRender(context);
            DrawScript(context);
        }
        ImGui::End();
    }

    void EditorGui::DrawTitle(Context::Context& context)
    {
        auto& scene     = context.GetEngineContext().GetScene();
        auto  entity    = m_SelectedObject.value();
        auto& hierarchy = scene.GetHierarchyComponent(entity);
        auto  name      = hierarchy.m_Name;

        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::InputText("Name", &name[0], name.capacity() + 1))
                hierarchy.m_Name = name;
        }
    }

    void EditorGui::DrawTransform(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& transform = scene.GetTransformComponent(entity);

            ImGui::SeparatorText("Position");
            ImGui::DragFloat("pos x", &transform.m_Position.x(), 0.5f);
            ImGui::DragFloat("pos y", &transform.m_Position.y(), 0.5f);
            ImGui::DragFloat("pos z", &transform.m_Position.z(), 0.5f);

            ImGui::SeparatorText("Rotation");
            ImGui::DragFloat("rot x", &transform.m_Rotation.x(), 0.5f);
            ImGui::DragFloat("rot y", &transform.m_Rotation.y(), 0.5f);
            ImGui::DragFloat("rot z", &transform.m_Rotation.z(), 0.5f);

            ImGui::SeparatorText("Scale");
            ImGui::DragFloat("scale x", &transform.m_Scale.x(), 0.5f);
            ImGui::DragFloat("scale y", &transform.m_Scale.y(), 0.5f);
            ImGui::DragFloat("scale z", &transform.m_Scale.z(), 0.5f);
        }
    }

    void EditorGui::DrawMesh(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (scene.HasMeshComponent(entity))
        {
            if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto&    mesh          = scene.GetMeshComponent(entity);
                auto&    meshes        = scene.GetMeshes();
                uint64_t selectedIndex = mesh.m_MeshIndex.value();

                if (ImGui::BeginCombo("mesh", mesh.m_MeshPath.c_str()))
                {
                    for (uint64_t i = 0; i < meshes.size(); ++i)
                    {
                        const bool isSelected = (selectedIndex == i);
                        if (ImGui::Selectable(meshes[i].c_str(), isSelected))
                        {
                            selectedIndex = i;
                            scene.ChangeMeshComponent(entity, meshes[i]);
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Load mesh"))
                    scene.LoadMesh(entity);
                if (ImGui::Button("Remove mesh component"))
                    scene.RemoveMeshComponent(entity);
            }
        }
    }

    void EditorGui::DrawRender(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (scene.HasRenderComponent(entity))
        {
            if (ImGui::CollapsingHeader("Rendering Component", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& render = scene.GetRenderComponent(entity);

                bool enabled = render.m_IsVisible;
                if (ImGui::Checkbox("Visible", &enabled))
                    render.m_IsVisible = enabled;

                auto& materials = scene.GetMaterials();

                if (!materials.empty())
                {
                    if (ImGui::BeginCombo("material", render.m_Material.c_str()))
                    {
                        uint64_t selectedIndex = render.m_MaterialIndex.has_value()
                            ? render.m_MaterialIndex.value() : 0;

                        for (uint64_t i = 0; i < materials.size(); ++i)
                        {
                            const bool isSelected = (selectedIndex == i);
                            if (ImGui::Selectable(materials[i].c_str(), isSelected))
                            {
                                selectedIndex = i;
                                render.m_Material = materials[i];
                                scene.UpdateMaterialComponent(entity);
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                if (ImGui::Button("Load material"))
                    scene.LoadMaterial(entity);
                if (!materials.empty())
                {
                    if (ImGui::Button("Save material"))
                        context.GetEngineContext().GetMaterialContainer().SaveMaterial(
                            render.m_MaterialIndex.value());
                }
                if (ImGui::Button("Remove render component"))
                    scene.RemoveRenderComponent(entity);

                if (!materials.empty() && render.m_MaterialIndex.has_value())
                {
                    ImGui::SeparatorText("Material");
                    auto& materialContainer = context.GetEngineContext().GetMaterialContainer();
                    auto& textureContainer  = context.GetEngineContext().GetTextureContainer();
                    auto& materialData      = materialContainer.GetMaterialDataByIndex(
                        render.m_MaterialIndex.value());

                    const char* types[]  = { "Opaque", "Cutoff", "Transparent" };
                    int materialType = static_cast<int>(materialData.type);
                    ImGui::Combo("Type", &materialType, types, IM_ARRAYSIZE(types));
                    materialData.type = static_cast<Context::MaterialType>(materialType);

                    auto& textures    = textureContainer.GetTexturePathes();
                    int selectedIndex = static_cast<int>(
                        textureContainer.GetTextureIndex(materialData.baseColor));

                    if (ImGui::BeginCombo("baseColor", materialData.baseColor.c_str()))
                    {
                        for (int i = 0; i < static_cast<int>(textures.size()); ++i)
                        {
                            const bool isSelected = (selectedIndex == i);
                            if (ImGui::Selectable(textures[i].c_str(), isSelected))
                            {
                                selectedIndex = i;
                                materialData.baseColor = textures[i];
                                materialContainer.SetMaterialDirty(render.m_MaterialIndex.value());
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Load texture"))
                        materialContainer.LoadBaseTexture(render.m_MaterialIndex.value());

                    if (materialData.type == Context::MaterialType::Transparent)
                    {
                        float materialTransparency = materialData.transparency;
                        ImGui::SliderFloat("slider float", &materialTransparency, 0.0f, 1.0f,
                            "ratio = %.3f");
                        if (materialData.transparency != materialTransparency)
                        {
                            materialData.transparency = materialTransparency;
                            materialContainer.SetMaterialDirty(render.m_MaterialIndex.value());
                        }
                    }
                }
            }
        }
    }

    namespace
    {
        Scene::LightType LightIndexToLightType(int index)
        {
            switch (index)
            {
            case 0: return Scene::LightType::DirectionLight;
            case 1: return Scene::LightType::PointLight;
            case 2: return Scene::LightType::SpotLight;
            default: throw std::runtime_error("Wrong light index");
            }
        }

        int LightTypeToLightIndex(Scene::LightType type)
        {
            switch (type)
            {
            case Scene::LightType::DirectionLight: return 0;
            case Scene::LightType::PointLight:     return 1;
            case Scene::LightType::SpotLight:      return 2;
            default: throw std::runtime_error("Wrong light type");
            }
        }
    }

    void EditorGui::DrawLight(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (scene.HasLightComponent(entity))
        {
            if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& light = scene.GetLightComponent(entity);

                bool enabled = light.m_Enable;
                if (ImGui::Checkbox("Enabled", &enabled))
                    light.m_Enable = enabled;

                const char* types[] = { "Direction Light", "Point Light", "Spot Light" };
                int lightType = LightTypeToLightIndex(light.m_LightType);
                if (ImGui::Combo("Type", &lightType, types, IM_ARRAYSIZE(types)))
                    light.m_LightType = LightIndexToLightType(lightType);

                float color[3] = { light.m_Color.r(), light.m_Color.g(), light.m_Color.b() };
                if (ImGui::ColorEdit3("Color", color))
                    light.m_Color = { color[0], color[1], color[2] };

                float intensity = light.m_Intensity;
                if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 3.0f, "ratio = %.03f"))
                    light.m_Intensity = intensity;

                if (lightType > 0)
                {
                    float radius = light.m_Radius;
                    if (ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f, "ratio = %.3f"))
                        light.m_Radius = radius;

                    if (lightType > 1)
                    {
                        float coneAngle = light.m_ConeAngle;
                        if (ImGui::SliderFloat("Cone angle", &coneAngle, 0.1f, 179.9f, "ratio = %.3f"))
                            light.m_ConeAngle = coneAngle;
                    }
                }

                if (lightType == 0)
                {
                    bool castShadows = light.m_CastShadows;
                    if (ImGui::Checkbox("Cast shadows", &castShadows))
                        scene.UpdateShadowCasting(entity, castShadows);
                }

                if (ImGui::Button("Remove light component"))
                    scene.RemoveLightComponent(entity);
            }
        }
    }

    void EditorGui::DrawScript(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (scene.HasScriptComponent(entity))
        {
            if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& component = scene.GetScriptComponent(entity);

                bool enabled = component.m_Enable;
                if (ImGui::Checkbox("Enabled", &enabled))
                    component.m_NewEnable = enabled;

                std::string name = component.m_ScriptPath.empty()
                    ? "No script selected" : component.m_ScriptPath;
                ImGui::InputText("Selected script:", &name[0], name.capacity() + 1);

                if (ImGui::Button("Load Script"))
                    scene.ChangeScript(entity);

                if (!component.m_Params.empty())
                    ImGui::SeparatorText("Script parameters");

                for (auto& param : component.m_Params)
                {
                    switch (param.second.type)
                    {
                    case Scene::ParamType::Boolean:
                    {
                        bool bVal = std::get<bool>(param.second.value);
                        if (ImGui::Checkbox(param.first.c_str(), &bVal))
                        {
                            param.second.value = bVal;
                            param.second.dirty = true;
                        }
                        break;
                    }
                    case Scene::ParamType::Number:
                    {
                        float nVal = std::get<float>(param.second.value);
                        if (ImGui::DragFloat(param.first.c_str(), &nVal, 0.05f))
                        {
                            param.second.value = nVal;
                            param.second.dirty = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (ImGui::Button("Remove script component"))
                    scene.RemoveScriptComponent(entity);
            }
        }
    }

    // ─── Main menu ───────────────────────────────────────────────────────────

    void EditorGui::DrawMainMenu(Context::Context& context)
    {
        auto& scene = context.GetEngineContext().GetScene();
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))    { scene.ResetScene(); }
                if (ImGui::MenuItem("Rename")) { scene.RenameScene(); }
                if (ImGui::MenuItem("Open"))   { scene.Load(); }
                if (ImGui::MenuItem("Save"))   { scene.Save(); }

                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Alt+F4")) {}
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Create game object"))
                    scene.CreateGameObject(m_SelectedObject);

                ImGui::Separator();
                if (ImGui::BeginMenu("Add component"))
                {
                    if (ImGui::MenuItem("Mesh component") && m_SelectedObject.has_value())
                        scene.AddMeshComponent(m_SelectedObject.value());
                    if (ImGui::MenuItem("Render component") && m_SelectedObject.has_value())
                        scene.AddRenderComponent(m_SelectedObject.value());
                    if (ImGui::MenuItem("Light component") && m_SelectedObject.has_value())
                        scene.AddLightComponent(m_SelectedObject.value());
                    if (ImGui::MenuItem("Script component") && m_SelectedObject.has_value())
                        scene.AddScriptComponent(m_SelectedObject.value());
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Create material"))
                    context.GetEngineContext().GetMaterialContainer().CreateNewMaterial();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::MenuItem("Settings")) { ShowSettingsWindow(); }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    // ─── Scene inspector ─────────────────────────────────────────────────────

    void EditorGui::DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index)
    {
        auto& scene     = context.GetEngineContext().GetScene();
        auto& component = scene.GetHierarchyComponent(entity);

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        bool isSelected = m_SelectedObject.has_value() && entity == m_SelectedObject.value();
        if (isSelected)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;

        auto toggleSelection = [&]()
        {
            if (m_SelectedObject.has_value() && m_SelectedObject.value() == entity)
                m_SelectedObject.reset();
            else
                m_SelectedObject = entity;
        };

        if (!component.m_Children.empty())
        {
            bool node_open = ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());
            if (ImGui::IsItemClicked())
                toggleSelection();
            ++index;
            if (node_open)
            {
                for (auto child : component.m_Children)
                    DrawHierarchyNode(context, child, index);
                ImGui::TreePop();
            }
        }
        else
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                toggleSelection();
            ++index;
        }
    }

    void EditorGui::DrawSceneInspector(Context::Context& context)
    {
        const float sizeX = 300.0f;
        const float sizeY = static_cast<float>(ImGui::GetIO().DisplaySize.y);

        ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(sizeX, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

        auto& scene = context.GetEngineContext().GetScene();
        ImGui::Begin(scene.GetSceneName().c_str(), nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        {
            int index = 0;
            ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);

            if (ImGui::Button("Create object", sz))
                scene.CreateGameObject(m_SelectedObject);
            if (ImGui::Button("Delete object", sz))
            {
                if (m_SelectedObject.has_value() && m_SelectedObject.value() != scene.GetRoot())
                {
                    scene.DeleteGameObject(m_SelectedObject.value());
                    m_SelectedObject.reset();
                }
            }

            DrawHierarchyNode(context, scene.GetRoot(), index);
        }

        ImGui::End();
    }

    // ─── Settings window ─────────────────────────────────────────────────────

    void EditorGui::DrawSettingsWindow(Context::Context& context)
    {
        if (!m_SettingsWindowShow)
            return;

        ImGui::SetNextWindowSize(ImVec2(600.0f, 400.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(750.0f, 150.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));

        ImGui::Begin("Settings Editor", &m_SettingsWindowShow, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Main"))
            {
                if (ImGui::MenuItem("Quit")) { m_SettingsWindowShow = false; }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        auto& settings = context.GetEngineContext().GetSettings();

        if (ImGui::CollapsingHeader("Shadow settings", 32))
        {
            auto& shadowmapSettings = settings.GetShadowmapSettings();

            float split1 = shadowmapSettings->GetSplit1();
            float split2 = shadowmapSettings->GetSplit2();
            float split3 = shadowmapSettings->GetSplit3();

            int shadowmapSize = shadowmapSettings->GetShadowmapSize();
            if (ImGui::InputInt("Shadowmap size", &shadowmapSize))
            {
                shadowmapSettings->SetShadowmapSize(shadowmapSize);
                const auto originalSize = shadowmapSettings->GetShadowmapSize();
                if (originalSize != shadowmapSize)
                    shadowmapSize = originalSize;
            }

            float shadowLambda = shadowmapSettings->GetCascadeSplitLambda();
            if (ImGui::SliderFloat("Shadowmap lambda", &shadowLambda, 0.0f, 1.0f))
                shadowmapSettings->SetCascadeSplitLambda(shadowLambda);

            int cascatesCount = shadowmapSettings->GetCascadesCount();
            if (ImGui::SliderInt("Cascades count", &cascatesCount, 1, 4))
            {
                shadowmapSettings->SetCascadesCount(cascatesCount);
                shadowmapSettings->SetDefaultSplits();

                split1 = shadowmapSettings->GetSplit1();
                split2 = shadowmapSettings->GetSplit2();
                split3 = shadowmapSettings->GetSplit3();
            }

            if (cascatesCount > 1)
            {
                if (ImGui::SliderFloat("Split 1", &split1, 0.0f, split2))
                    shadowmapSettings->SetSplit1(split1);
            }

            if (cascatesCount > 2)
            {
                if (ImGui::SliderFloat("Split 2", &split2, split1, split3))
                    shadowmapSettings->SetSplit2(split2);
            }

            if (cascatesCount > 3)
            {
                if (ImGui::SliderFloat("Split 3", &split3, split2, 100.0f))
                    shadowmapSettings->SetSplit3(split3);
            }

            ImGui::SeparatorText("Debug");
            auto& shadowDir = context.GetEngineContext().GetScene().GetShadowLightDirection();
            if (shadowDir.has_value())
            {
                float x = shadowDir.value().x();
                float y = shadowDir.value().y();
                float z = shadowDir.value().z();

                ImGui::SeparatorText("Shadow direction");
                ImGui::DragFloat("dir x", &x, 0.5f);
                ImGui::DragFloat("dir y", &y, 0.5f);
                ImGui::DragFloat("dir z", &z, 0.5f);
            }
        }

        ImGui::End();
    }
}
