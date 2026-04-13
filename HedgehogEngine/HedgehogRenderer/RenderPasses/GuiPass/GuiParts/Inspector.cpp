#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <stdexcept>

namespace Renderer
{
    void GuiPass::DrawTitle(Context::Context& context)
    {
        auto& scene    = context.GetEngineContext().GetScene();
        auto  entity   = m_SelectedObject.value();
        auto& hierarchy = scene.GetHierarchyComponent(entity);
        auto  name     = hierarchy.m_Name;

        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::InputText("Name", &name[0], name.capacity() + 1))
            {
                hierarchy.m_Name = name;
            }
        }
    }

    void GuiPass::DrawTransform(Context::Context& context)
    {
        auto& scene    = context.GetEngineContext().GetScene();
        auto  entity   = m_SelectedObject.value();

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

    void GuiPass::DrawMesh(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedObject.value();

        if (scene.HasMeshComponent(entity))
        {
            if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& mesh         = scene.GetMeshComponent(entity);
                auto& meshes       = scene.GetMeshes();
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
                {
                    scene.LoadMesh(entity);
                }
                if (ImGui::Button("Remove mesh component"))
                {
                    scene.RemoveMeshComponent(entity);
                }
            }
        }
    }

    void GuiPass::DrawRender(Context::Context& context)
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
                {
                    render.m_IsVisible = enabled;
                }

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
                {
                    scene.LoadMaterial(entity);
                }
                if (!materials.empty())
                {
                    if (ImGui::Button("Save material"))
                    {
                        context.GetEngineContext().GetMaterialContainer().SaveMaterial(
                            render.m_MaterialIndex.value());
                    }
                }
                if (ImGui::Button("Remove render component"))
                {
                    scene.RemoveRenderComponent(entity);
                }

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

                    auto& textures     = textureContainer.GetTexturePathes();
                    int selectedIndex  = static_cast<int>(
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
                    {
                        materialContainer.LoadBaseTexture(render.m_MaterialIndex.value(),
                            context.GetVulkanContext(), textureContainer);
                    }

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
    } // anonymous namespace

    void GuiPass::DrawLight(Context::Context& context)
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
                {
                    light.m_Enable = enabled;
                }

                const char* types[] = { "Direction Light", "Point Light", "Spot Light" };
                int lightType = LightTypeToLightIndex(light.m_LightType);
                if (ImGui::Combo("Type", &lightType, types, IM_ARRAYSIZE(types)))
                {
                    light.m_LightType = LightIndexToLightType(lightType);
                }

                float color[3] = { light.m_Color.r(), light.m_Color.g(), light.m_Color.b() };
                if (ImGui::ColorEdit3("Color", color))
                {
                    light.m_Color = { color[0], color[1], color[2] };
                }

                float intensity = light.m_Intensity;
                if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 3.0f, "ratio = %.03f"))
                {
                    light.m_Intensity = intensity;
                }

                if (lightType > 0)
                {
                    float radius = light.m_Radius;
                    if (ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f, "ratio = %.3f"))
                    {
                        light.m_Radius = radius;
                    }

                    if (lightType > 1)
                    {
                        float coneAngle = light.m_ConeAngle;
                        if (ImGui::SliderFloat("Cone angle", &coneAngle, 0.1f, 179.9f, "ratio = %.3f"))
                        {
                            light.m_ConeAngle = coneAngle;
                        }
                    }
                }

                if (lightType == 0)
                {
                    bool castShadows = light.m_CastShadows;
                    if (ImGui::Checkbox("Cast shadows", &castShadows))
                    {
                        scene.UpdateShadowCasting(entity, castShadows);
                    }
                }

                if (ImGui::Button("Remove light component"))
                {
                    scene.RemoveLightComponent(entity);
                }
            }
        }
    }

    void GuiPass::DrawScript(Context::Context& context)
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
                {
                    component.m_NewEnable = enabled;
                }

                std::string name = component.m_ScriptPath.empty()
                    ? "No script selected" : component.m_ScriptPath;
                ImGui::InputText("Selected script:", &name[0], name.capacity() + 1);

                if (ImGui::Button("Load Script"))
                {
                    scene.ChangeScript(entity);
                }

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
                {
                    scene.RemoveScriptComponent(entity);
                }
            }
        }
    }

    void GuiPass::DrawInspector(Context::Context& context)
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
}
