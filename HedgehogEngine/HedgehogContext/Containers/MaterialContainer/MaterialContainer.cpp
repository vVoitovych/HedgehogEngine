#include "MaterialContainer.hpp"
#include "MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Sampler/Sampler.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"
#include "DialogueWindows/TextureDialogue/TextureDialogue.hpp"

#include "ContentLoader/CommonFunctions.hpp"

#include "Scene/Scene.hpp"

namespace Context
{
    MaterialContainer::MaterialContainer(const VulkanContext& context)
    {
        uint32_t materialCount = MAX_MATERIAL_COUNT;
        std::vector<Wrappers::PoolSizeRatio> sizes =
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES_PER_MATERIAL }
        };

        m_DescriptorAllocator = std::make_unique<Wrappers::DescriptorAllocator>(context.GetDevice(), materialCount, sizes);

        Wrappers::DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_Layout = std::make_unique<Wrappers::DescriptorSetLayout>(context.GetDevice(), builder, VK_SHADER_STAGE_FRAGMENT_BIT);
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
        size_t createdResourceCount = m_MaterialUniforms.size();
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
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(MaterialUniform);
        MaterialUniform materialData;
        materialData.transparency = m_Materials[index].transparency;
        Wrappers::Buffer stagingBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        stagingBuffer.CopyDataToBufferMemory(device, &materialData, static_cast<size_t>(size));

        device.CopyBufferToBuffer(stagingBuffer.GetNativeBuffer(), m_MaterialUniforms[index].GetNativeBuffer(), size);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_MaterialUniforms[index].GetNativeBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = m_MaterialUniforms[index].GetBufferSize();

        stagingBuffer.DestroyBuffer(device);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureContainer.GetImage(device, m_Materials[index].baseColor).GetNativeView();
        imageInfo.sampler = textureContainer.GetSampler(device, SamplerType::Linear).GetNativeSampler();

        std::vector<Wrappers::DescriptorWrites> writes;
        writes.resize(2);

        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        m_DescriptorSets[index].Update(context.GetDevice(), writes);
    }

    void MaterialContainer::CreateMaterialResources(MaterialData& data, const VulkanContext& context, const TextureContainer& textureContainer)
    {
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(MaterialUniform);
        MaterialUniform materialData;
        materialData.transparency = data.transparency;
        Wrappers::Buffer stagingBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        stagingBuffer.CopyDataToBufferMemory(device, &materialData, static_cast<size_t>(size));

        Wrappers::Buffer materialUniform(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        device.CopyBufferToBuffer(stagingBuffer.GetNativeBuffer(), materialUniform.GetNativeBuffer(), size);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = materialUniform.GetNativeBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = materialUniform.GetBufferSize();

        stagingBuffer.DestroyBuffer(device);
        m_MaterialUniforms.push_back(std::move(materialUniform));

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureContainer.GetImage(device, data.baseColor).GetNativeView();
        imageInfo.sampler = textureContainer.GetSampler(device, SamplerType::Linear).GetNativeSampler();

        std::vector<Wrappers::DescriptorWrites> writes;
        writes.resize(2);

        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        Wrappers::DescriptorSet descriptorSet(context.GetDevice(), *m_DescriptorAllocator, *m_Layout);
        descriptorSet.Update(context.GetDevice(), writes);

        m_DescriptorSets.push_back(std::move(descriptorSet));

        data.isDirty = false;
    }

    void MaterialContainer::Cleanup(const VulkanContext& context)
    {
        for (auto& set : m_DescriptorSets)
        {
            set.Cleanup(context.GetDevice(), *m_DescriptorAllocator);
        }
        m_DescriptorSets.clear();
        for (auto& uniform : m_MaterialUniforms)
        {
            uniform.DestroyBuffer(context.GetDevice());
        }
        m_MaterialUniforms.clear();
        m_Layout->Cleanup(context.GetDevice());
        m_DescriptorAllocator->Cleanup(context.GetDevice());
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

    const Wrappers::DescriptorSetLayout& MaterialContainer::GetDescriptorSetLayout() const
    {
        return *m_Layout;
    }

    const Wrappers::DescriptorSet& MaterialContainer::GetDescriptorSet(size_t index) const
    {
        return m_DescriptorSets[index];
    }

    Wrappers::DescriptorSet& MaterialContainer::GetDescriptorSet(size_t index)
    {
        return m_DescriptorSets[index];
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



