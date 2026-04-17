#include "MaterialContainer.hpp"
#include "MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"
#include "DialogueWindows/TextureDialogue/TextureDialogue.hpp"

#include "ContentLoader/api/CommonFunctions.hpp"

#include "Scene/Scene.hpp"

namespace Context
{
    MaterialContainer::MaterialContainer(const VulkanContext& context)
    {
        uint32_t materialCount = MAX_MATERIAL_COUNT;

        auto& rhiDevice = context.GetRHIDevice();
        std::vector<RHI::DescriptorBinding> rhiBindings = {
            { 0, RHI::DescriptorType::UniformBuffer,        1, RHI::ShaderStage::Fragment },
            { 1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment },
        };
        m_RHILayout = rhiDevice.CreateDescriptorSetLayout(rhiBindings);

        std::vector<RHI::PoolSize> poolSizes = {
            { RHI::DescriptorType::UniformBuffer,        materialCount },
            { RHI::DescriptorType::CombinedImageSampler, materialCount * MAX_TEXTURES_PER_MATERIAL },
        };
        m_RHIDescriptorPool = rhiDevice.CreateDescriptorPool(materialCount, poolSizes);
    }

    MaterialContainer::~MaterialContainer()
    {
    }

    void MaterialContainer::Update(const Scene::Scene& scene)
    {
        auto materialsInScene = scene.GetMaterials();
        for (size_t i = m_Materials.size(); i < materialsInScene.size(); ++i)
        {
            MaterialData data;
            MaterialSerializer::Deserialize(data, ContentLoader::GetAssetsDirectory() + materialsInScene[i]);
            m_Materials.push_back(data);
        }
    }

    void MaterialContainer::UpdateResources(const VulkanContext& context, const TextureContainer& textureContainer)
    {
        size_t createdResourceCount = m_RHIMaterialUniforms.size();
        for (size_t i = 0; i < createdResourceCount; ++i)
        {
            if (m_Materials[i].isDirty)
            {
                UpdateMaterialByIndex(i, context, textureContainer);
                m_Materials[i].isDirty = false;
            }
        }
        for (size_t i = createdResourceCount; i < m_Materials.size(); ++i)
        {
            CreateMaterialResources(m_Materials[i], context, textureContainer);
        }
    }

    void MaterialContainer::SetMaterialDirty(size_t index)
    {
        m_Materials[index].isDirty = true;
    }

    void MaterialContainer::UpdateMaterialByIndex(size_t index, const VulkanContext& context, const TextureContainer& textureContainer)
    {
        MaterialUniform materialData;
        materialData.transparency = m_Materials[index].transparency;

        m_RHIMaterialUniforms[index]->CopyData(&materialData, sizeof(materialData));
        m_RHIDescriptorSets[index]->WriteUniformBuffer(0, *m_RHIMaterialUniforms[index]);
        m_RHIDescriptorSets[index]->WriteTexture(1,
            textureContainer.GetRHITexture(context, m_Materials[index].baseColor),
            textureContainer.GetRHISampler(context, SamplerType::Linear));
        m_RHIDescriptorSets[index]->Flush();
    }

    void MaterialContainer::CreateMaterialResources(MaterialData& data, const VulkanContext& context, const TextureContainer& textureContainer)
    {
        MaterialUniform materialData;
        materialData.transparency = data.transparency;

        auto& rhiDevice = context.GetRHIDevice();
        auto rhiUniform = rhiDevice.CreateBuffer(
            sizeof(MaterialUniform), RHI::BufferUsage::UniformBuffer, RHI::MemoryUsage::CpuToGpu);
        rhiUniform->CopyData(&materialData, sizeof(materialData));
        m_RHIMaterialUniforms.push_back(std::move(rhiUniform));

        auto rhiSet = rhiDevice.AllocateDescriptorSet(*m_RHIDescriptorPool, *m_RHILayout);
        rhiSet->WriteUniformBuffer(0, *m_RHIMaterialUniforms.back());
        rhiSet->WriteTexture(1,
            textureContainer.GetRHITexture(context, data.baseColor),
            textureContainer.GetRHISampler(context, SamplerType::Linear));
        rhiSet->Flush();
        m_RHIDescriptorSets.push_back(std::move(rhiSet));

        data.isDirty = false;
    }

    void MaterialContainer::Cleanup(const VulkanContext& context)
    {
        context.GetRHIDevice().WaitIdle();
        m_RHIDescriptorSets.clear();
        m_RHIMaterialUniforms.clear();
        m_RHIDescriptorPool.reset();
        m_RHILayout.reset();
    }

    void MaterialContainer::ClearMaterials()
    {
        m_Materials.clear();
    }

    void MaterialContainer::CreateNewMaterial()
    {
        auto path = DialogueWindows::MaterialCreationDialogue();
        if (path != nullptr)
        {
            MaterialData newData;
            newData.type = MaterialType::Opaque;
            newData.transparency = 1.0f;
            newData.baseColor = m_DefaultCellTexture;
            newData.path = ContentLoader::GetAssetRelativetlyPath(path);

            MaterialSerializer::Serialize(newData, path);

            m_Materials.push_back(newData);
        }
    }

    void MaterialContainer::SaveMaterial(size_t index)
    {
        MaterialData newData = m_Materials[index];

        MaterialSerializer::Serialize(newData, ContentLoader::GetAssetsDirectory() + newData.path);
    }

    void MaterialContainer::LoadBaseTexture(size_t index, const VulkanContext& context, const TextureContainer& textureContainer)
    {
        auto texturePath = DialogueWindows::TextureOpenDialogue();
        if (texturePath == nullptr)
            return;
        m_Materials[index].baseColor = ContentLoader::GetAssetRelativetlyPath(texturePath);

        UpdateMaterialByIndex(index, context, textureContainer);
    }

    const RHI::IRHIDescriptorSetLayout& MaterialContainer::GetRHIDescriptorSetLayout() const
    {
        return *m_RHILayout;
    }

    const RHI::IRHIDescriptorSet& MaterialContainer::GetRHIDescriptorSet(size_t index) const
    {
        return *m_RHIDescriptorSets[index];
    }

    MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index)
    {
        return m_Materials[index];
    }

    const MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index) const
    {
        return m_Materials[index];
    }

}
