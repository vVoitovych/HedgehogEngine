#include "MaterialContainer.hpp"
#include "MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "Renderer/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"
#include "Renderer/Wrappeers/Resources/Sampler/Sampler.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Containers/TextureContainer.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#include "Scene/Scene.hpp"

namespace Renderer
{
	MaterialContainer::MaterialContainer(const VulkanContext& context)
	{
		uint32_t materialCount = MAX_MATERIAL_COUNT;
		std::vector<PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES_PER_MATERIAL }
		};

		mDescriptorAllocator = std::make_unique<DescriptorAllocator>(context.GetDevice(), materialCount, sizes);

		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		mLayout = std::make_unique<DescriptorSetLayout>(context.GetDevice(), builder, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	MaterialContainer::~MaterialContainer()
	{
	}

	void MaterialContainer::Update(const Scene::Scene& scene)
	{
		auto materialsInScene = scene.GetMaterials();
		for (size_t i = mMaterials.size(); i < materialsInScene.size(); ++i)
		{
			MaterialData data;
			MaterialSerializer::Deserialize(data, ContentLoader::GetAssetsDirectory() + materialsInScene[i]);
			mMaterials.push_back(data);
		}
		UpdateIndicies(); // TODO: optimize it
	}

	void MaterialContainer::UpdateResources(const VulkanContext& context, const TextureContainer& textureContainer)
	{
		for (size_t i = mMaterialUniforms.size(); i < mMaterials.size(); ++i)
		{
			auto& device = context.GetDevice();
			VkDeviceSize size = sizeof(MaterialUniform);
			MaterialUniform materialData;
			materialData.transparency = mMaterials[i].transparency;
			Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			staginBuffer.CopyDataToBufferMemory(device, &materialData, (size_t)size);

			Buffer materialUniform(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), materialUniform.GetNativeBuffer(), size);

			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = materialUniform.GetNativeBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = materialUniform.GetBufferSize();

			staginBuffer.DestroyBuffer(device);
			mMaterialUniforms.push_back(std::move(materialUniform));

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureContainer.GetImage(device, mMaterials[i].baseColor).GetNativeView();
			imageInfo.sampler = textureContainer.GetSampler(device, SamplerType::Linear).GetNativeSampler();

			std::vector<DescriptorWrites> writes;
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

			DescriptorSet descriptorSet(context.GetDevice(), *mDescriptorAllocator, *mLayout);
			descriptorSet.Update(context.GetDevice(), writes);

			mDescriptorSets.push_back(std::move(descriptorSet));
		}
	}

	void MaterialContainer::UpdateMaterialByIndex(size_t index, const VulkanContext& context, const TextureContainer& textureContainer)
	{
		auto& device = context.GetDevice();
		VkDeviceSize size = sizeof(MaterialUniform);
		MaterialUniform materialData;
		materialData.transparency = mMaterials[index].transparency;
		Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		staginBuffer.CopyDataToBufferMemory(device, &materialData, (size_t)size);

		device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), mMaterialUniforms[index].GetNativeBuffer(), size);

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = mMaterialUniforms[index].GetNativeBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = mMaterialUniforms[index].GetBufferSize();

		staginBuffer.DestroyBuffer(device);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureContainer.GetImage(device, mMaterials[index].baseColor).GetNativeView();
		imageInfo.sampler = textureContainer.GetSampler(device, SamplerType::Linear).GetNativeSampler();

		std::vector<DescriptorWrites> writes;
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

		mDescriptorSets[index].Update(context.GetDevice(), writes);
	}

	void MaterialContainer::Cleanup(const VulkanContext& context)
	{
		for (auto& set : mDescriptorSets)
		{
			set.Cleanup(context.GetDevice(), *mDescriptorAllocator);
		}
		mDescriptorSets.clear();
		for (auto& uniform : mMaterialUniforms)
		{
			uniform.DestroyBuffer(context.GetDevice());
		}
		mMaterialUniforms.clear();
		mLayout->Cleanup(context.GetDevice());
		mDescriptorAllocator->Cleanup(context.GetDevice());
	}

	void MaterialContainer::ClearMaterials()
	{
		mMaterials.clear();
	}

	void MaterialContainer::CreateNewMaterial()
	{
		auto path = DialogueWindows::MaterialCreationDialogue();
		if (path != nullptr)
		{
			MaterialData newData;
			newData.type = MaterialType::Opaque;
			newData.transparency = 1.0f;
			newData.baseColor = mDefaultCellTexture;
			newData.path = ContentLoader::GetAssetRelativetlyPath(path);

			MaterialSerializer::Serialize(newData, path);

			mMaterials.push_back(newData);
		}

	}

	void MaterialContainer::UpdateIndicies()
	{
		mOpaqueMaterials.clear();
		mTransparentMaterials.clear();
		mCutoffMaterials.clear();

		for (size_t i = 0; i < mMaterials.size(); ++i)
		{
			switch (mMaterials[i].type)
			{
			case MaterialType::Opaque: mOpaqueMaterials.push_back(i);
				break;
			case MaterialType::Transparency: mTransparentMaterials.push_back(i);
				break;
			case MaterialType::Cutoff: mCutoffMaterials.push_back(i);
				break;
			}
		}
	}

	size_t MaterialContainer::GetOpaqueMaterialsCount() const
	{
		return mOpaqueMaterials.size();
	}

	size_t MaterialContainer::GetTransparentMaterialsCount() const
	{
		return mTransparentMaterials.size();
	}

	size_t MaterialContainer::GetCutoffMaterialsCount() const
	{
		return mCutoffMaterials.size();
	}

	const std::vector<size_t>& MaterialContainer::GetOpequeMaterials() const
	{
		return mOpaqueMaterials;
	}

	const std::vector<size_t>& MaterialContainer::GetTransparentMaterials() const
	{
		return mTransparentMaterials;
	}

	const std::vector<size_t>& MaterialContainer::GetCutoffMaterials() const
	{
		return mCutoffMaterials;
	}

	const DescriptorSetLayout& MaterialContainer::GetDescriptorSetLayout() const
	{
		return *mLayout;
	}

	const DescriptorSet& MaterialContainer::GetDescriptorSet(size_t index) const
	{
		return mDescriptorSets[index];
	}

	MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index)
	{
		return mMaterials[index];
	}

}




