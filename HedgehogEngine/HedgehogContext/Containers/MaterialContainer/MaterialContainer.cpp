#include "MaterialContainer.hpp"
#include "MaterialData.hpp"

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

#include "ContentLoader/Common/CommonFunctions.hpp"
#include "ContentLoader/MaterialLoader/MaterialSerializer.hpp"
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

		mDescriptorAllocator = std::make_unique<Wrappers::DescriptorAllocator>(context.GetDevice(), materialCount, sizes);

		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		mLayout = std::make_unique<Wrappers::DescriptorSetLayout>(context.GetDevice(), builder, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	MaterialContainer::~MaterialContainer()
	{
	}

	ContentLoader::MaterialData ToContentLoaderData(const MaterialData& data)
	{
		ContentLoader::MaterialData result;

		result.baseColor = data.baseColor;
		result.isDirty = data.isDirty;
		result.path = data.path;
		result.transparency = data.transparency;
		switch (data.type)
		{
		case MaterialType::Cutoff:
			result.type = ContentLoader::MaterialType::Cutoff;
			break;
		case MaterialType::Opaque:
			result.type = ContentLoader::MaterialType::Opaque;
			break;
		case MaterialType::Transparent:
			result.type = ContentLoader::MaterialType::Transparent;
		default:
			break;
		}

		return result;
	}

	MaterialData ToContainerData(const ContentLoader::MaterialData& data)
	{
		MaterialData result;

		result.baseColor = data.baseColor;
		result.isDirty = data.isDirty;
		result.path = data.path;
		result.transparency = data.transparency;
		switch (data.type)
		{
		case ContentLoader::MaterialType::Cutoff:
			result.type = MaterialType::Cutoff;
			break;
		case ContentLoader::MaterialType::Opaque:
			result.type = MaterialType::Opaque;
			break;
		case ContentLoader::MaterialType::Transparent:
			result.type = MaterialType::Transparent;
		default:
			break;
		}

		return result;
	}

	void MaterialContainer::Update(const Scene::Scene& scene)
	{
		auto materialsInScene = scene.GetMaterials();
		for (size_t i = mMaterials.size(); i < materialsInScene.size(); ++i)
		{
			ContentLoader::MaterialData data;
			ContentLoader::MaterialSerializer::Deserialize(data, ContentLoader::GetAssetsDirectory() + materialsInScene[i]);

			MaterialData materialData = ToContainerData(data);

			mMaterials.push_back(materialData);
		}
	}

	void MaterialContainer::UpdateResources(const VulkanContext& context, const TextureContainer& textureContainer)
	{
		size_t createdResourceCount = mMaterialUniforms.size();
		for (size_t i = 0; i < createdResourceCount; ++i)
		{
			if (mMaterials[i].isDirty)
			{
				UpdateMaterialByIndex(i, context, textureContainer);
				mMaterials[i].isDirty = false;
			}
		}
		for (size_t i = createdResourceCount; i < mMaterials.size(); ++i)
		{
			CreateMaterialResources(mMaterials[i], context, textureContainer);
		}
	}

	void MaterialContainer::SetMaterialDirty(size_t index)
	{
		mMaterials[index].isDirty = true;
	}

	void MaterialContainer::UpdateMaterialByIndex(size_t index, const VulkanContext& context, const TextureContainer& textureContainer)
	{
		auto& device = context.GetDevice();
		VkDeviceSize size = sizeof(MaterialUniform);
		MaterialUniform materialData;
		materialData.transparency = mMaterials[index].transparency;
		Wrappers::Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
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

		mDescriptorSets[index].Update(context.GetDevice(), writes);
	}

	void MaterialContainer::CreateMaterialResources(MaterialData& data, const VulkanContext& context, const TextureContainer& textureContainer)
	{
		auto& device = context.GetDevice();
		VkDeviceSize size = sizeof(MaterialUniform);
		MaterialUniform materialData;
		materialData.transparency = data.transparency;
		Wrappers::Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		staginBuffer.CopyDataToBufferMemory(device, &materialData, (size_t)size);

		Wrappers::Buffer materialUniform(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
		device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), materialUniform.GetNativeBuffer(), size);

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = materialUniform.GetNativeBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = materialUniform.GetBufferSize();

		staginBuffer.DestroyBuffer(device);
		mMaterialUniforms.push_back(std::move(materialUniform));

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

		Wrappers::DescriptorSet descriptorSet(context.GetDevice(), *mDescriptorAllocator, *mLayout);
		descriptorSet.Update(context.GetDevice(), writes);

		mDescriptorSets.push_back(std::move(descriptorSet));

		data.isDirty = false;
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
			ContentLoader::MaterialData newData;
			newData.type = ContentLoader::MaterialType::Opaque;
			newData.transparency = 1.0f;
			newData.baseColor = mDefaultCellTexture;
			newData.path = ContentLoader::GetAssetRelativetlyPath(path);

			ContentLoader::MaterialSerializer::Serialize(newData, path);

			mMaterials.push_back(ToContainerData(newData));
		}

	}

	void MaterialContainer::SaveMaterial(size_t index)
	{
		auto newData = ToContentLoaderData(mMaterials[index]);

		ContentLoader::MaterialSerializer::Serialize(newData, ContentLoader::GetAssetsDirectory() + newData.path);
	}

	void MaterialContainer::LoadBaseTexture(size_t index, const VulkanContext& context, const TextureContainer& textureContainer)
	{
		auto texturePath = DialogueWindows::TextureOpenDialogue();
		if (texturePath == nullptr)
			return;
		mMaterials[index].baseColor = ContentLoader::GetAssetRelativetlyPath(texturePath);

		UpdateMaterialByIndex(index, context, textureContainer);
	}

	const Wrappers::DescriptorSetLayout& MaterialContainer::GetDescriptorSetLayout() const
	{
		return *mLayout;
	}

	const Wrappers::DescriptorSet& MaterialContainer::GetDescriptorSet(size_t index) const
	{
		return mDescriptorSets[index];
	}

	Wrappers::DescriptorSet& MaterialContainer::GetDescriptorSet(size_t index)
	{
		return mDescriptorSets[index];
	}

	MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index)
	{
		return mMaterials[index];
	}

	const MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index) const
	{
		return mMaterials[index];
	}

}




